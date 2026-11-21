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
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralDelegateMultiplexer.h"
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

/** Tracks a pending or in-flight L2CAP channel open attempt for a peripheral. */
@interface GNCL2CAPChannelOpenAttempt : NSObject

@property(nonatomic, readonly) uint16_t psm;
@property(nonatomic, copy, readonly, nullable)
    GNCOpenL2CAPStreamCompletionHandler completionHandler;
@property(nonatomic, strong, readonly, nullable) GNCBLEL2CAPClient *ownedClient;
@property(nonatomic, strong, readonly, nullable) GNCBLEL2CAPClient *activeClient;
@property(nonatomic) BOOL channelOpenStarted;
@property(nonatomic) BOOL waitingForDisconnect;

- (instancetype)initWithPSM:(uint16_t)psm
          completionHandler:(nullable GNCOpenL2CAPStreamCompletionHandler)completionHandler
                ownedClient:(nullable GNCBLEL2CAPClient *)ownedClient
               activeClient:(nullable GNCBLEL2CAPClient *)activeClient NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@implementation GNCL2CAPChannelOpenAttempt

- (instancetype)initWithPSM:(uint16_t)psm
          completionHandler:(nullable GNCOpenL2CAPStreamCompletionHandler)completionHandler
                ownedClient:(nullable GNCBLEL2CAPClient *)ownedClient
               activeClient:(nullable GNCBLEL2CAPClient *)activeClient {
  self = [super init];
  if (self) {
    _psm = psm;
    _completionHandler = [completionHandler copy];
    _ownedClient = ownedClient;
    _activeClient = activeClient;
  }
  return self;
}

@end

/** Tracks a GATT connection attempt queued behind an in-flight local disconnect. */
@interface GNCQueuedGATTConnectionAttempt : NSObject

@property(nonatomic, strong, readonly) id<GNCPeripheral> peripheral;
@property(nonatomic, copy, readonly, nullable) GNCGATTDisconnectionHandler disconnectionHandler;
@property(nonatomic, copy, readonly) GNCGATTConnectionCompletionHandler completionHandler;

- (instancetype)initWithPeripheral:(id<GNCPeripheral>)peripheral
              disconnectionHandler:(nullable GNCGATTDisconnectionHandler)disconnectionHandler
                 completionHandler:(GNCGATTConnectionCompletionHandler)completionHandler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@implementation GNCQueuedGATTConnectionAttempt

- (instancetype)initWithPeripheral:(id<GNCPeripheral>)peripheral
              disconnectionHandler:(nullable GNCGATTDisconnectionHandler)disconnectionHandler
                 completionHandler:(GNCGATTConnectionCompletionHandler)completionHandler {
  self = [super init];
  if (self) {
    _peripheral = peripheral;
    _disconnectionHandler = [disconnectionHandler copy];
    _completionHandler = [completionHandler copy];
  }
  return self;
}

@end

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

  // The active L2CAP client injected for testing, or @nil in production.
  GNCBLEL2CAPClient *_l2capClient;

  // Per-peripheral L2CAP clients, pending PSMs, and pending/in-flight open attempts.
  NSMutableDictionary<NSUUID *, NSMutableSet<GNCBLEL2CAPClient *> *> *_l2capClients;
  NSMutableDictionary<NSUUID *, NSNumber *> *_l2capPSMs;
  NSMutableDictionary<NSUUID *, GNCL2CAPChannelOpenAttempt *> *_l2capOpenAttempts;

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

  // Active LE link claims per peripheral: weak references to active GATT clients. The peripheral
  // connection is cancelled only when no GATT or L2CAP claim remains.
  NSMutableDictionary<NSUUID *, NSHashTable<GNCBLEGATTClient *> *> *_activeGATTClients;

  // The timeout duration for peripheral connection attempts.
  NSTimeInterval _peripheralConnectTimeout;

  // Per-peripheral delegate multiplexers so GATT and L2CAP clients can share a peripheral.
  NSMutableDictionary<NSUUID *, GNCPeripheralDelegateMultiplexer *>
      *_peripheralDelegateMultiplexers;

  // Peripherals with a local cancelPeripheralConnection: awaiting didDisconnectPeripheral:.
  NSMutableSet<NSUUID *> *_cancelingPeripheralIdentifiers;

  // Peripherals whose connectPeripheral: is queued behind an in-flight local disconnect.
  NSMutableDictionary<NSUUID *, id<GNCPeripheral>> *_queuedConnectPeripherals;

  // GATT connection attempts queued behind an in-flight local disconnect.
  NSMutableDictionary<NSUUID *, GNCQueuedGATTConnectionAttempt *> *_queuedGATTConnectionAttempts;
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
    _l2capClients = [NSMutableDictionary dictionary];
    _l2capPSMs = [NSMutableDictionary dictionary];
    _l2capOpenAttempts = [NSMutableDictionary dictionary];
    _gattConnectionTimeoutBlocks = [NSMutableDictionary dictionary];
    _l2capConnectionTimeoutBlocks = [NSMutableDictionary dictionary];
    _activeGATTClients = [NSMutableDictionary dictionary];
    _peripheralConnectTimeout = kPeripheralConnectTimeout;
    _peripheralDelegateMultiplexers = [NSMutableDictionary dictionary];
    _cancelingPeripheralIdentifiers = [NSMutableSet set];
    _queuedConnectPeripherals = [NSMutableDictionary dictionary];
    _queuedGATTConnectionAttempts = [NSMutableDictionary dictionary];
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
  completionHandler = [(completionHandler
                            ?: ^(GNCBLEGATTClient *_Nullable client, NSError *_Nullable error){
                               }) copy];
  __weak __typeof__(self) weakSelf = self;
  dispatch_queue_t queue = _queue;
  dispatch_async(queue, ^{
    __strong __typeof__(self) strongSelf = weakSelf;
    if (!strongSelf) {
      GNCLoggerDebug(@"GNCBLEMedium instance no longer exists when connect to gatt server.");
      return;
    }
    NSUUID *identifier = remotePeripheral.identifier;
    if (!identifier) {
      completionHandler(nil, [NSError errorWithDomain:GNCBLEErrorDomain
                                                 code:GNCBLEErrorUnknown
                                             userInfo:nil]);
      return;
    }
    [strongSelf cancelConnectionTimeoutInMap:strongSelf->_gattConnectionTimeoutBlocks
                               forIdentifier:identifier];
    GNCQueuedGATTConnectionAttempt *queuedAttempt = nil;
    if ([strongSelf->_cancelingPeripheralIdentifiers containsObject:identifier]) {
      GNCQueuedGATTConnectionAttempt *previousQueuedAttempt =
          strongSelf->_queuedGATTConnectionAttempts[identifier];
      if (previousQueuedAttempt) {
        [strongSelf->_queuedGATTConnectionAttempts removeObjectForKey:identifier];
        previousQueuedAttempt.completionHandler(nil, [NSError errorWithDomain:GNCBLEErrorDomain
                                                                         code:GNCBLEErrorUnknown
                                                                     userInfo:nil]);
      }
      queuedAttempt =
          [[GNCQueuedGATTConnectionAttempt alloc] initWithPeripheral:remotePeripheral
                                                disconnectionHandler:disconnectionHandler
                                                   completionHandler:completionHandler];
      strongSelf->_queuedGATTConnectionAttempts[identifier] = queuedAttempt;
      strongSelf->_queuedConnectPeripherals[identifier] = remotePeripheral;
    } else {
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
    }
    if (GNCFeatureFlags.bleL2capEnabled) {
      __weak GNCQueuedGATTConnectionAttempt *weakQueuedAttempt = queuedAttempt;
      [strongSelf
          scheduleTimeoutInMap:strongSelf->_gattConnectionTimeoutBlocks
                 forPeripheral:remotePeripheral
                       timeout:strongSelf->_peripheralConnectTimeout
               timeoutCallback:^(NSError *error) {
                 __strong __typeof__(self) innerSelf = weakSelf;
                 if (!innerSelf) {
                   return;
                 }
                 GNCQueuedGATTConnectionAttempt *strongQueuedAttempt = weakQueuedAttempt;
                 if (strongQueuedAttempt &&
                     innerSelf->_queuedGATTConnectionAttempts[identifier] == strongQueuedAttempt) {
                   [innerSelf->_queuedGATTConnectionAttempts removeObjectForKey:identifier];
                   if (!innerSelf->_l2capOpenAttempts[identifier]) {
                     [innerSelf->_queuedConnectPeripherals removeObjectForKey:identifier];
                   }
                   completionHandler(nil, error);
                   return;
                 }
                 [innerSelf->_gattConnectionCompletionHandlers removeObjectForKey:identifier];
                 [innerSelf->_gattDisconnectionHandlers removeObjectForKey:identifier];
                 [innerSelf cancelPeripheralConnectionIfUnused:remotePeripheral];
                 completionHandler(nil, error);
               }];
    }
    if (!queuedAttempt) {
      [strongSelf->_centralManager connectPeripheral:remotePeripheral options:@{}];
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
  completionHandler =
      completionHandler ?: ^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
      };
  __weak __typeof__(self) weakSelf = self;
  dispatch_queue_t queue = _queue;
  dispatch_async(queue, ^{
    __strong __typeof__(self) strongSelf = weakSelf;
    if (!strongSelf) {
      GNCLoggerDebug(@"GNCBLEMedium instance no longer exists when open L2CAP channel.");
      return;
    }
    NSUUID *identifier = remotePeripheral.identifier;
    if (!identifier) {
      if (completionHandler) {
        completionHandler(nil, [NSError errorWithDomain:GNCBLEErrorDomain
                                                   code:GNCBLEErrorUnknown
                                               userInfo:nil]);
      }
      return;
    }
    [strongSelf cancelConnectionTimeoutInMap:strongSelf->_l2capConnectionTimeoutBlocks
                               forIdentifier:identifier];
    GNCOpenL2CAPStreamCompletionHandler fallbackPreviousHandler =
        strongSelf->_l2capStreamCompletionHandlers[identifier];
    GNCL2CAPChannelOpenAttempt *previousAttempt =
        [strongSelf takeL2CAPOpenAttemptForIdentifier:identifier];
    GNCOpenL2CAPStreamCompletionHandler previousHandler =
        previousAttempt ? previousAttempt.completionHandler : fallbackPreviousHandler;
    if (previousHandler) {
      previousHandler(nil, [NSError errorWithDomain:GNCBLEErrorDomain
                                               code:GNCBLEErrorUnknown
                                           userInfo:nil]);
    }
    GNCBLEL2CAPClient *ownedClient = nil;
    if (!strongSelf->_l2capClient) {
      __block __weak GNCBLEL2CAPClient *weakClient = nil;
      GNCBLEL2CAPClient *client = [[GNCBLEL2CAPClient alloc]
          initWithRequestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull peripheral) {
            __strong GNCBLEL2CAPClient *strongClient = weakClient;
            dispatch_async(queue, ^{
              __strong __typeof__(self) innerSelf = weakSelf;
              if (!innerSelf || !strongClient || !peripheral) {
                return;
              }
              NSUUID *peripheralIdentifier = peripheral.identifier ?: identifier;
              if ([innerSelf removeL2CAPClient:strongClient forIdentifier:peripheralIdentifier]) {
                [innerSelf cancelPeripheralConnectionIfUnused:peripheral];
              }
            });
          }];
      weakClient = client;
      ownedClient = client;
      [strongSelf addL2CAPClient:client forIdentifier:identifier];
    }
    GNCBLEL2CAPClient *activeClient = strongSelf->_l2capClient ?: ownedClient;
    GNCL2CAPChannelOpenAttempt *attempt =
        [[GNCL2CAPChannelOpenAttempt alloc] initWithPSM:psm
                                      completionHandler:completionHandler
                                            ownedClient:ownedClient
                                           activeClient:activeClient];
    BOOL waitingForDisconnect =
        [strongSelf->_cancelingPeripheralIdentifiers containsObject:identifier];
    attempt.waitingForDisconnect = waitingForDisconnect;
    strongSelf->_l2capOpenAttempts[identifier] = attempt;
    strongSelf->_l2capStreamCompletionHandlers[identifier] = completionHandler;
    strongSelf->_l2capPSMs[identifier] = @(psm);
    // There is a Core Bluetooth problem: Either -didConnectPeripheral or
    // -didFailToConnectPeripheral should be called at this point, but sometimes neither is
    // called. CoreBluetooth L2CAP channel negotiation can also stall after connection.
    // TODO: b/419127415 - Investigate the root cause of this problem.
    __weak GNCL2CAPChannelOpenAttempt *weakAttempt = attempt;
    [strongSelf scheduleTimeoutInMap:strongSelf->_l2capConnectionTimeoutBlocks
                       forPeripheral:remotePeripheral
                             timeout:strongSelf->_peripheralConnectTimeout
                     timeoutCallback:^(NSError *error) {
                       __strong __typeof__(self) innerSelf = weakSelf;
                       GNCL2CAPChannelOpenAttempt *strongAttempt = weakAttempt;
                       if (!innerSelf || !strongAttempt ||
                           innerSelf->_l2capOpenAttempts[identifier] != strongAttempt) {
                         return;
                       }
                       BOOL wasWaitingForDisconnect = strongAttempt.waitingForDisconnect;
                       [innerSelf takeL2CAPOpenAttemptForIdentifier:identifier];
                       if (wasWaitingForDisconnect &&
                           !innerSelf->_queuedGATTConnectionAttempts[identifier]) {
                         [innerSelf->_queuedConnectPeripherals removeObjectForKey:identifier];
                       }
                       [innerSelf cancelPeripheralConnectionIfUnused:remotePeripheral];
                       if (strongAttempt.completionHandler) {
                         strongAttempt.completionHandler(nil, error);
                       }
                     }];
    if (waitingForDisconnect) {
      strongSelf->_queuedConnectPeripherals[identifier] = remotePeripheral;
    } else {
      [strongSelf->_centralManager connectPeripheral:remotePeripheral options:@{}];
    }
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
  __weak NSMutableDictionary<NSUUID *, dispatch_block_t> *weakTimeoutBlocks = timeoutBlocks;
  dispatch_block_t timeoutBlock = dispatch_block_create(0, ^{
    __strong __typeof__(self) strongSelf = weakSelf;
    NSMutableDictionary<NSUUID *, dispatch_block_t> *strongTimeoutBlocks = weakTimeoutBlocks;
    if (!strongSelf || !strongTimeoutBlocks) {
      return;
    }
    [strongTimeoutBlocks removeObjectForKey:identifier];
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

- (void)addL2CAPClient:(GNCBLEL2CAPClient *)client forIdentifier:(NSUUID *)identifier {
  dispatch_assert_queue(_queue);
  NSMutableSet<GNCBLEL2CAPClient *> *clients = _l2capClients[identifier];
  if (!clients) {
    clients = [NSMutableSet set];
    _l2capClients[identifier] = clients;
  }
  [clients addObject:client];
}

- (BOOL)removeL2CAPClient:(nullable GNCBLEL2CAPClient *)client forIdentifier:(NSUUID *)identifier {
  dispatch_assert_queue(_queue);
  if (!client) {
    return NO;
  }
  NSMutableSet<GNCBLEL2CAPClient *> *clients = _l2capClients[identifier];
  if (![clients containsObject:client]) {
    return NO;
  }
  [clients removeObject:client];
  if (clients.count == 0) {
    [_l2capClients removeObjectForKey:identifier];
  }
  return YES;
}

- (nullable GNCL2CAPChannelOpenAttempt *)takeL2CAPOpenAttemptForIdentifier:(NSUUID *)identifier {
  dispatch_assert_queue(_queue);
  GNCL2CAPChannelOpenAttempt *attempt = _l2capOpenAttempts[identifier];
  [_l2capOpenAttempts removeObjectForKey:identifier];
  [_l2capStreamCompletionHandlers removeObjectForKey:identifier];
  [_l2capPSMs removeObjectForKey:identifier];
  if (attempt) {
    [self removeL2CAPClient:attempt.ownedClient forIdentifier:identifier];
    if (attempt.channelOpenStarted) {
      [attempt.activeClient disconnect];
    }
  }
  return attempt;
}

- (BOOL)hasActiveGATTClientsForIdentifier:(NSUUID *)identifier {
  dispatch_assert_queue(_queue);
  NSHashTable<GNCBLEGATTClient *> *clients = _activeGATTClients[identifier];
  if (!clients) {
    return NO;
  }
  NSUInteger activeCount = 0;
  @autoreleasepool {
    activeCount = clients.allObjects.count;
  }
  if (activeCount == 0) {
    [_activeGATTClients removeObjectForKey:identifier];
    return NO;
  }
  return YES;
}

- (BOOL)removeGATTClient:(nullable GNCBLEGATTClient *)client forIdentifier:(NSUUID *)identifier {
  dispatch_assert_queue(_queue);
  if (!client) {
    return NO;
  }
  NSHashTable<GNCBLEGATTClient *> *clients = _activeGATTClients[identifier];
  if (![clients containsObject:client]) {
    return NO;
  }
  [clients removeObject:client];
  @autoreleasepool {
    if (clients.allObjects.count == 0) {
      [_activeGATTClients removeObjectForKey:identifier];
    }
  }
  return YES;
}

- (void)cancelPeripheralConnectionIfUnused:(id<GNCPeripheral>)peripheral {
  dispatch_assert_queue(_queue);
  NSUUID *identifier = peripheral.identifier;
  if (!identifier) {
    return;
  }
  BOOL hasGATT = [self hasActiveGATTClientsForIdentifier:identifier] ||
                 _gattConnectionCompletionHandlers[identifier] != nil ||
                 _queuedGATTConnectionAttempts[identifier] != nil;
  BOOL hasL2CAP = _l2capClients[identifier].count > 0 || _l2capOpenAttempts[identifier] != nil ||
                  _l2capStreamCompletionHandlers[identifier] != nil;
  if (!hasGATT && !hasL2CAP) {
    [self cancelPeripheralConnectionForPeripheral:peripheral];
  }
}

- (void)ensureDelegateMultiplexerForPeripheral:(id<GNCPeripheral>)peripheral {
  dispatch_assert_queue(_queue);
  NSUUID *identifier = peripheral.identifier;
  if (!identifier) {
    return;
  }
  GNCPeripheralDelegateMultiplexer *multiplexer = _peripheralDelegateMultiplexers[identifier];
  if (!multiplexer) {
    multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
    _peripheralDelegateMultiplexers[identifier] = multiplexer;
  }
  if (peripheral.peripheralDelegate != multiplexer) {
    peripheral.peripheralDelegate = multiplexer;
  }
}

- (void)internalOpenL2CAPChannel:(id<GNCPeripheral>)remotePeripheral {
  dispatch_assert_queue(_queue);

  NSUUID *identifier = remotePeripheral.identifier;
  if (!identifier) {
    return;
  }
  GNCL2CAPChannelOpenAttempt *attempt = _l2capOpenAttempts[identifier];
  if (!attempt || attempt.channelOpenStarted) {
    return;
  }
  [_l2capPSMs removeObjectForKey:identifier];

  GNCBLEL2CAPClient *l2capClient = attempt.activeClient ?: _l2capClient;
  if (!l2capClient) {
    [self cancelConnectionTimeoutInMap:_l2capConnectionTimeoutBlocks forIdentifier:identifier];
    [self takeL2CAPOpenAttemptForIdentifier:identifier];
    [self cancelPeripheralConnectionIfUnused:remotePeripheral];
    if (attempt.completionHandler) {
      attempt.completionHandler(nil, [NSError errorWithDomain:GNCBLEErrorDomain
                                                         code:GNCBLEErrorUnknown
                                                     userInfo:nil]);
    }
    return;
  }

  attempt.channelOpenStarted = YES;
  [self ensureDelegateMultiplexerForPeripheral:remotePeripheral];
  __weak __typeof__(self) weakSelf = self;
  __weak GNCL2CAPChannelOpenAttempt *weakAttempt = attempt;
  dispatch_queue_t queue = _queue;
  [l2capClient
      openL2CAPChannelWithPSM:attempt.psm
                   peripheral:remotePeripheral
            completionHandler:^(GNCBLEL2CAPStream *_Nullable stream, NSError *_Nullable error) {
              dispatch_async(queue, ^{
                __strong __typeof__(self) strongSelf = weakSelf;
                GNCL2CAPChannelOpenAttempt *strongAttempt = weakAttempt;
                if (!strongSelf || !strongAttempt ||
                    strongSelf->_l2capOpenAttempts[identifier] != strongAttempt) {
                  if (stream) {
                    [stream tearDown];
                  }
                  return;
                }
                [strongSelf cancelConnectionTimeoutInMap:strongSelf->_l2capConnectionTimeoutBlocks
                                           forIdentifier:identifier];
                [strongSelf->_l2capOpenAttempts removeObjectForKey:identifier];
                [strongSelf->_l2capStreamCompletionHandlers removeObjectForKey:identifier];
                if (error || !stream) {
                  [strongSelf removeL2CAPClient:strongAttempt.ownedClient forIdentifier:identifier];
                  [strongSelf cancelPeripheralConnectionIfUnused:remotePeripheral];
                }
                if (strongAttempt.completionHandler) {
                  strongAttempt.completionHandler(stream, error);
                }
              });
            }];
}

- (void)cancelPeripheralConnectionForPeripheral:(id<GNCPeripheral>)peripheral {
  dispatch_assert_queue(_queue);
  if (!peripheral) {
    return;
  }
  if ([(id)peripheral isKindOfClass:[CBPeripheral class]] &&
      ((CBPeripheral *)peripheral).state == CBPeripheralStateDisconnected) {
    return;
  }
  if ([(id)_centralManager isKindOfClass:[CBCentralManager class]] &&
      _centralManager.state != CBManagerStatePoweredOn) {
    return;
  }
  if (_centralManager.state != CBManagerStatePoweredOn &&
      _centralManager.state != CBManagerStateUnknown) {
    return;
  }
  NSUUID *identifier = peripheral.identifier;
  if (!identifier) {
    [_centralManager cancelPeripheralConnection:peripheral];
    return;
  }
  if ([_cancelingPeripheralIdentifiers containsObject:identifier]) {
    return;
  }
  [_cancelingPeripheralIdentifiers addObject:identifier];
  [_centralManager cancelPeripheralConnection:peripheral];
}

- (void)handleCentralManagerUnavailable {
  dispatch_assert_queue(_queue);
  [self cancelAllConnectionTimeouts];

  NSMutableSet<NSUUID *> *identifiers =
      [NSMutableSet setWithSet:_cancelingPeripheralIdentifiers];
  [identifiers addObjectsFromArray:_queuedConnectPeripherals.allKeys];
  [identifiers addObjectsFromArray:_queuedGATTConnectionAttempts.allKeys];
  [identifiers addObjectsFromArray:_gattConnectionCompletionHandlers.allKeys];
  [identifiers addObjectsFromArray:_gattDisconnectionHandlers.allKeys];
  [identifiers addObjectsFromArray:_l2capOpenAttempts.allKeys];
  [identifiers addObjectsFromArray:_l2capStreamCompletionHandlers.allKeys];
  [identifiers addObjectsFromArray:_l2capPSMs.allKeys];
  [identifiers addObjectsFromArray:_l2capClients.allKeys];
  [identifiers addObjectsFromArray:_peripheralDelegateMultiplexers.allKeys];
  [identifiers addObjectsFromArray:_activeGATTClients.allKeys];

  NSDictionary<NSUUID *, GNCQueuedGATTConnectionAttempt *> *queuedGATTAttempts =
      [_queuedGATTConnectionAttempts copy];
  [_cancelingPeripheralIdentifiers removeAllObjects];
  [_queuedConnectPeripherals removeAllObjects];
  [_queuedGATTConnectionAttempts removeAllObjects];

  NSError *error = [NSError errorWithDomain:GNCBLEErrorDomain
                                       code:GNCBLEErrorUnknown
                                   userInfo:nil];
  for (NSUUID *identifier in identifiers) {
    GNCQueuedGATTConnectionAttempt *queuedGATT = queuedGATTAttempts[identifier];
    if (queuedGATT) {
      queuedGATT.completionHandler(nil, error);
    }
    [self handleDisconnectionForIdentifier:identifier
                                     error:error
                             isLocalCancel:NO
                invokeDisconnectionHandler:YES
                          useFallbackError:YES];
  }
}

- (void)handleDisconnectionForIdentifier:(NSUUID *)identifier
                                   error:(nullable NSError *)error
                           isLocalCancel:(BOOL)isLocalCancel
              invokeDisconnectionHandler:(BOOL)invokeDisconnectionHandler
                        useFallbackError:(BOOL)useFallbackError {
  dispatch_assert_queue(_queue);
  if (isLocalCancel) {
    [_cancelingPeripheralIdentifiers removeObject:identifier];
  }

  GNCQueuedGATTConnectionAttempt *queuedGATT =
      isLocalCancel ? _queuedGATTConnectionAttempts[identifier] : nil;
  [_queuedGATTConnectionAttempts removeObjectForKey:identifier];
  if (!queuedGATT) {
    [self cancelConnectionTimeoutInMap:_gattConnectionTimeoutBlocks forIdentifier:identifier];
  }

  GNCGATTConnectionCompletionHandler priorGattHandler =
      _gattConnectionCompletionHandlers[identifier];
  [_gattConnectionCompletionHandlers removeObjectForKey:identifier];

  GNCL2CAPChannelOpenAttempt *currentL2CAPAttempt = _l2capOpenAttempts[identifier];
  BOOL hasQueuedL2CAPAttempt =
      isLocalCancel && currentL2CAPAttempt != nil && currentL2CAPAttempt.waitingForDisconnect;
  GNCOpenL2CAPStreamCompletionHandler priorL2CAPHandler = nil;
  if (hasQueuedL2CAPAttempt) {
    NSSet<GNCBLEL2CAPClient *> *clientsToDisconnect = [_l2capClients[identifier] copy];
    for (GNCBLEL2CAPClient *client in clientsToDisconnect) {
      if (client != currentL2CAPAttempt.ownedClient) {
        [self removeL2CAPClient:client forIdentifier:identifier];
        [client disconnect];
      }
    }
  } else {
    [self cancelConnectionTimeoutInMap:_l2capConnectionTimeoutBlocks forIdentifier:identifier];
    GNCOpenL2CAPStreamCompletionHandler fallbackL2CAPHandler =
        _l2capStreamCompletionHandlers[identifier];
    GNCL2CAPChannelOpenAttempt *priorL2CAPAttempt =
        [self takeL2CAPOpenAttemptForIdentifier:identifier];
    NSSet<GNCBLEL2CAPClient *> *remainingClients = [_l2capClients[identifier] copy];
    [_l2capClients removeObjectForKey:identifier];
    for (GNCBLEL2CAPClient *client in remainingClients) {
      [client disconnect];
    }
    priorL2CAPHandler =
        priorL2CAPAttempt ? priorL2CAPAttempt.completionHandler : fallbackL2CAPHandler;
  }

  [_activeGATTClients removeObjectForKey:identifier];
  [_peripheralDelegateMultiplexers removeObjectForKey:identifier];

  GNCGATTDisconnectionHandler disconnectionHandler = _gattDisconnectionHandlers[identifier];
  [_gattDisconnectionHandlers removeObjectForKey:identifier];

  id<GNCPeripheral> queuedPeripheral = isLocalCancel ? _queuedConnectPeripherals[identifier] : nil;
  [_queuedConnectPeripherals removeObjectForKey:identifier];

  if (priorGattHandler || priorL2CAPHandler) {
    NSError *callbackError = error
                                 ?: (useFallbackError ? [NSError errorWithDomain:GNCBLEErrorDomain
                                                                            code:GNCBLEErrorUnknown
                                                                        userInfo:nil]
                                                      : nil);
    if (priorGattHandler) {
      priorGattHandler(nil, callbackError);
    }
    if (priorL2CAPHandler) {
      priorL2CAPHandler(nil, callbackError);
    }
  }
  if (invokeDisconnectionHandler && disconnectionHandler) {
    disconnectionHandler();
  }

  if (isLocalCancel && queuedPeripheral) {
    if (queuedGATT) {
      if (queuedGATT.disconnectionHandler) {
        _gattDisconnectionHandlers[identifier] = queuedGATT.disconnectionHandler;
      }
      _gattConnectionCompletionHandlers[identifier] = queuedGATT.completionHandler;
    }
    if (hasQueuedL2CAPAttempt) {
      currentL2CAPAttempt.waitingForDisconnect = NO;
    }
    [_centralManager connectPeripheral:queuedPeripheral options:@{}];
  }
}

#pragma mark - GNCCentralManagerDelegate

- (void)gnc_centralManagerDidUpdateState:(id<GNCCentralManager>)central {
  if (!_queue) {
    return;
  }
  dispatch_assert_queue(_queue);
  if (central.state != CBManagerStatePoweredOn && central.state != CBManagerStateUnknown) {
    [self handleCentralManagerUnavailable];
  }
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
  if (!identifier) {
    return;
  }
  if ([_cancelingPeripheralIdentifiers containsObject:identifier]) {
    GNCLoggerDebug(@"Ignoring didConnectPeripheral for %@ while cancel is in flight.", identifier);
    return;
  }

  [self cancelConnectionTimeoutInMap:_gattConnectionTimeoutBlocks forIdentifier:identifier];

  GNCGATTConnectionCompletionHandler handler = _gattConnectionCompletionHandlers[identifier];
  if (handler) {
    [_gattConnectionCompletionHandlers removeObjectForKey:identifier];
    [self ensureDelegateMultiplexerForPeripheral:peripheral];

    __weak __typeof__(self) weakSelf = self;
    dispatch_queue_t queue = _queue;
    __block __weak GNCBLEGATTClient *weakClient = nil;
    __block BOOL didRequestDisconnection = NO;
    GNCBLEGATTClient *client = [[GNCBLEGATTClient alloc]
                 initWithPeripheral:peripheral
        requestDisconnectionHandler:^(id<GNCPeripheral> clientPeripheral) {
          __strong GNCBLEGATTClient *strongClient = weakClient;
          dispatch_async(queue, ^{
            if (didRequestDisconnection) {
              return;
            }
            didRequestDisconnection = YES;
            __strong __typeof__(self) strongSelf = weakSelf;
            if (!strongSelf || !clientPeripheral) {
              return;
            }
            NSUUID *clientIdentifier = clientPeripheral.identifier ?: identifier;
            if ([strongSelf removeGATTClient:strongClient forIdentifier:clientIdentifier]) {
              [strongSelf cancelPeripheralConnectionIfUnused:clientPeripheral];
            }
          });
        }];
    weakClient = client;
    NSHashTable<GNCBLEGATTClient *> *clients = _activeGATTClients[identifier];
    if (!clients) {
      clients = [NSHashTable weakObjectsHashTable];
      _activeGATTClients[identifier] = clients;
    }
    [clients addObject:client];
    handler(client, nil);
  }

  GNCL2CAPChannelOpenAttempt *attempt = _l2capOpenAttempts[identifier];
  if (attempt && !attempt.channelOpenStarted) {
    [self internalOpenL2CAPChannel:peripheral];
  }
}

- (void)gnc_centralManager:(id<GNCCentralManager>)central
    didFailToConnectPeripheral:(id<GNCPeripheral>)peripheral
                         error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  NSUUID *identifier = peripheral.identifier;
  if (!identifier) {
    return;
  }
  if ([_cancelingPeripheralIdentifiers containsObject:identifier]) {
    [self handleDisconnectionForIdentifier:identifier
                                     error:error
                             isLocalCancel:YES
                invokeDisconnectionHandler:NO
                          useFallbackError:NO];
    return;
  }
  [self cancelConnectionTimeoutInMap:_gattConnectionTimeoutBlocks forIdentifier:identifier];
  [self cancelConnectionTimeoutInMap:_l2capConnectionTimeoutBlocks forIdentifier:identifier];
  [_activeGATTClients removeObjectForKey:identifier];

  GNCGATTConnectionCompletionHandler gattHandler = _gattConnectionCompletionHandlers[identifier];
  [_gattConnectionCompletionHandlers removeObjectForKey:identifier];
  [_gattDisconnectionHandlers removeObjectForKey:identifier];
  if (gattHandler) {
    gattHandler(nil, error);
  }

  GNCOpenL2CAPStreamCompletionHandler fallbackL2CAPHandler =
      _l2capStreamCompletionHandlers[identifier];
  GNCL2CAPChannelOpenAttempt *l2capAttempt = [self takeL2CAPOpenAttemptForIdentifier:identifier];
  NSSet<GNCBLEL2CAPClient *> *remainingClients = [_l2capClients[identifier] copy];
  [_l2capClients removeObjectForKey:identifier];
  for (GNCBLEL2CAPClient *client in remainingClients) {
    [client disconnect];
  }
  GNCOpenL2CAPStreamCompletionHandler l2capHandler =
      l2capAttempt ? l2capAttempt.completionHandler : fallbackL2CAPHandler;
  if (l2capHandler) {
    l2capHandler(nil, error);
  }
}

- (void)gnc_centralManager:(id<GNCCentralManager>)central
    didDisconnectPeripheral:(id<GNCPeripheral>)peripheral
                      error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);

  NSUUID *identifier = peripheral.identifier;
  if (!identifier) {
    return;
  }
  BOOL isLocalCancel = [_cancelingPeripheralIdentifiers containsObject:identifier];
  [self handleDisconnectionForIdentifier:identifier
                                   error:error
                           isLocalCancel:isLocalCancel
              invokeDisconnectionHandler:YES
                        useFallbackError:YES];
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
