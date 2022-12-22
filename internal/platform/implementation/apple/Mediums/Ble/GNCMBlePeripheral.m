// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBlePeripheral.h"

#import <CoreBluetooth/CoreBluetooth.h>

#import "internal/platform/implementation/apple/GNCUtils.h"
#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleConnection.h"
#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleUtils.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralManager.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralServiceManager.h"
#import "internal/platform/implementation/apple/Mediums/GNCMConnection.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, GNCMPeripheralState) {
  GNCMPeripheralStateStopped,
  GNCMPeripheralStateAdvertising,
};

@interface GNCMBlePeripheral () <CBPeripheralManagerDelegate>
@end

@implementation GNCMBlePeripheral {
  /** Service UUID for advertisement. */
  CBMutableService *_advertisementService;
  /** GATT service for GATT connection. */
  CBMutableService *_GATTService;
  /** GATT characteristics for GATT connection. */
  NSMutableArray<CBCharacteristic *> *_gattCharacteristics;
  /** CBUUID characteristic to NSData value dictionary for GATT connection. */
  NSMutableDictionary<CBUUID *, NSData *> *_gattCharacteristicValues;
  /** Data to be advertised. */
  NSData *_advertisementData;
  /** Peripheral manager used to advertise or connect to peripherals. */
  CBPeripheralManager *_peripheralManager;
  /** Serial background queue for |peripheralManager|. */
  dispatch_queue_t _selfQueue;
  /** Peripheral state for stop or advertising. */
  GNCMPeripheralState _state;
  /** Peripheral manager used for socket connection based on weave protocol. */
  GNSPeripheralManager *_socketPeripheralManager;
  /** Peripheral service manager used to manage one BLE service. */
  GNSPeripheralServiceManager *_socketPeripheralServiceManager;
  /** Client callback queue. If client doesn't assign it, then use main queue. */
  dispatch_queue_t _clientCallbackQueue;
  /** Internal async priority queue. */
  dispatch_queue_t _internalCallbackQueue;
  /** Flag to disable callback for dealloc. */
  BOOL _callbacksEnabled;
}

- (instancetype)init {
  if (self = [super init]) {
    // To make this class thread-safe, use a serial queue for all state changes, and have Core
    // Bluetooth also use this queue.
    _selfQueue = dispatch_queue_create("GNCPeripheralManagerQueue", DISPATCH_QUEUE_SERIAL);

    _state = GNCMPeripheralStateStopped;
  }
  return self;
}

- (void)dealloc {
  // These calls must be made on |selfQueue|. Can't capture |self| in an async block, so must use
  // dispatch_sync. This means delloc must be called from an external queue, which means |self|
  // must never be captured by any escaping block used in this class.
  dispatch_sync(_selfQueue, ^{
    [self stopAdvertisingInternal];

    _callbacksEnabled = NO;
  });
}

- (void)addCBServiceWithUUID:(CBUUID *)serviceUUID {
  if (!_GATTService) {
    // If it has been called, then don't do it again. Initialize one time.
    _GATTService = [[CBMutableService alloc] initWithType:serviceUUID primary:YES];
    _gattCharacteristics = [[NSMutableArray alloc] init];
    _gattCharacteristicValues = [[NSMutableDictionary alloc] init];
  }
}

- (void)addCharacteristic:(CBCharacteristic *)characteristic {
  if (_gattCharacteristics) {
    [_gattCharacteristics addObject:characteristic];
  }
  if (_gattCharacteristicValues) {
    [_gattCharacteristicValues setObject:[[NSData alloc] init] forKey:characteristic.UUID];
  }
}

- (void)updateValue:(NSData *)value forCharacteristic:(CBUUID *)characteristicUUID {
  if ([_gattCharacteristicValues objectForKey:characteristicUUID]) {
    [_gattCharacteristicValues setObject:value forKey:characteristicUUID];
  }
}

- (void)stopGATTService {
  if (!_GATTService) return;
  dispatch_sync(_selfQueue, ^{
    [_peripheralManager removeService:_GATTService];
  });
}

- (BOOL)startAdvertisingWithServiceUUID:(NSString *)serviceUUID
                      advertisementData:(NSData *)advertisementData
               endpointConnectedHandler:(GNCMConnectionHandler)endpointConnectedHandler
                          callbackQueue:(nullable dispatch_queue_t)callbackQueue {
  NSLog(@"[NEARBY] Client rquests startAdvertising");
  // The client may be using the callback queue for other purposes, so wrap it with a private
  // queue to know with certainty when all callbacks are done.
  _clientCallbackQueue = callbackQueue ?: dispatch_get_main_queue();
  _internalCallbackQueue =
      dispatch_queue_create("GNCMBlePeripheralCallbackQueue", DISPATCH_QUEUE_PRIORITY_DEFAULT);
  _callbacksEnabled = YES;

  _advertisementService = [[CBMutableService alloc] initWithType:[CBUUID UUIDWithString:serviceUUID]
                                                         primary:YES];
  _advertisementData = [advertisementData copy];

  // To make this class thread-safe, use a serial queue for all state changes, and have Core
  // Bluetooth also use this queue.
  _selfQueue = dispatch_queue_create("GNCPeripheralManagerQueue", DISPATCH_QUEUE_SERIAL);
  __weak __typeof__(self) weakSelf = self;
  // Set up the peripheral manager for the socket. This must be done before creating the
  // peripheral manager for the advertisement data because it's started/stopped in the
  // -peripheralManagerDidUpdateState: callback.
  _socketPeripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:_advertisementService.UUID
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        // Call the connection handler when the socket has connected or fails to connect.
        GNCMWaitForConnection(socket, ^(BOOL didConnect) {
          [weakSelf establishConnectionWithSocket:socket
                                       didConnect:didConnect
                         endpointConnectedHandler:endpointConnectedHandler];
        });
        return YES;
      }
                          queue:_selfQueue];
  _socketPeripheralManager = [[GNSPeripheralManager alloc] initWithAdvertisedName:nil
                                                                restoreIdentifier:nil
                                                                            queue:_selfQueue];
  [_socketPeripheralManager addPeripheralServiceManager:_socketPeripheralServiceManager
                              bleServiceAddedCompletion:^(NSError *error) {
                                NSLog(@"Failed to add service");
                              }];

  // Set up the peripheral manager for the advertisement data.
  _peripheralManager = [[CBPeripheralManager alloc]
      initWithDelegate:self
                 queue:_selfQueue
               options:@{CBPeripheralManagerOptionShowPowerAlertKey : @NO}];

  if (_GATTService) {
    if (_gattCharacteristics && _gattCharacteristics.count > 0) {
      _GATTService.characteristics = _gattCharacteristics;
    }
  }
  _state = GNCMPeripheralStateAdvertising;
  return YES;
}

#pragma mark CBPeripheralManagerDelegate

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
  NSLog(@"[NEARBY] peripheralManagerDidUpdateState %li", (long)peripheral.state);
  if (peripheral.state == CBManagerStatePoweredOn && !peripheral.isAdvertising &&
      _state == GNCMPeripheralStateAdvertising) {
    NSLog(@"[NEARBY] CBPeripheralManager powered on; starting advertising");
    [_socketPeripheralManager start];
    [self startAdvertisingInternal];
  } else {
    NSLog(@"[NEARBY] CBPeripheralManager not powered on; stopping advertising");
    [self stopAdvertisingInternal];
  }
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral
                                       error:(nullable NSError *)error {
  if (error) {
    NSLog(@"[NEARBY] Error starting advertising: %@,", [error localizedDescription]);
    return;
  }
  if (_peripheralManager.state != CBManagerStatePoweredOn) {
    NSLog(@"[NEARBY] Error starting advertising: peripheral manager not on!");
    return;
  }

  NSLog(@"[NEARBY] Peripheral manager started advertising");
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didReceiveReadRequest:(CBATTRequest *)request {
  NSLog(@"[NEARBY] peripheralManager:didReceiveReadRequest");
  // This is called when a central asks to read a characteristic's value.
  CBATTError error = CBATTErrorAttributeNotFound;
  NSData *value = _gattCharacteristicValues[request.characteristic.UUID];
  if (value != nil && value.length > 0) {
    if (request.offset > value.length) {
      error = CBATTErrorInvalidOffset;
    } else {
      // Reply with the advertisement data.
      NSRange rangeFromOffset = NSMakeRange(request.offset, value.length - request.offset);
      request.value = [value subdataWithRange:rangeFromOffset];
      error = CBATTErrorSuccess;
    }
  }
  [_peripheralManager respondToRequest:request withResult:error];
}

#pragma mark Private

/** Signals the peripheral manager to start advertising. Must be called on _selfQueue */
- (void)startAdvertisingInternal {
  if (![_peripheralManager isAdvertising]) {
    NSLog(@"[NEARBY] startAdvertisingInternal");

    if (_GATTService) {
      [_peripheralManager addService:_GATTService];
    }
    [_peripheralManager startAdvertising:@{
      CBAdvertisementDataServiceUUIDsKey : @[ _advertisementService.UUID ],
      CBAdvertisementDataLocalNameKey : _advertisementData
    }];
  }
}

/** Signals the peripheral manager to stop advertising. Must be called on _selfQueue */
- (void)stopAdvertisingInternal {
  if ([_peripheralManager isAdvertising]) {
    NSLog(@"[NEARBY] stopAdvertisingInternal");
    _state = GNCMPeripheralStateStopped;

    if (_GATTService) {
      [_peripheralManager removeService:_GATTService];
    }
    [_peripheralManager stopAdvertising];
  }
}

/**
 * Connects with socket and callback the |GNCMBleConnection| is established or nil if it is not.
 */
- (void)establishConnectionWithSocket:(GNSSocket *)socket
                           didConnect:(BOOL)didConnect
             endpointConnectedHandler:(GNCMConnectionHandler)endpointConnectedHandler {
  if (!_callbacksEnabled) {
    return;
  }

  [self callbackAsync:^{
    if (!didConnect) {
      NSLog(@"[NEARBY] Peripheral failed to create BLE socket");
      endpointConnectedHandler(nil);
    } else {
      GNCMBleConnection *connection = [GNCMBleConnection connectionWithSocket:socket
                                                                    serviceID:nil
                                                          expectedIntroPacket:YES
                                                                callbackQueue:_clientCallbackQueue];
      connection.connectionHandlers = endpointConnectedHandler(connection);
    }
  }];
}

/**
 * Calls the specified block on the callback queue, preventing it from being dispatched to the
 * client callback queue when callbacks are disabled. And without capturing |self|, since
 * callbacks are disabled in dealloc.
 */
- (void)callbackAsync:(dispatch_block_t)block {
  dispatch_queue_t clientCallbackQueue = _clientCallbackQueue;  // don't capture |self|
  dispatch_async(_internalCallbackQueue, ^{
    dispatch_sync(clientCallbackQueue, block);
  });
}

@end

NS_ASSUME_NONNULL_END
