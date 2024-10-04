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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEMedium.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTClient.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTServer.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCCentralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/NSData+GNCWebSafeBase64.h"

NS_ASSUME_NONNULL_BEGIN

static char *const kBLEMediumQueueLabel = "com.nearby.GNCBLEMedium";

static NSError *AlreadyScanningError() {
  return [NSError errorWithDomain:GNCBLEErrorDomain code:GNCBLEErrorAlreadyScanning userInfo:nil];
}

@interface GNCBLEMedium () <GNCCentralManagerDelegate, CBCentralManagerDelegate>
@end

@implementation GNCBLEMedium {
  dispatch_queue_t _queue;
  id<GNCCentralManager> _centralManager;

  // The active GATT server, or @nil if one hasn't been started yet.
  GNCBLEGATTServer *_server;

  // The service that is being actively scanned for, or @c nil if not currently scanning.
  CBUUID *_serviceUUID;

  // The handler called when an advertisement for the service represented by @c _serviceUUID has
  // been discovered. This will be called continuously, until the peripheral disappears.
  GNCAdvertisementFoundHandler _advertisementFoundHandler;

  // A peripheral to connection completion handler map. Used to track connection attempts. When a
  // connection attempt has succeeded or failed, the completion handler is called and removed from
  // the map.
  NSMutableDictionary<NSUUID *, GNCGATTConnectionCompletionHandler> *_connectionCompletionHandlers;

  // A peripheral to disconnection handler map. Used to track when a peripheral becomes
  // disconnected. Once disconnected, the completion handler is called and removed from the map.
  NSMutableDictionary<NSUUID *, GNCGATTDisconnectionHandler> *_disconnectionHandlers;
}

- (instancetype)init {
  CBCentralManager *centralManager =
      [[CBCentralManager alloc] initWithDelegate:self
                                           queue:_queue
                                         options:@{CBCentralManagerOptionShowPowerAlertKey : @NO}];
  dispatch_queue_t queue = dispatch_queue_create(kBLEMediumQueueLabel, DISPATCH_QUEUE_SERIAL);
  return [self initWithCentralManager:centralManager queue:queue];
}

// This is private and should only be used for tests. The provided central manager must call
// delegate methods on the main queue.
- (instancetype)initWithCentralManager:(id<GNCCentralManager>)centralManager
                                 queue:(nullable dispatch_queue_t)queue {
  self = [super init];
  if (self) {
    _queue = queue ?: dispatch_get_main_queue();
    _centralManager = centralManager;
    _centralManager.centralDelegate = self;
    _connectionCompletionHandlers = [NSMutableDictionary dictionary];
    _disconnectionHandlers = [NSMutableDictionary dictionary];
  }
  return self;
}

- (BOOL)supportsExtendedAdvertisements {
  // TODO(b/294736083): CoreBluetooth doesn't support actually advertising any extensions, however
  // some devices can scan for them if the feature is available. If we return @c YES from this
  // method, we would be enabling advertising extensions (which won't work), so we must return @c NO
  // until we add support for a new method to check only if extended scans are supported.
  return NO;
}

- (void)startAdvertisingData:(NSDictionary<CBUUID *, NSData *> *)serviceData
           completionHandler:(nullable GNCStartAdvertisingCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    if (!_server) {
      _server = [[GNCBLEGATTServer alloc] init];
    }
    [_server startAdvertisingData:serviceData completionHandler:completionHandler];
  });
}

- (void)stopAdvertisingWithCompletionHandler:
    (nullable GNCStopAdvertisingCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    if (!_server) {
      if (completionHandler) {
        completionHandler(nil);
      }
      return;
    }
    [_server stopAdvertisingWithCompletionHandler:completionHandler];
  });
}

- (void)startScanningForService:(CBUUID *)serviceUUID
      advertisementFoundHandler:(GNCAdvertisementFoundHandler)advertisementFoundHandler
              completionHandler:(nullable GNCStartScanningCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    if (_serviceUUID) {
      if (completionHandler) {
        completionHandler(AlreadyScanningError());
      }
      return;
    }

    _serviceUUID = serviceUUID;
    _advertisementFoundHandler = advertisementFoundHandler;

    [self internalStartScanningIfPoweredOn];
    if (completionHandler) {
      completionHandler(nil);
    }
  });
}

- (void)stopScanningWithCompletionHandler:
    (nullable GNCStopScanningCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    _serviceUUID = nil;
    _advertisementFoundHandler = nil;
    [_centralManager stopScan];
    if (completionHandler) {
      completionHandler(nil);
    }
  });
}

- (void)startGATTServerWithCompletionHandler:
    (nullable GNCGATTServerCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    if (!_server) {
      _server = [[GNCBLEGATTServer alloc] init];
    }
    if (completionHandler) {
      completionHandler(_server, nil);
    }
  });
}

- (void)connectToGATTServerForPeripheral:(id<GNCPeripheral>)remotePeripheral
                    disconnectionHandler:(nullable GNCGATTDisconnectionHandler)disconnectionHandler
                       completionHandler:
                           (nullable GNCGATTConnectionCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    _disconnectionHandlers[remotePeripheral.identifier] = disconnectionHandler;
    _connectionCompletionHandlers[remotePeripheral.identifier] = completionHandler;
    [_centralManager connectPeripheral:remotePeripheral options:@{}];
  });
}

#pragma mark - Internal

- (void)internalStartScanningIfPoweredOn {
  dispatch_assert_queue(_queue);
  // Scanning can only be done when powered on and must be restarted if bluetooth is turned off
  // then back on. This will be called anytime the central manager's state changes, so
  // @c scanForPeripheralsWithServices:options: will be called anytime state transitions back to
  // powered on.
  if (_centralManager.state == CBManagerStatePoweredOn && _serviceUUID != nil) {
    // Stop scanning just in case something outside of this class is already scanning.
    [_centralManager stopScan];
    [_centralManager
        scanForPeripheralsWithServices:@[ _serviceUUID ]
                               // Nearby relies on the existence of an advertisement for endpoint
                               // discovery/lost events, so we must set this key to keep the stream
                               // of duplicate delegate events flowing. This has adverse effect on
                               // battery life, but currently necessary.
                               options:@{CBCentralManagerScanOptionAllowDuplicatesKey : @YES}];
  }
}

- (NSDictionary<CBUUID *, NSData *> *)decodeAdvertisementData:
    (NSDictionary<NSString *, id> *)advertisementData {
  dispatch_assert_queue(_queue);
  // If service data is available, return it directly.
  NSDictionary<CBUUID *, NSData *> *serviceData =
      advertisementData[CBAdvertisementDataServiceDataKey];
  if (serviceData) {
    return serviceData;
  }

  // Apple devices don't support advertising service data, so Apple devices advertise a base64
  // encoded local name, while other devices advertise service data. Here we attempt to reconstruct
  // service data by decoding the local name. If successful, this is possibly a Nearby advertisement
  // on an Apple device.
  NSString *localName = advertisementData[CBAdvertisementDataLocalNameKey];
  if (!localName) {
    return @{};
  }
  NSData *data = [[NSData alloc] initWithWebSafeBase64EncodedString:localName];

  // A Nearby Apple advertisement should only have a single service, so simply grab the first one if
  // it exists.
  NSArray<CBUUID *> *serviceUUIDs = advertisementData[CBAdvertisementDataServiceUUIDsKey];
  CBUUID *serviceUUID = serviceUUIDs.firstObject;
  if (data && serviceUUID) {
    return @{serviceUUID : data};
  }

  return @{};
}

#pragma mark - GNCCentralManagerDelegate

- (void)gnc_centralManagerDidUpdateState:(id<GNCCentralManager>)central {
  dispatch_assert_queue(_queue);
  [self internalStartScanningIfPoweredOn];
}

- (void)gnc_centralManager:(id<GNCCentralManager>)central
     didDiscoverPeripheral:(id<GNCPeripheral>)peripheral
         advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                      RSSI:(NSNumber *)RSSI {
  dispatch_assert_queue(_queue);
  if (_advertisementFoundHandler) {
    _advertisementFoundHandler(peripheral, [self decodeAdvertisementData:advertisementData]);
  }
}

- (void)gnc_centralManager:(id<GNCCentralManager>)central
      didConnectPeripheral:(id<GNCPeripheral>)peripheral {
  dispatch_assert_queue(_queue);
  GNCGATTConnectionCompletionHandler handler = _connectionCompletionHandlers[peripheral.identifier];
  _connectionCompletionHandlers[peripheral.identifier] = nil;
  if (handler) {
    GNCBLEGATTClient *client =
        [[GNCBLEGATTClient alloc] initWithPeripheral:peripheral
                         requestDisconnectionHandler:^(id<GNCPeripheral> peripheral) {
                           dispatch_async(_queue, ^{
                             [_centralManager cancelPeripheralConnection:peripheral];
                           });
                         }];
    handler(client, nil);
  }
}

- (void)gnc_centralManager:(id<GNCCentralManager>)central
    didFailToConnectPeripheral:(id<GNCPeripheral>)peripheral
                         error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  GNCGATTConnectionCompletionHandler handler = _connectionCompletionHandlers[peripheral.identifier];
  _connectionCompletionHandlers[peripheral.identifier] = nil;
  if (handler) {
    handler(nil, error);
  }
}

- (void)gnc_centralManager:(id<GNCCentralManager>)central
    didDisconnectPeripheral:(id<GNCPeripheral>)peripheral
                      error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  GNCGATTDisconnectionHandler handler = _disconnectionHandlers[peripheral.identifier];
  _disconnectionHandlers[peripheral.identifier] = nil;
  if (handler) {
    handler();
  }
}

#pragma mark - CBCentralManagerDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
  dispatch_async(_queue, ^{
    [self gnc_centralManagerDidUpdateState:central];
  });
}

- (void)centralManager:(CBCentralManager *)central
    didDiscoverPeripheral:(CBPeripheral *)peripheral
        advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                     RSSI:(NSNumber *)RSSI {
  dispatch_async(_queue, ^{
    [self gnc_centralManager:central
        didDiscoverPeripheral:peripheral
            advertisementData:advertisementData
                         RSSI:RSSI];
  });
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {
  dispatch_async(_queue, ^{
    [self gnc_centralManager:central didConnectPeripheral:peripheral];
  });
}

- (void)centralManager:(CBCentralManager *)central
    didFailToConnectPeripheral:(CBPeripheral *)peripheral
                         error:(nullable NSError *)error {
  dispatch_async(_queue, ^{
    [self gnc_centralManager:central didFailToConnectPeripheral:peripheral error:error];
  });
}

- (void)centralManager:(CBCentralManager *)central
    didDisconnectPeripheral:(CBPeripheral *)peripheral
                      error:(nullable NSError *)error {
  dispatch_async(_queue, ^{
    [self gnc_centralManager:central didDisconnectPeripheral:peripheral error:error];
  });
}

@end

NS_ASSUME_NONNULL_END
