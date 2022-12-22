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

#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleCentral.h"

#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleConnection.h"
#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleUtils.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralManager.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralPeerManager.h"
#import "internal/platform/implementation/apple/Mediums/GNCMConnection.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, GNCMCentralState) {
  GNCMCentralStateStopped,
  GNCMCentralStateScanning,
};

typedef void (^GNCMBleCharacteristicsHandler)(NSArray<CBCharacteristic *> *characteristics,
                                              NSError *error);
typedef void (^GNCMBleCharacteristicValueHandler)(CBCharacteristic *characteristic, NSError *error);
typedef void (^GNCIntHandler)(int);

/** This lets a GNCIntHandler call itself. */
GNCIntHandler GNCRecursiveIntHandler(void (^block)(GNCIntHandler blockSelf, int i)) {
  return ^(int i) {
    return block(GNCRecursiveIntHandler(block), i);
  };
}

/** This represents a discovered peripheral. */
@interface GNCMPeripheralInfo : NSObject
@property(nonatomic) CBPeripheral *peripheral;
@property(nonatomic, copy) NSDictionary<NSString *, id> *advertisementData;

/** Called when characteristics are discovered. */
@property(nonatomic, nullable) GNCMBleCharacteristicsHandler charsHandler;

/** Called when a characteristic value is read. */
@property(nonatomic, nullable) GNCMBleCharacteristicValueHandler charValueHandler;

@end

@implementation GNCMPeripheralInfo

- (instancetype)initWithPeripheral:(CBPeripheral *)peripheral
                 advertisementData:(NSDictionary<NSString *, id> *)advertisementData {
  self = [super init];
  if (self) {
    _peripheral = peripheral;
    _advertisementData = [advertisementData copy];
  }
  return self;
}

- (BOOL)isEqual:(id)object {
  // There is always exactly one info object per peripheral, so compare by identity. This is needed
  // for maintenance of the peripherals stored in multiple maps.
  return self == object;
}

- (NSUInteger)hash {
  return (NSUInteger)self;
}

@end

@interface GNCMBleCentral () <CBCentralManagerDelegate,
                              CBPeripheralDelegate,
                              GNSCentralManagerDelegate>
@end

@implementation GNCMBleCentral {
  /** Service UUID the central is scanning for. */
  CBUUID *_serviceUUID;
  /** The scan result callback handler. */
  GNCMScanResultHandler _scanResultHandler;
  /** Central manager used to scan or connect to peripherals. */
  CBCentralManager *_centralManager;
  /** Serial background queue for |centralManager|. */
  dispatch_queue_t _selfQueue;
  /** Central state for stop or scanning. */
  GNCMCentralState _centralState;
  /** The dictionary keyed by CBPeripheral identifier to value GNCMPeripheralInfo. */
  NSMutableDictionary<NSUUID *, GNCMPeripheralInfo *> *_nearbyPeripheralsByID;
  /** Array of characteristic UUID used for discovering. */
  NSArray<CBUUID *> *_characteristicUUIDs;
  /** GATT connection result handler. */
  GNCMGATTConnectionResultHandler _gattConnectionResultHandler;
  /** GATT service and characteristic discovery result hanadler. */
  GNCMGATTDiscoverResultHandler _gattDiscoverResultHandler;
  /** The discovered characteristic values map used to callback for `_gattDiscoverResultHandler`. */
  NSMutableOrderedSet<CBCharacteristic *> *_gattCharacteristicValues;
  /** Central manager used for socket connection based on weave protocol. */
  GNSCentralManager *_socketCentralManager;
  /** A callback handler to reuqest connection on the discovered advertiser. */
  GNCMBleRequestConnectionHandler _requestConnectionHandler;
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
    _selfQueue = dispatch_queue_create("GNCCentralManagerQueue", DISPATCH_QUEUE_SERIAL);

    _nearbyPeripheralsByID = [NSMutableDictionary dictionary];
    _gattCharacteristicValues = [[NSMutableOrderedSet alloc] init];

    _centralState = GNCMCentralStateStopped;
  }
  return self;
}

- (void)dealloc {
  // These calls must be made on |selfQueue|.  Can't capture |self| in an async block, so must use
  // dispatch_sync. This means dealloc must be called from an external queue, which means |self|
  // must never be captured by any escaping block used in this class.
  dispatch_sync(_selfQueue, ^{
    [self stopScanningInternal];
    [_socketCentralManager stopNoScanMode];

    _callbacksEnabled = NO;
  });
}

- (BOOL)startScanningWithServiceUUID:(NSString *)serviceUUID
                   scanResultHandler:(GNCMScanResultHandler)scanResultHandler
            requestConnectionHandler:(GNCMBleRequestConnectionHandler)requestConnectionHandler
                       callbackQueue:(nullable dispatch_queue_t)callbackQueue {
  NSLog(@"[NEARBY] Client rquests startScanning");
  _serviceUUID = [CBUUID UUIDWithString:serviceUUID];
  _scanResultHandler = scanResultHandler;
  _requestConnectionHandler = requestConnectionHandler;

  // The client may be using the callback queue for other purposes, so wrap it with a private
  // queue to know with certainty when all callbacks are done.
  _clientCallbackQueue = callbackQueue ?: dispatch_get_main_queue();
  _internalCallbackQueue =
      dispatch_queue_create("GNCMBleCentralCallbackQueue", DISPATCH_QUEUE_PRIORITY_DEFAULT);
  _callbacksEnabled = YES;

  // Set up the central manager for scanning.
  _centralManager =
      [[CBCentralManager alloc] initWithDelegate:self
                                           queue:_selfQueue
                                         options:@{CBCentralManagerOptionShowPowerAlertKey : @NO}];

  // Set up the central manager for the socket.
  _socketCentralManager = [[GNSCentralManager alloc] initWithSocketServiceUUID:_serviceUUID
                                                                         queue:_selfQueue];
  _socketCentralManager.delegate = self;
  [_socketCentralManager startNoScanModeWithAdvertisedServiceUUIDs:@[ _serviceUUID ]];

  _centralState = GNCMCentralStateScanning;
  return YES;
}

- (void)connectGattServerWithPeripheralID:(NSString *)peripheralID
              gattConnectionResultHandler:
                  (GNCMGATTConnectionResultHandler)gattConnectionResultHandler {
  _gattConnectionResultHandler = gattConnectionResultHandler;
  dispatch_sync(_selfQueue, ^{
    GNCMPeripheralInfo *peripheralInfo =
        _nearbyPeripheralsByID[[[NSUUID alloc] initWithUUIDString:peripheralID]];
    if (!peripheralInfo) return;
    [_centralManager connectPeripheral:peripheralInfo.peripheral options:nil];
  });
}

- (void)discoverGattService:(CBUUID *)serviceUUID
          gattCharacteristics:(NSArray<CBUUID *> *)characteristicUUIDs
                 peripheralID:(NSString *)peripheralID
    gattDiscoverResultHandler:(GNCMGATTDiscoverResultHandler)gattDiscoverResultHandler {
  _gattDiscoverResultHandler = gattDiscoverResultHandler;
  [_gattCharacteristicValues removeAllObjects];
  dispatch_sync(_selfQueue, ^{
    GNCMPeripheralInfo *peripheralInfo =
        _nearbyPeripheralsByID[[[NSUUID alloc] initWithUUIDString:peripheralID]];
    if (!peripheralInfo) return;
    _characteristicUUIDs = [characteristicUUIDs copy];

    // Start to discover service and the delegate will get its characteristics and read their values
    // recursively
    [peripheralInfo.peripheral discoverServices:@[ serviceUUID ]];
  });
}

- (void)disconnectGattServiceWithPeripheralID:(NSString *)peripheralID {
  dispatch_sync(_selfQueue, ^{
    GNCMPeripheralInfo *peripheralInfo =
        _nearbyPeripheralsByID[[[NSUUID alloc] initWithUUIDString:peripheralID]];
    if (!peripheralInfo) return;
    _gattConnectionResultHandler = nil;
    _gattDiscoverResultHandler = nil;
    [_centralManager cancelPeripheralConnection:peripheralInfo.peripheral];
  });
}

#pragma mark CBCentralManagerDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
  if (central.state == CBManagerStatePoweredOn && !central.isScanning &&
      _centralState == GNCMCentralStateScanning) {
    NSLog(@"[NEARBY] CBCentralManager powered on; starting scan");
    [self startScanningInternal];
  } else {
    NSLog(@"[NEARBY] CBCentralManager not powered on; stopping scan");
    [self stopScanningInternal];
  }
}

- (void)centralManager:(CBCentralManager *)central
    didDiscoverPeripheral:(CBPeripheral *)peripheral
        advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                     RSSI:(NSNumber *)RSSI {
  NSNumber *connectable = advertisementData[CBAdvertisementDataIsConnectable];
  if (![connectable boolValue]) return;

  // Look for the NC advertisement header in either the service data (from non-iOS) or the
  // advertised name (from iOS).
  NSData *serviceData = advertisementData[CBAdvertisementDataServiceDataKey][_serviceUUID]
                            ?: advertisementData[CBAdvertisementDataLocalNameKey];

  // Try to look up the peripheral by ID.
  GNCMPeripheralInfo *info = _nearbyPeripheralsByID[peripheral.identifier];
  if (!info) {
    NSLog(@"[NEARBY] New peripheral: %@", peripheral);
    // This is a new peripheral, so create a new peripheral info object.
    info = [[GNCMPeripheralInfo alloc] initWithPeripheral:peripheral
                                        advertisementData:advertisementData];
  } else {
    info.peripheral = peripheral;
  }

  _nearbyPeripheralsByID[peripheral.identifier] = info;
  _scanResultHandler(peripheral.identifier.UUIDString, serviceData);
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {
  NSLog(@"[NEARBY] Connected to peripheral: %@", peripheral);
  peripheral.delegate = self;
  // Tell the caller the connection is done.
  _gattConnectionResultHandler(nil);
}

- (void)centralManager:(CBCentralManager *)central
    didFailToConnectPeripheral:(CBPeripheral *)peripheral
                         error:(nullable NSError *)error {
  NSLog(@"[NEARBY] Failed to connect to peripheral: %@, error: %@", peripheral, error);
  // Tell the caller the connection failed.
  _gattConnectionResultHandler(error);
}

- (void)centralManager:(CBCentralManager *)central
    didDisconnectPeripheral:(CBPeripheral *)peripheral
                      error:(nullable NSError *)error {
  NSLog(@"[NEARBY] Disconnected to peripheral: %@, error: %@", peripheral, error);
}

#pragma mark CBPeripheralDelegate

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(nullable NSError *)error {
  GNCMPeripheralInfo *peripheralInfo = _nearbyPeripheralsByID[peripheral.identifier];
  if (!peripheralInfo) return;
  if (error || (peripheral.services.count == 0)) {
    NSLog(@"[NEARBY] Error reading advertisement: unable to discover services.");
    _gattDiscoverResultHandler(nil);
  } else {
    NSLog(@"[NEARBY] Discovered services for %@: %@", peripheral.name, peripheral.services);

    // Helper functions for discovering characteristics and reading their values.
    void (^discoverChars)(CBService *, GNCMBleCharacteristicsHandler) =
        ^(CBService *service, GNCMBleCharacteristicsHandler handler) {
          NSAssert(!peripheralInfo.charsHandler, @"Unexpected characteristic handler");
          peripheralInfo.charsHandler = handler;

          // Discover all characteristics that may contain the advertisement.
          [peripheral discoverCharacteristics:_characteristicUUIDs forService:service];
        };
    void (^readCharValue)(CBCharacteristic *, GNCMBleCharacteristicValueHandler) =
        ^(CBCharacteristic *characteristic, GNCMBleCharacteristicValueHandler handler) {
          NSAssert(!peripheralInfo.charValueHandler, @"Unexpected characteristic value handler");
          peripheralInfo.charValueHandler = handler;
          [peripheral readValueForCharacteristic:characteristic];
        };

    // Multiple services may have the same UUID, so find the right service by searching for the
    // characteristic containing the advertisement with a matching service ID hash.
    __weak __typeof__(self) weakSelf = self;
    void (^tryService)(int) = GNCRecursiveIntHandler(^(GNCIntHandler tryService, int serviceIndex) {
      __strong __typeof__(self) strongSelf = weakSelf;
      if (!strongSelf) return;

      // We've tried all the services, report the discovered characteristics and their values.
      if (serviceIndex == peripheral.services.count) {
        NSLog(@"[NEARBY] Done traversing all services or no services to traverse.");
        _gattDiscoverResultHandler(_gattCharacteristicValues);
        if (_gattCharacteristicValues.count > 0) {
          CBCharacteristic *characteristic = [_gattCharacteristicValues objectAtIndex:0];
          [strongSelf completeGATTReadWithData:characteristic.value
                             forPeripheralInfo:peripheralInfo];
        }
        return;
      }

      NSLog(@"[NEARBY] Trying service: %@", peripheral.services[serviceIndex]);
      discoverChars(
          peripheral.services[serviceIndex], ^(NSArray<CBCharacteristic *> *chars, NSError *error) {
            void (^tryNextService)() = ^{
              tryService(serviceIndex + 1);
            };

            // If there was an error or there are no characteristics on this service, try next one.
            if (error || (chars.count == 0)) {
              tryNextService();
              return;
            }

            // Read each characteristic.
            void (^tryChar)(int) = GNCRecursiveIntHandler(^(GNCIntHandler tryChar, int charIndex) {
              // We've tried all characteristics on this service without error, try next service.
              if (charIndex == chars.count) {
                NSLog(@"[NEARBY] No matching advertisement found");
                tryNextService();
                return;
              }

              NSLog(@"[NEARBY] Trying characteristic: %@", chars[charIndex]);
              readCharValue(chars[charIndex], ^(CBCharacteristic *characteristic, NSError *error) {
                if (error) {
                  tryNextService();
                } else {
                  // We've found the characteristic and its non-nil value. Store it.
                  if (characteristic.value.length != 0) {
                    [_gattCharacteristicValues addObject:characteristic];
                  }
                  tryChar(charIndex + 1);
                }
              });
            });

            // Start searching the characteristics for the current service.
            tryChar(0);
          });
    });

    // Start searching the services.
    tryService(0);
  }
}

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(nullable NSError *)error {
  NSLog(@"[NEARBY] Discovered characteristics: %@ error: %@", service.characteristics, error);
  GNCMPeripheralInfo *peripheralInfo = _nearbyPeripheralsByID[peripheral.identifier];
  if (!peripheralInfo) return;
  GNCMBleCharacteristicsHandler charsHandler = peripheralInfo.charsHandler;
  peripheralInfo.charsHandler = nil;
  charsHandler(service.characteristics, error);
}

- (void)peripheral:(CBPeripheral *)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(nullable NSError *)error {
  NSLog(@"[NEARBY] Read characteristic value: %@ error: %@", characteristic, error);
  GNCMPeripheralInfo *peripheralInfo = _nearbyPeripheralsByID[peripheral.identifier];
  if (!peripheralInfo) return;
  GNCMBleCharacteristicValueHandler valueHandler = peripheralInfo.charValueHandler;
  peripheralInfo.charValueHandler = nil;
  valueHandler(characteristic, error);
}

#pragma mark GNSCentralManagerDelegate

- (void)centralManager:(GNSCentralManager *)centralManager
       didDiscoverPeer:(GNSCentralPeerManager *)centralPeerManager
     advertisementData:(nullable NSDictionary<NSString *, id> *)advertisementData {
  if (!_callbacksEnabled) return;

  // Retrieve the cached peripheral info.
  NSUUID *peerId = centralPeerManager.identifier;
  GNCMPeripheralInfo *peripheralInfo = _nearbyPeripheralsByID[peerId];
  if (!peripheralInfo) return;

  __weak __typeof__(self) weakSelf = self;
  [self callbackAsync:^{
    _requestConnectionHandler(^(NSString *serviceID, GNCMConnectionHandler connectionHandler) {
      __strong __typeof__(self) strongSelf = weakSelf;
      if (!strongSelf) return;

      void (^callConnectionHandler)(GNSSocket *__nullable) = ^(GNSSocket *__nullable socket) {
        __strong __typeof__(self) strongSelf = weakSelf;
        if (!strongSelf->_callbacksEnabled) return;
        [strongSelf callbackAsync:^{
          if (!socket) {
            NSLog(@"[NEARBY] Central failed to create BLE socket");
            connectionHandler(nil);
          } else {
            GNCMBleConnection *connection =
                [GNCMBleConnection connectionWithSocket:socket
                                              serviceID:serviceID
                                    expectedIntroPacket:NO
                                          callbackQueue:strongSelf->_clientCallbackQueue];
            connection.connectionHandlers = connectionHandler(connection);
          }
        }];
      };

      dispatch_async(strongSelf->_selfQueue, ^{
        // A connection is being requested, so establish a BLE socket. Make sure to use the most
        // up-to-date GNSCentralPeerManager in case the MAC address rotated. The cached
        // peripheral info should be the single source of truth for which MAC address to use.
        GNSCentralPeerManager *updatedCentralPeerManager =
            [centralManager retrieveCentralPeerWithIdentifier:peripheralInfo.peripheral.identifier];
        if (!updatedCentralPeerManager) {
          callConnectionHandler(nil);
        }

        // Make a socket connection.
        [updatedCentralPeerManager
            socketWithPairingCharacteristic:NO
                                 completion:^(GNSSocket *socket, NSError *error) {
                                   __strong __typeof__(self) strongSelf = weakSelf;
                                   if (!strongSelf) return;
                                   dispatch_async(strongSelf->_selfQueue, ^{
                                     if (!error) {
                                       // Call the connection handler when the socket has
                                       // connected or fails to connect.
                                       GNCMWaitForConnection(socket, ^(BOOL didConnect) {
                                         callConnectionHandler(didConnect ? socket : nil);
                                       });
                                     } else {
                                       callConnectionHandler(nil);
                                     }
                                   });
                                 }];
      });
    });
  }];
}

- (void)centralManagerDidUpdateBleState:(GNSCentralManager *)centralManager {
  // No op.
}

#pragma mark Private

/** This method assumes it's being called on selfQueue. */
- (void)completeGATTReadWithData:(NSData *)advertisement
               forPeripheralInfo:(GNCMPeripheralInfo *)peripheralInfo {
  NSAssert(peripheralInfo, @"Nil peripheralInfo");
  if (peripheralInfo) {
    CBPeripheral *peripheral = peripheralInfo.peripheral;
    NSUUID *peripheralID = peripheral.identifier;
    // Store the data for the socket callback, and report the peripheral to the socket library.
    [_socketCentralManager retrievePeripheralWithIdentifier:peripheralID
                                          advertisementData:peripheralInfo.advertisementData];
  }
}

/** Signals the central manager to start scanning. Must be called on _selfQueue. */
- (void)startScanningInternal {
  if (![_centralManager isScanning]) {
    NSLog(@"[NEARBY] startScanningInternal");

    [_centralManager
        scanForPeripheralsWithServices:@[ _serviceUUID ]
                               options:@{CBCentralManagerScanOptionAllowDuplicatesKey : @YES}];
  }
}

/** Signals the central manager to stop scanning. Must be called on _selfQueue */
- (void)stopScanningInternal {
  if ([_centralManager isScanning]) {
    NSLog(@"[NEARBY] stopScanningInternal");
    _centralState = GNCMCentralStateStopped;

    [_centralManager stopScan];
  }
}

/** Calls the specified block on the callback queue. */
- (void)callbackAsync:(dispatch_block_t)block {
  dispatch_queue_t clientCallbackQueue = _clientCallbackQueue;  // don't capture |self|
  dispatch_async(_internalCallbackQueue, ^{
    dispatch_sync(clientCallbackQueue, block);
  });
}

@end

NS_ASSUME_NONNULL_END
