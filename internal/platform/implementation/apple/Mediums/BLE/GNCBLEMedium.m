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
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPServer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCCentralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralManagerMultiplexer.h"
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

@interface GNCBLEMedium () <GNCCentralManagerDelegate, CBCentralManagerDelegate>
- (instancetype)initWithCentralManager:(id<GNCCentralManager>)centralManager
                     peripheralManager:(nullable id<GNCPeripheralManager>)peripheralManager
                                 queue:(dispatch_queue_t)queue;
@end

@implementation GNCBLEMedium {
  dispatch_queue_t _queue;
  id<GNCCentralManager> _centralManager;
  id<GNCPeripheralManager> _peripheralManager;
  GNCPeripheralManagerMultiplexer *_multiplexer;

  // The active GATT server, or @nil if one hasn't been started yet.
  GNCBLEGATTServer *_server;

  // The active L2CAP server, or @nil if one hasn't been started yet.
  GNCBLEL2CAPServer *_l2capServer;

  // The active L2CAP client, or @nil if one hasn't been started yet.
  GNCBLEL2CAPClient *_l2capClient;

  // Per-peripheral PSMs for pending L2CAP channel open requests.
  NSMutableDictionary<NSUUID *, NSNumber *> *_l2capPSMs;

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

  // Per-peripheral timeout blocks for pending GATT and L2CAP connection attempts.
  NSMutableDictionary<NSUUID *, dispatch_block_t> *_gattConnectionTimeoutBlocks;
  NSMutableDictionary<NSUUID *, dispatch_block_t> *_l2capConnectionTimeoutBlocks;

  // Active LE link claims per peripheral: tokens for active GATT clients and identifiers for
  // pending/open L2CAP channels. The peripheral connection is cancelled only when no claim remains.
  NSMutableDictionary<NSUUID *, NSMutableSet<NSObject *> *> *_activeGATTClientTokens;
  NSMutableSet<NSUUID *> *_activeL2CAPPeripheralIdentifiers;

  // The timeout duration for peripheral connection attempts.
  NSTimeInterval _peripheralConnectTimeout;
}

- (instancetype)init {
  dispatch_queue_t queue = dispatch_queue_create(kBLEMediumQueueLabel, DISPATCH_QUEUE_SERIAL);
  CBCentralManager *centralManager =
      [[CBCentralManager alloc] initWithDelegate:nil
                                           queue:queue
                                         options:@{CBCentralManagerOptionShowPowerAlertKey : @NO}];
  CBPeripheralManager *peripheralManager = [[CBPeripheralManager alloc] initWithDelegate:nil
                                                                                   queue:queue];
  return [self initWithCentralManager:centralManager
                    peripheralManager:peripheralManager
                                queue:queue];
}

// This is private and should only be used for tests. The provided central manager must call
// delegate methods on the main queue.
- (instancetype)initWithCentralManager:(id<GNCCentralManager>)centralManager
                     peripheralManager:(nullable id<GNCPeripheralManager>)peripheralManager
                                 queue:(dispatch_queue_t)queue {
  self = [super init];
  if (self) {
    _queue = queue;
    _centralManager = centralManager;
    _centralManager.centralDelegate = self;
    _peripheralManager = peripheralManager;
    if (GNCFeatureFlags.sharedPeripheralManagerEnabled && _peripheralManager) {
      _multiplexer = [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:_queue];
      _peripheralManager.peripheralDelegate = _multiplexer;
    }
    _gattConnectionCompletionHandlers = [NSMutableDictionary dictionary];
    _gattDisconnectionHandlers = [NSMutableDictionary dictionary];
    _scanningServiceUUIDs = [NSMutableArray array];
    _l2capStreamCompletionHandlers = [NSMutableDictionary dictionary];
    _l2capPSMs = [NSMutableDictionary dictionary];
    _gattConnectionTimeoutBlocks = [NSMutableDictionary dictionary];
    _l2capConnectionTimeoutBlocks = [NSMutableDictionary dictionary];
    _activeGATTClientTokens = [NSMutableDictionary dictionary];
    _activeL2CAPPeripheralIdentifiers = [NSMutableSet set];
    _peripheralConnectTimeout = kPeripheralConnectTimeout;
  }
  return self;
}

- (void)stop {
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  dispatch_async(_queue, ^{
    [self cancelAllConnectionTimeouts];
    dispatch_semaphore_signal(semaphore);
  });
  dispatch_time_t timeout =
      dispatch_time(DISPATCH_TIME_NOW, kStopTimeout * NSEC_PER_SEC);  // 5 seconds timeout
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"GNCBLEMedium stop timeout.");
  }
}

- (nullable CBPeripheral *)retrievePeripheralWithIdentifier:(NSUUID *)identifier {
  NSAssert(_centralManager, @"CBCentralManager not created.");
  NSAssert(identifier, @"Should have an identifier, self: %@", self);

  NSArray<CBPeripheral *> *peripherals =
      [_centralManager retrievePeripheralsWithIdentifiers:@[ identifier ]];

  for (CBPeripheral *peripheral in peripherals) {
    if ([peripheral.identifier isEqual:identifier]) {
      return peripheral;
    }
  }

  return nil;
}

- (BOOL)supportsExtendedAdvertisements {
  // TODO(b/294736083): CoreBluetooth doesn't support actually advertising any extensions, however
  // some devices can scan for them if the feature is available. If we return @c YES from this
  // method, we would be enabling advertising extensions (which won't work), so we must return @c NO
  // until we add support for a new method to check only if extended scans are supported.
  return NO;
}

- (void)dealloc {
  [_centralManager stopScan];
  _centralManager.centralDelegate = nil;

  [_peripheralManager stopAdvertising];
  _peripheralManager.peripheralDelegate = nil;
}

- (void)startAdvertisingData:(NSDictionary<CBUUID *, NSData *> *)serviceData
           completionHandler:(nullable GNCStartAdvertisingCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    if (!_server) {
      if (GNCFeatureFlags.sharedPeripheralManagerEnabled) {
        _server = [[GNCBLEGATTServer alloc] initWithPeripheralManager:_peripheralManager
                                                                queue:_queue];
        [_multiplexer addListener:_server];
      } else {
        // In legacy mode, we pass nil (or a separate manager) and do NOT add to multiplexer.
        // GNCBLEGATTServer will create its own internal manager.
        _server = [[GNCBLEGATTServer alloc] initWithPeripheralManager:nil queue:nil];
      }
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

    [self updateScanningState];
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
    [self updateScanningState];
    if (completionHandler) {
      completionHandler(nil);
    }
  });
}

- (void)startGATTServerWithCompletionHandler:
    (nullable GNCGATTServerCompletionHandler)completionHandler {
  dispatch_async(_queue, ^{
    if (!_server) {
      if (GNCFeatureFlags.sharedPeripheralManagerEnabled) {
        _server = [[GNCBLEGATTServer alloc] initWithPeripheralManager:_peripheralManager
                                                                queue:_queue];
        [_multiplexer addListener:_server];
      } else {
        _server = [[GNCBLEGATTServer alloc] initWithPeripheralManager:nil queue:nil];
      }
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
    NSUUID *identifier = remotePeripheral.identifier;
    GNCGATTConnectionCompletionHandler previousHandler =
        strongSelf->_gattConnectionCompletionHandlers[identifier];
    if (previousHandler) {
      [strongSelf->_gattConnectionCompletionHandlers removeObjectForKey:identifier];
      previousHandler(nil, [NSError errorWithDomain:GNCBLEErrorDomain
                                               code:GNCBLEErrorUnknown
                                           userInfo:nil]);
    }
    strongSelf->_gattDisconnectionHandlers[identifier] = disconnectionHandler;
    strongSelf->_gattConnectionCompletionHandlers[identifier] = completionHandler;
    if (GNCFeatureFlags.bleL2capEnabled) {
      [strongSelf
          scheduleTimeoutInMap:strongSelf->_gattConnectionTimeoutBlocks
                 forPeripheral:remotePeripheral
                       timeout:strongSelf->_peripheralConnectTimeout
               timeoutCallback:^(NSError *error) {
                 [strongSelf->_gattConnectionCompletionHandlers removeObjectForKey:identifier];
                 [strongSelf cancelPeripheralConnectionIfUnused:remotePeripheral];
                 if (completionHandler) {
                   completionHandler(nil, error);
                 }
               }];
    }
    [strongSelf->_centralManager connectPeripheral:remotePeripheral options:@{}];
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
      if (GNCFeatureFlags.sharedPeripheralManagerEnabled) {
        _l2capServer = [[GNCBLEL2CAPServer alloc]
            initWithPeripheralManager:peripheralManager ?: _peripheralManager
                                queue:peripheralManager ? dispatch_get_main_queue() : _queue];
        // Only add to multiplexer if we are using the internal shared manager.
        // If a specific manager was passed in (e.g. for testing?), we might still need logic here.
        // But typically `peripheralManager` is nil in prod.
        if (peripheralManager == nil || peripheralManager == _peripheralManager) {
          [_multiplexer addListener:_l2capServer];
        }
      } else {
        // Legacy mode
        _l2capServer = [[GNCBLEL2CAPServer alloc]
            initWithPeripheralManager:peripheralManager  // Likely nil, so Server creates new one
                                queue:peripheralManager ? dispatch_get_main_queue() : nil];
      }
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
                        initWithQueue:strongSelf->_queue
          requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull peripheral) {
            dispatch_async(strongSelf->_queue, ^{
              __strong __typeof__(self) innerSelf = weakSelf;
              if (!innerSelf || !peripheral) {
                return;
              }
              [innerSelf->_activeL2CAPPeripheralIdentifiers removeObject:peripheral.identifier];
              [innerSelf cancelPeripheralConnectionIfUnused:peripheral];
            });
          }];
    }
    NSUUID *identifier = remotePeripheral.identifier;
    GNCOpenL2CAPStreamCompletionHandler previousHandler =
        strongSelf->_l2capStreamCompletionHandlers[identifier];
    if (previousHandler) {
      [strongSelf->_l2capStreamCompletionHandlers removeObjectForKey:identifier];
      previousHandler(nil, [NSError errorWithDomain:GNCBLEErrorDomain
                                               code:GNCBLEErrorUnknown
                                           userInfo:nil]);
    }
    strongSelf->_l2capStreamCompletionHandlers[identifier] = completionHandler;
    strongSelf->_l2capPSMs[identifier] = @(psm);
    [strongSelf->_activeL2CAPPeripheralIdentifiers addObject:identifier];
    // There is a Core Bluetooth problem: Either -didConnectPeripheral or
    // -didFailToConnectPeripheral should be called at this point, but sometimes neither is
    // called.
    // TODO: b/419127415 - Investigate the root cause of this problem.
    [strongSelf scheduleTimeoutInMap:strongSelf->_l2capConnectionTimeoutBlocks
                       forPeripheral:remotePeripheral
                             timeout:strongSelf->_peripheralConnectTimeout
                     timeoutCallback:^(NSError *error) {
                       [strongSelf->_l2capStreamCompletionHandlers removeObjectForKey:identifier];
                       [strongSelf->_l2capPSMs removeObjectForKey:identifier];
                       [strongSelf->_activeL2CAPPeripheralIdentifiers removeObject:identifier];
                       [strongSelf cancelPeripheralConnectionIfUnused:remotePeripheral];
                       if (completionHandler) {
                         completionHandler(nil, error);
                       }
                     }];
    [strongSelf->_centralManager connectPeripheral:remotePeripheral options:@{}];
  });
}

// This is private and should only be used for tests. The provided L2CAP client must call
// delegate methods on the main queue.
- (void)setL2CAPClient:(GNCBLEL2CAPClient *)l2capClient {
  _l2capClient = l2capClient;
}

// This is private and should only be used for tests.
- (void)setPeripheralConnectTimeout:(NSTimeInterval)timeout {
  _peripheralConnectTimeout = timeout;
}

#pragma mark - Internal

- (void)updateScanningState {
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

- (void)scheduleTimeoutInMap:(NSMutableDictionary<NSUUID *, dispatch_block_t> *)timeoutBlocks
               forPeripheral:(id<GNCPeripheral>)peripheral
                     timeout:(NSTimeInterval)timeout
             timeoutCallback:(void (^)(NSError *))callback {
  dispatch_assert_queue(_queue);
  NSUUID *identifier = peripheral.identifier;
  [self cancelConnectionTimeoutInMap:timeoutBlocks forIdentifier:identifier];
  __weak __typeof__(self) weakSelf = self;
  dispatch_block_t timeoutBlock = dispatch_block_create(0, ^{
    __strong __typeof__(self) strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    [timeoutBlocks removeObjectForKey:identifier];
    if (callback) {
      callback([NSError errorWithDomain:GNCBLEErrorDomain
                                   code:GNCBLEErrorBlePerperipheralConnectionTimeout
                               userInfo:nil]);
    }
  });
  if (timeoutBlock) {
    timeoutBlocks[identifier] = timeoutBlock;
    dispatch_time_t delay = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeout * NSEC_PER_SEC));
    dispatch_after(delay, _queue, timeoutBlock);
  }
}

- (void)cancelConnectionTimeoutInMap:
            (NSMutableDictionary<NSUUID *, dispatch_block_t> *)timeoutBlocks
                       forIdentifier:(NSUUID *)identifier {
  dispatch_assert_queue(_queue);
  dispatch_block_t timeoutBlock = timeoutBlocks[identifier];
  if (timeoutBlock) {
    dispatch_block_cancel(timeoutBlock);
    [timeoutBlocks removeObjectForKey:identifier];
  }
}

- (void)cancelAllConnectionTimeouts {
  dispatch_assert_queue(_queue);
  for (dispatch_block_t timeoutBlock in [_gattConnectionTimeoutBlocks allValues]) {
    dispatch_block_cancel(timeoutBlock);
  }
  [_gattConnectionTimeoutBlocks removeAllObjects];
  for (dispatch_block_t timeoutBlock in [_l2capConnectionTimeoutBlocks allValues]) {
    dispatch_block_cancel(timeoutBlock);
  }
  [_l2capConnectionTimeoutBlocks removeAllObjects];
}

- (void)cancelPeripheralConnectionIfUnused:(id<GNCPeripheral>)peripheral {
  dispatch_assert_queue(_queue);
  NSUUID *identifier = peripheral.identifier;
  if (!identifier) {
    return;
  }
  BOOL hasGATT = _activeGATTClientTokens[identifier].count > 0 ||
                 _gattConnectionCompletionHandlers[identifier] != nil;
  BOOL hasL2CAP = [_activeL2CAPPeripheralIdentifiers containsObject:identifier] ||
                  _l2capStreamCompletionHandlers[identifier] != nil;
  if (!hasGATT && !hasL2CAP) {
    [_centralManager cancelPeripheralConnection:peripheral];
  }
}

- (void)internalOpenL2CAPChannel:(id<GNCPeripheral>)remotePeripheral {
  dispatch_assert_queue(_queue);

  NSUUID *identifier = remotePeripheral.identifier;
  GNCOpenL2CAPStreamCompletionHandler handler = _l2capStreamCompletionHandlers[identifier];
  NSNumber *psmNumber = _l2capPSMs[identifier];
  [_l2capStreamCompletionHandlers removeObjectForKey:identifier];
  [_l2capPSMs removeObjectForKey:identifier];

  if (!handler || !psmNumber) {
    [_activeL2CAPPeripheralIdentifiers removeObject:identifier];
    [self cancelPeripheralConnectionIfUnused:remotePeripheral];
    return;
  }

  __weak __typeof__(self) weakSelf = self;
  [_l2capClient
      openL2CAPChannelWithPSM:psmNumber.unsignedShortValue
                   peripheral:remotePeripheral
            completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
              __strong __typeof__(self) strongSelf = weakSelf;
              if (!strongSelf) {
                return;
              }
              dispatch_async(strongSelf->_queue, ^{
                if (error) {
                  [strongSelf->_activeL2CAPPeripheralIdentifiers removeObject:identifier];
                  [strongSelf cancelPeripheralConnectionIfUnused:remotePeripheral];
                }
                handler(stream, error);
              });
            }];
}

#pragma mark - GNCCentralManagerDelegate

- (void)gnc_centralManagerDidUpdateState:(id<GNCCentralManager>)central {
  if (!_queue) {
    return;
  }
  dispatch_assert_queue(_queue);
  [self updateScanningState];
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
  NSUUID *identifier = peripheral.identifier;

  GNCGATTConnectionCompletionHandler gattHandler = _gattConnectionCompletionHandlers[identifier];
  if (gattHandler) {
    [self cancelConnectionTimeoutInMap:_gattConnectionTimeoutBlocks forIdentifier:identifier];
    [_gattConnectionCompletionHandlers removeObjectForKey:identifier];
    NSObject *clientToken = [[NSObject alloc] init];
    NSMutableSet<NSObject *> *tokens = _activeGATTClientTokens[identifier];
    if (!tokens) {
      tokens = [NSMutableSet set];
      _activeGATTClientTokens[identifier] = tokens;
    }
    [tokens addObject:clientToken];

    __weak __typeof__(self) weakSelf = self;
    GNCBLEGATTClient *client = [[GNCBLEGATTClient alloc]
                 initWithPeripheral:peripheral
        requestDisconnectionHandler:^(id<GNCPeripheral> clientPeripheral) {
          dispatch_async(_queue, ^{
            __strong __typeof__(self) strongSelf = weakSelf;
            if (!strongSelf || !clientPeripheral) {
              return;
            }
            NSUUID *clientIdentifier = clientPeripheral.identifier;
            NSMutableSet<NSObject *> *activeTokens =
                strongSelf->_activeGATTClientTokens[clientIdentifier];
            [activeTokens removeObject:clientToken];
            if (activeTokens.count == 0) {
              [strongSelf->_activeGATTClientTokens removeObjectForKey:clientIdentifier];
            }
            [strongSelf cancelPeripheralConnectionIfUnused:clientPeripheral];
          });
        }];
    gattHandler(client, nil);
  }

  if (_l2capStreamCompletionHandlers[identifier]) {
    [self cancelConnectionTimeoutInMap:_l2capConnectionTimeoutBlocks forIdentifier:identifier];
    [self internalOpenL2CAPChannel:peripheral];
  }
}

- (void)gnc_centralManager:(id<GNCCentralManager>)central
    didFailToConnectPeripheral:(id<GNCPeripheral>)peripheral
                         error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  NSUUID *identifier = peripheral.identifier;
  [self cancelConnectionTimeoutInMap:_gattConnectionTimeoutBlocks forIdentifier:identifier];
  [self cancelConnectionTimeoutInMap:_l2capConnectionTimeoutBlocks forIdentifier:identifier];

  GNCGATTConnectionCompletionHandler gattHandler = _gattConnectionCompletionHandlers[identifier];
  [_gattConnectionCompletionHandlers removeObjectForKey:identifier];
  if (gattHandler) {
    gattHandler(nil, error);
  }

  GNCOpenL2CAPStreamCompletionHandler l2capHandler = _l2capStreamCompletionHandlers[identifier];
  [_l2capStreamCompletionHandlers removeObjectForKey:identifier];
  [_l2capPSMs removeObjectForKey:identifier];
  [_activeL2CAPPeripheralIdentifiers removeObject:identifier];
  if (l2capHandler) {
    l2capHandler(nil, error);
  }
}

- (void)gnc_centralManager:(id<GNCCentralManager>)central
    didDisconnectPeripheral:(id<GNCPeripheral>)peripheral
                      error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  NSUUID *identifier = peripheral.identifier;
  [_activeGATTClientTokens removeObjectForKey:identifier];
  [_activeL2CAPPeripheralIdentifiers removeObject:identifier];

  GNCGATTDisconnectionHandler handler = _gattDisconnectionHandlers[identifier];
  _gattDisconnectionHandlers[identifier] = nil;
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
