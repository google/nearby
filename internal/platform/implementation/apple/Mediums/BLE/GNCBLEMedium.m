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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEMedium.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Flags/GNCFeatureFlags.h"
#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTClient.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTServer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPClient.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPServer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCCentralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/NSData+GNCBase85.h"
#import "internal/platform/implementation/apple/Mediums/BLE/NSData+GNCWebSafeBase64.h"

NS_ASSUME_NONNULL_BEGIN

static char *const kBLEMediumQueueLabel = "com.nearby.GNCBLEMedium";
// TODO: b/762454867 - Make this a flag.
static const NSTimeInterval kPeripheralConnectTimeout = 10.0;  // 10 seconds timeout
static const NSTimeInterval kStopTimeout = 5.0;

static NSError *AlreadyScanningError() {
  return [NSError errorWithDomain:GNCBLEErrorDomain code:GNCBLEErrorAlreadyScanning userInfo:nil];
}

static GNCBLEL2CAPServer *_Nonnull CreateL2CapServer(
    id<GNCPeripheralManager> _Nullable peripheralManager) {
  if (!peripheralManager) {
    return [[GNCBLEL2CAPServer alloc] init];
  } else {
    return [[GNCBLEL2CAPServer alloc] initWithPeripheralManager:peripheralManager
                                                          queue:dispatch_get_main_queue()];
  }
}

@interface GNCBLEMedium () <GNCCentralManagerDelegate, CBCentralManagerDelegate>
@end

@implementation GNCBLEMedium {
  dispatch_queue_t _queue;
  id<GNCCentralManager> _centralManager;

  // The active GATT server, or @nil if one hasn't been started yet.
  GNCBLEGATTServer *_server;

  // The active L2CAP server, or @nil if one hasn't been started yet.
  GNCBLEL2CAPServer *_l2capServer;

  // The active L2CAP client, or @nil if one hasn't been started yet.
  GNCBLEL2CAPClient *_l2capClient;

  // The PSM number of the remote peripheral's L2CAP server.
  uint16_t _l2capPSM;

  // The services that is being actively scanned for.
  NSMutableArray<CBUUID *> *_scanningServiceUUIDs;

  // The handler called when an advertisement for the service represented by @c _serviceUUID has
  // been discovered. This will be called continuously, until the peripheral disappears.
  GNCAdvertisementFoundHandler _advertisementFoundHandler;

  // A peripheral to connection completion handler map. Used to track connection attempts. When a
  // connection attempt has succeeded or failed, the completion handler is called and removed from
  // the map.
  NSMutableDictionary<NSUUID *, GNCGATTConnectionCompletionHandler>
      *_gattConnectionCompletionHandlers;

  // A peripheral to disconnection handler map. Used to track when a peripheral becomes
  // disconnected. Once disconnected, the disconnection handler is called and removed from the map.
  NSMutableDictionary<NSUUID *, GNCGATTDisconnectionHandler> *_gattDisconnectionHandlers;

  // A peripheral to L2CAP stream completion handler map. Used to track L2CAP stream attempts. When
  // a L2CAP stream attempt has succeeded or failed, the completion handler is called and removed
  // from the map.
  NSMutableDictionary<NSUUID *, GNCOpenL2CAPStreamCompletionHandler>
      *_l2capStreamCompletionHandlers;

  // The block to call when the BLE connection times out.
  dispatch_block_t _connectionTimeoutBlock;
}

- (instancetype)init {
  dispatch_queue_t queue = dispatch_queue_create(kBLEMediumQueueLabel, DISPATCH_QUEUE_SERIAL);
  CBCentralManager *centralManager =
      [[CBCentralManager alloc] initWithDelegate:self
                                           queue:queue
                                         options:@{CBCentralManagerOptionShowPowerAlertKey : @NO}];
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
    _gattConnectionCompletionHandlers = [NSMutableDictionary dictionary];
    _gattDisconnectionHandlers = [NSMutableDictionary dictionary];
    _scanningServiceUUIDs = [NSMutableArray array];
    _l2capStreamCompletionHandlers = [NSMutableDictionary dictionary];
    _l2capPSM = 0;
  }
  return self;
}

- (void)stop {
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  dispatch_async(_queue, ^{
    [self cancelConnectionTimeout];
    dispatch_semaphore_signal(semaphore);
  });
  dispatch_time_t timeout =
      dispatch_time(DISPATCH_TIME_NOW, kStopTimeout * NSEC_PER_SEC);  // 5 seconds timeout
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"GNCBLEMedium stop timeout.");
  }
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
  [self startScanningForMultipleServices:@[ serviceUUID ]
               advertisementFoundHandler:advertisementFoundHandler
                       completionHandler:completionHandler];
}

- (void)startScanningForMultipleServices:(NSArray<CBUUID *> *)serviceUUIDs
               advertisementFoundHandler:(GNCAdvertisementFoundHandler)advertisementFoundHandler
                       completionHandler:
                           (nullable GNCStartScanningCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    if (_scanningServiceUUIDs.count > 0) {
      if (completionHandler) {
        completionHandler(AlreadyScanningError());
      }
      return;
    }

    [_scanningServiceUUIDs addObjectsFromArray:serviceUUIDs];
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
    [_scanningServiceUUIDs removeAllObjects];
    _advertisementFoundHandler = nil;
    [_centralManager stopScan];
    if (completionHandler) {
      completionHandler(nil);
    }
  });
}

- (void)resumeMediumScanning:(nullable GNCStartScanningCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    [self internalStartScanningIfPoweredOn];
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
  __weak __typeof__(self) weakSelf = self;
  dispatch_async(_queue, ^{
    __strong __typeof__(self) strongSelf = weakSelf;
    if (!strongSelf) {
      GNCLoggerDebug(@"GNCBLEMedium instance no longer exists when connect to gatt server.");
      return;
    }
    strongSelf->_gattDisconnectionHandlers[remotePeripheral.identifier] = disconnectionHandler;
    strongSelf->_gattConnectionCompletionHandlers[remotePeripheral.identifier] = completionHandler;
    strongSelf->_l2capPSM = 0;
    if (GNCFeatureFlags.bleL2capEnabled) {
      [strongSelf connectToPeripheral:remotePeripheral
          withTimeout:kPeripheralConnectTimeout
          options:@{}
          timeoutCallback:^(NSError *error) {
            if (completionHandler) {
              completionHandler(nil, error);
            }
          }];
    } else {
      [_centralManager connectPeripheral:remotePeripheral options:@{}];
    }
  });
}

- (void)openL2CAPServerWithPSMPublishedCompletionHandler:
            (GNCOpenL2CAPServerPSMPublishedCompletionHandler)psmPublishedCompletionHandler
                          channelOpenedCompletionHandler:
                              (GNCOpenL2CAPServerChannelOpendCompletionHandler)
                                  channelOpenedCompletionHandler
                                       peripheralManager:
                                           (nullable id<GNCPeripheralManager>)peripheralManager {
  dispatch_async(_queue, ^{
    if (!_l2capServer) {
      _l2capServer = CreateL2CapServer(peripheralManager);
    }
    [_l2capServer
        startListeningChannelWithPSMPublishedCompletionHandler:psmPublishedCompletionHandler
                                channelOpenedCompletionHandler:channelOpenedCompletionHandler];
  });
}

- (void)openL2CAPChannelWithPSM:(uint16_t)psm
                     peripheral:(id<GNCPeripheral>)remotePeripheral
              completionHandler:(nullable GNCOpenL2CAPStreamCompletionHandler)completionHandler {
  __weak __typeof__(self) weakSelf = self;
  dispatch_async(_queue, ^{
    __strong __typeof__(self) strongSelf = weakSelf;
    if (!strongSelf) {
      GNCLoggerDebug(@"GNCBLEMedium instance no longer exists when open L2CAP channel.");
      return;
    }
    if (!strongSelf->_l2capClient) {
      strongSelf->_l2capClient = [[GNCBLEL2CAPClient alloc]
          initWithRequestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull peripheral) {
            dispatch_async(_queue, ^{
              [_centralManager cancelPeripheralConnection:remotePeripheral];
            });
          }];
    }
    strongSelf->_l2capStreamCompletionHandlers[remotePeripheral.identifier] = completionHandler;
    strongSelf->_l2capPSM = psm;
    // There is a Core Bluetooth problem: Either -didConnectPeripheral or
    // -didFailToConnectPeripheral should be called at this point, but sometimes neither is
    // called.
    // TODO: b/419127415 - Investigate the root cause of this problem.
    [strongSelf connectToPeripheral:remotePeripheral
        withTimeout:kPeripheralConnectTimeout
        options:@{}
        timeoutCallback:^(NSError *error) {
          if (completionHandler) {
            completionHandler(nil, error);
          }
        }];
  });
}

// This is private and should only be used for tests. The provided L2CAP client must call
// delegate methods on the main queue.
- (void)setL2CAPClient:(GNCBLEL2CAPClient *)l2capClient {
  _l2capClient = l2capClient;
}

#pragma mark - Internal

- (void)internalStartScanningIfPoweredOn {
  dispatch_assert_queue(_queue);
  // Scanning can only be done when powered on and must be restarted if bluetooth is turned off
  // then back on. This will be called anytime the central manager's state changes, so
  // @c scanForPeripheralsWithServices:options: will be called anytime state transitions back to
  // powered on.
  if (_centralManager.state == CBManagerStatePoweredOn && _scanningServiceUUIDs.count > 0) {
    // Stop scanning just in case something outside of this class is already scanning.
    [_centralManager stopScan];
    [_centralManager
        scanForPeripheralsWithServices:_scanningServiceUUIDs
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

  NSData *data = GNCFeatureFlags.dctEnabled
                     ? [[NSData alloc] initWithBase85EncodedString:localName]
                     : [[NSData alloc] initWithWebSafeBase64EncodedString:localName];

  // A Nearby Apple advertisement should only have a single service, so simply grab the first one if
  // it exists.
  NSArray<CBUUID *> *serviceUUIDs = advertisementData[CBAdvertisementDataServiceUUIDsKey];
  CBUUID *serviceUUID = serviceUUIDs.firstObject;
  if (data && serviceUUID) {
    return @{serviceUUID : data};
  }

  return @{};
}

- (void)connectToPeripheral:(id<GNCPeripheral>)peripheral
                withTimeout:(NSTimeInterval)timeout
                    options:(nullable NSDictionary<NSString *, id> *)options
            timeoutCallback:(void (^)(NSError *))callback {
  dispatch_assert_queue(_queue);
  [self cancelConnectionTimeout];
  __weak __typeof__(self) weakSelf = self;
  _connectionTimeoutBlock = dispatch_block_create(0, ^{
    __strong __typeof__(self) strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    strongSelf->_connectionTimeoutBlock = nil;

    // Perform timeout actions
    if (callback) {
      callback([NSError errorWithDomain:GNCBLEErrorDomain
                                   code:GNCBLEErrorBlePerperipheralConnectionTimeout
                               userInfo:nil]);
    }
  });

  if (_connectionTimeoutBlock) {
    dispatch_time_t delay = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeout * NSEC_PER_SEC));
    dispatch_after(delay, _queue, _connectionTimeoutBlock);
  }

  [_centralManager connectPeripheral:peripheral options:options];
}

- (void)cancelConnectionTimeout {
  dispatch_assert_queue(_queue);
  if (_connectionTimeoutBlock) {
    dispatch_block_cancel(_connectionTimeoutBlock);
    _connectionTimeoutBlock = nil;
  }
}

- (void)internalOpenL2CAPChannel:(id<GNCPeripheral>)remotePeripheral {
  dispatch_assert_queue(_queue);

  __weak __typeof__(self) weakSelf = self;
  GNCOpenL2CAPStreamCompletionHandler handler =
      _l2capStreamCompletionHandlers[remotePeripheral.identifier];
  _l2capStreamCompletionHandlers[remotePeripheral.identifier] = nil;

  if (!handler) {
    return;
  }

  [_l2capClient
      openL2CAPChannelWithPSM:_l2capPSM
                   peripheral:remotePeripheral
            completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
              __typeof__(self) strongSelf = weakSelf;
              if (!strongSelf) {
                return;
              }
              dispatch_async(strongSelf->_queue, ^{
                if (error) {
                  handler(nil, error);
                } else {
                  handler(stream, nil);
                }
              });
            }];
}

#pragma mark - GNCCentralManagerDelegate

- (void)gnc_centralManagerDidUpdateState:(id<GNCCentralManager>)central {
  if (!_queue) {
    return;
  }
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
  [self cancelConnectionTimeout];
  if (_l2capPSM > 0) {
    [self internalOpenL2CAPChannel:peripheral];
    return;
  }
  GNCGATTConnectionCompletionHandler handler =
      _gattConnectionCompletionHandlers[peripheral.identifier];
  _gattConnectionCompletionHandlers[peripheral.identifier] = nil;
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
  [self cancelConnectionTimeout];
  if (_l2capPSM > 0) {
    GNCOpenL2CAPStreamCompletionHandler l2capStreamCompletionhandler =
        _l2capStreamCompletionHandlers[peripheral.identifier];
    _l2capStreamCompletionHandlers[peripheral.identifier] = nil;
    if (l2capStreamCompletionhandler) {
      l2capStreamCompletionhandler(nil, error);
    }
    return;
  }
  GNCGATTConnectionCompletionHandler handler =
      _gattConnectionCompletionHandlers[peripheral.identifier];
  _gattConnectionCompletionHandlers[peripheral.identifier] = nil;
  if (handler) {
    handler(nil, error);
  }
}

- (void)gnc_centralManager:(id<GNCCentralManager>)central
    didDisconnectPeripheral:(id<GNCPeripheral>)peripheral
                      error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  GNCGATTDisconnectionHandler handler = _gattDisconnectionHandlers[peripheral.identifier];
  _gattDisconnectionHandlers[peripheral.identifier] = nil;
  if (handler) {
    handler();
  }
}

#pragma mark - CBCentralManagerDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
  [self gnc_centralManagerDidUpdateState:central];
}

- (void)centralManager:(CBCentralManager *)central
    didDiscoverPeripheral:(CBPeripheral *)peripheral
        advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                     RSSI:(NSNumber *)RSSI {
  [self gnc_centralManager:central
      didDiscoverPeripheral:peripheral
          advertisementData:advertisementData
                       RSSI:RSSI];
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {
  [self gnc_centralManager:central didConnectPeripheral:peripheral];
}

- (void)centralManager:(CBCentralManager *)central
    didFailToConnectPeripheral:(CBPeripheral *)peripheral
                         error:(nullable NSError *)error {
  [self gnc_centralManager:central didFailToConnectPeripheral:peripheral error:error];
}

- (void)centralManager:(CBCentralManager *)central
    didDisconnectPeripheral:(CBPeripheral *)peripheral
                      error:(nullable NSError *)error {
  [self gnc_centralManager:central didDisconnectPeripheral:peripheral error:error];
}

@end

NS_ASSUME_NONNULL_END
