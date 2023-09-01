// Copyright 2023 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTServer.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/NSData+GNCWebSafeBase64.h"
#import "GoogleToolboxForMac/GTMLogger.h"

NS_ASSUME_NONNULL_BEGIN

static char *const kGNCBLEGATTServerQueueLabel = "com.nearby.GNCBLEGATTServer";

@interface GNCBLEGATTServer () <GNCPeripheralManagerDelegate>
@end

@implementation GNCBLEGATTServer {
  dispatch_queue_t _queue;
  id<GNCPeripheralManager> _peripheralManager;

  // A map of service UUIDs to the service. This is used to keep track of all services and
  // characteristics that have been requested to be added to the GATT database. This may not match
  // the "real" services that remote devices can see.
  NSMutableDictionary<CBUUID *, CBMutableService *> *_services;

  // A list of characteristics that will be added to the GATT database once the peripheral
  // transitions into a valid state. If a characteristic is in this list it means that a
  // CoreBluetooth request to add to the characteristic to the GATT database has not yet been made.
  NSMutableDictionary<CBUUID *, NSMutableArray<CBMutableCharacteristic *> *>
      *_pendingCharacteristics;

  // This keeps track of a characteristic's dynamic read value. This is the value returned when a
  // remote device attempts to read the characteristic.
  NSMutableDictionary<CBUUID *, NSMutableDictionary<CBUUID *, NSData *> *> *_characteristicValues;

  // The data that should be advertised. This value is cached until the caller explicitly stops
  // advertising, because we will do our best to restart the advertisement if BT turns off then back
  // on.
  NSDictionary<NSString *, id> *_advertisementData;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _queue = dispatch_queue_create(kGNCBLEGATTServerQueueLabel, DISPATCH_QUEUE_SERIAL);
    _peripheralManager = [[CBPeripheralManager alloc] initWithDelegate:nil queue:_queue];
    // Set for @c GNCPeripheralManager to be able to forward callbacks.
    _peripheralManager.peripheralDelegate = self;
    _services = [[NSMutableDictionary alloc] init];
    _pendingCharacteristics = [[NSMutableDictionary alloc] init];
    _characteristicValues = [[NSMutableDictionary alloc] init];
    _advertisementData = nil;
  }
  return self;
}

// This is private and should only be used for tests. The provided peripheral manager must call
// delegate methods on the main queue.
- (instancetype)initWithPeripheralManager:(nullable id<GNCPeripheralManager>)peripheralManager {
  self = [super init];
  if (self) {
    _queue = dispatch_get_main_queue();
    _peripheralManager = peripheralManager;
    // Set for @c GNCPeripheralManager to be able to forward callbacks.
    _peripheralManager.peripheralDelegate = self;
    _services = [[NSMutableDictionary alloc] init];
    _pendingCharacteristics = [[NSMutableDictionary alloc] init];
    _characteristicValues = [[NSMutableDictionary alloc] init];
    _advertisementData = nil;
  }
  return self;
}

- (void)createCharacteristicWithServiceID:(CBUUID *)serviceUUID
                       characteristicUUID:(CBUUID *)characteristicUUID
                              permissions:(CBAttributePermissions)permissions
                               properties:(CBCharacteristicProperties)properties
                        completionHandler:
                            (nullable GNCCreateCharacteristicCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    // Check if the characteristic exists in the pending additions or existing services.
    for (CBCharacteristic *c in _pendingCharacteristics[serviceUUID]) {
      if ([c.UUID isEqual:characteristicUUID]) {
        if (completionHandler) {
          completionHandler(nil, [NSError errorWithDomain:GNCBLEErrorDomain
                                                     code:GNCBLEErrorDuplicateCharacteristic
                                                 userInfo:nil]);
        }
        return;
      }
    }
    for (CBCharacteristic *c in _services[serviceUUID].characteristics) {
      if ([c.UUID isEqual:characteristicUUID]) {
        if (completionHandler) {
          completionHandler(nil, [NSError errorWithDomain:GNCBLEErrorDomain
                                                     code:GNCBLEErrorDuplicateCharacteristic
                                                 userInfo:nil]);
        }
        return;
      }
    }

    // Create a characteristic with the specified permissions and properties.
    CBMutableCharacteristic *characteristic =
        [[CBMutableCharacteristic alloc] initWithType:characteristicUUID
                                           properties:properties
                                                value:nil
                                          permissions:permissions];
    // Track characteristics that need to be added to the GATT database. These will either be added
    // immediately or as soon as the peripheral manager is in a valid state.
    if (!_pendingCharacteristics[serviceUUID]) {
      _pendingCharacteristics[serviceUUID] = [[NSMutableArray alloc] init];
    }
    [_pendingCharacteristics[serviceUUID] addObject:characteristic];

    GNCBLEGATTCharacteristic *gncCharacteristic =
        [[GNCBLEGATTCharacteristic alloc] initWithUUID:characteristicUUID
                                           serviceUUID:serviceUUID
                                           permissions:permissions
                                            properties:properties];
    [self internalAddPendingServicesIfPoweredOn];
    if (completionHandler) {
      completionHandler(gncCharacteristic, nil);
    }
  });
}

- (void)updateCharacteristic:(GNCBLEGATTCharacteristic *)characteristic
                       value:(nullable NSData *)value
           completionHandler:(nullable GNCUpdateCharacteristicCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    // Keep track of the characteristics value, because we will need to retreive it during a read
    // request.
    if (_characteristicValues[characteristic.serviceUUID] == nil) {
      _characteristicValues[characteristic.serviceUUID] = [[NSMutableDictionary alloc] init];
    }
    _characteristicValues[characteristic.serviceUUID][characteristic.characteristicUUID] = value;
    if (completionHandler) {
      completionHandler(nil);
    }
  });
}

- (void)stop {
  dispatch_async(_queue, ^{
    // Note: Do not clear/stop advertisements here, since there is a separate method for that.
    [_peripheralManager removeAllServices];
    [_services removeAllObjects];
    [_pendingCharacteristics removeAllObjects];
    [_characteristicValues removeAllObjects];
  });
}

- (void)startAdvertisingData:(NSDictionary<CBUUID *, NSData *> *)serviceData
           completionHandler:(nullable GNCStartAdvertisingCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    // We can only handle advertising a single service data item, so return early if there is more
    // than one service incuded.
    if (serviceData.count != 1) {
      if (completionHandler) {
        completionHandler([NSError
            errorWithDomain:GNCBLEErrorDomain
                       code:GNCBLEErrorInvalidServiceData
                   userInfo:@{
                     NSLocalizedDescriptionKey :
                         @"Failed to start advertising, because Nearby for Apple platforms only "
                         @"supports a single service data item."
                   }]);
      }
      return;
    }

    // If we have advertisement data set, that means we are already advertising and should return
    // early. Advertising must be stopped before being started again.
    if (_advertisementData) {
      if (completionHandler) {
        completionHandler([NSError errorWithDomain:GNCBLEErrorDomain
                                              code:GNCBLEErrorAlreadyAdvertising
                                          userInfo:nil]);
      }
      return;
    }

    // Apple doesn't support setting service data, so we must convert it to a "local name". We do
    // this by assuming there will only ever be one service and then base64 encoding its associated
    // data. Other platforms are aware of this behavior and always check the local name if service
    // data is unavailable.
    CBUUID *serviceUUID = [serviceData.allKeys objectAtIndex:0];
    NSData *value = [serviceData objectForKey:serviceUUID];
    NSString *encoded = [value webSafeBase64EncodedString];

    // Apple only "guarantees" (best effort) 28 bytes of advertisement data. Base64 encoding
    // increases the size of the original data so we must truncate it to ensure it still meets the
    // 28 byte limit. Since we have a 2-byte service UUID and the header for local name and service
    // UUID is 2 bytes each, that leaves us with a maximum of 22 bytes for the local name. However,
    // it seems in practice we can only reliably advertise a 20-byte local name on iOS.
    if (encoded.length > 20) {
      encoded = [encoded substringToIndex:20];
    }

    _advertisementData = @{
      CBAdvertisementDataLocalNameKey : encoded,
      CBAdvertisementDataServiceUUIDsKey : @[ serviceUUID ]
    };

    [self internalStartAdvertisingIfPoweredOn];
    if (completionHandler) {
      completionHandler(nil);
    }
  });
}

- (void)stopAdvertisingWithCompletionHandler:
    (nullable GNCStopAdvertisingCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    _advertisementData = nil;
    [_peripheralManager stopAdvertising];
    if (completionHandler) {
      completionHandler(nil);
    }
  });
}

#pragma mark - Internal

- (void)internalAddPendingServicesIfPoweredOn {
  dispatch_assert_queue(_queue);
  // Services can only be added while bluetooth is on or off. Since this method will be called
  // anytime the peripheral manager's state changes and turning bluetooth on/off does not reset
  // the GATT database, we can add all pending services when we transition to the on or off state.
  // However, other state transitions may behave differently, so we may need to modify this
  // functionality in this future to re-add services if they disapear.
  if (_peripheralManager.state == CBManagerStatePoweredOn ||
      _peripheralManager.state == CBManagerStatePoweredOff) {
    for (CBUUID *serviceUUID in _pendingCharacteristics.allKeys) {
      // If a service with the specified UUID exists, we are modifying it, which requires us to
      // remove the service and then re-add it after the new characteristic has been added.
      // Otherwise, create the service for the specified UUID and keep track of it.
      CBMutableService *service = _services[serviceUUID];
      if (service != nil) {
        [_peripheralManager removeService:service];
      } else {
        service = [[CBMutableService alloc] initWithType:serviceUUID primary:YES];
        _services[serviceUUID] = service;
      }

      // Add the pending characteristics to the current service's list of characteristics.
      NSMutableArray<CBMutableCharacteristic *> *characteristics =
          [service.characteristics mutableCopy];
      if (characteristics == nil) {
        characteristics = [[NSMutableArray alloc] init];
      }
      // This makes the assumption that _pendingCharacteristics does not contain any duplicate
      // characteristics within itself or the current service characteristics. This is only
      // enforced on characteristic creation.
      [characteristics addObjectsFromArray:_pendingCharacteristics[serviceUUID]];
      service.characteristics = characteristics;

      // Publish the service.
      [_peripheralManager addService:service];
    }
    // Clean up pending characteristics so we don't try to re-add them any time the power state
    // changes.
    [_pendingCharacteristics removeAllObjects];
  }
}

- (void)internalStartAdvertisingIfPoweredOn {
  dispatch_assert_queue(_queue);
  // Advertising can only be done when powered on and must be restarted if bluetooth is turned off
  // then back on. This will be called anytime the peripheral manager's state changes, so
  // @c startAdvertising: will be called anytime state transitions back to powered on.
  if (_peripheralManager.state == CBManagerStatePoweredOn && _advertisementData != nil) {
    // Stop advertising just in case something outside of this class is advertising (like weave).
    [_peripheralManager stopAdvertising];
    [_peripheralManager startAdvertising:_advertisementData];
  }
}

#pragma mark - GNCPeripheralManagerDelegate

- (void)gnc_peripheralManagerDidUpdateState:(id<GNCPeripheralManager>)peripheral {
  dispatch_assert_queue(_queue);
  [self internalAddPendingServicesIfPoweredOn];
  [self internalStartAdvertisingIfPoweredOn];
}

- (void)gnc_peripheralManagerDidStartAdvertising:(id<GNCPeripheralManager>)peripheral
                                           error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  if (error) {
    GTMLoggerError(@"Failed to start advertising: %@", error);
  }
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
                didAddService:(CBService *)service
                        error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  if (error) {
    GTMLoggerError(@"Failed to add service %@: %@", service, error);
  }
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
        didReceiveReadRequest:(CBATTRequest *)request {
  dispatch_assert_queue(_queue);
  NSData *value =
      _characteristicValues[request.characteristic.service.UUID][request.characteristic.UUID];
  if (!value) {
    [_peripheralManager respondToRequest:request withResult:CBATTErrorAttributeNotFound];
    return;
  }
  request.value = value;
  [_peripheralManager respondToRequest:request withResult:CBATTErrorSuccess];
}

#pragma mark - CBPeripheralManagerDelegate

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
  dispatch_async(_queue, ^{
    [self gnc_peripheralManagerDidUpdateState:peripheral];
  });
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral
                                       error:(nullable NSError *)error {
  dispatch_async(_queue, ^{
    [self gnc_peripheralManagerDidStartAdvertising:peripheral error:error];
  });
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
            didAddService:(CBService *)service
                    error:(nullable NSError *)error {
  dispatch_async(_queue, ^{
    [self gnc_peripheralManager:peripheral didAddService:service error:error];
  });
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didReceiveReadRequest:(CBATTRequest *)request {
  dispatch_async(_queue, ^{
    [self gnc_peripheralManager:peripheral didReceiveReadRequest:request];
  });
}

@end

NS_ASSUME_NONNULL_END
