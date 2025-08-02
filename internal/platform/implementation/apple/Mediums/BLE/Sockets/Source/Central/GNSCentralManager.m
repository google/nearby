// Copyright 2020 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Central/GNSCentralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Central/GNSCentralManager+Private.h"

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Central/GNSCentralPeerManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Central/GNSCentralPeerManager.h"

NS_ASSUME_NONNULL_BEGIN

static NSString *CBManagerStateString(CBManagerState state) {
  switch (state) {
    case CBManagerStateUnknown:
      return @"CBManagerStateUnknown";
    case CBManagerStateResetting:
      return @"CBManagerStateResetting";
    case CBManagerStateUnsupported:
      return @"CBManagerStateUnsupported";
    case CBManagerStateUnauthorized:
      return @"CBManagerStateUnauthorized";
    case CBManagerStatePoweredOff:
      return @"CBManagerStatePoweredOff";
    case CBManagerStatePoweredOn:
      return @"CBManagerStatePoweredOn";
  }
  return [NSString stringWithFormat:@"CBManagerState Unknown(%ld)", (long)state];
}

@interface GNSCentralManager () {
  NSString *_advertisedName;
  CBUUID *_advertisedServiceUUID;
  NSArray<CBUUID *> *_advertisedServiceUUIDs;
  CBCentralManager *_cbCentralManager;
  NSMapTable *_centralPeerManagers;
  BOOL _cbCentralScanStarted;
  dispatch_queue_t _queue;
}

@end

@implementation GNSCentralManager

+ (CBCentralManager *)centralManagerWithDelegate:(id<CBCentralManagerDelegate>)delegate
                                           queue:(nullable dispatch_queue_t)queue
                                         options:(nullable NSDictionary<NSString *, id> *)options {
  return [[CBCentralManager alloc] initWithDelegate:delegate queue:queue options:options];
}

- (instancetype)init {
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

- (nullable instancetype)initWithSocketServiceUUID:(CBUUID *)socketServiceUUID
                                             queue:(dispatch_queue_t)queue {
  NSAssert(socketServiceUUID, @"Cannot create a GNSCentralManager with nil service UUID.");
  self = [super init];
  if (self) {
    _socketServiceUUID = socketServiceUUID;
    _queue = queue;
    _cbCentralManager =
        [[self class] centralManagerWithDelegate:self
                                           queue:queue
                                         options:@{CBCentralManagerOptionShowPowerAlertKey : @NO}];
    _centralPeerManagers = [NSMapTable strongToWeakObjectsMapTable];
  }
  return self;
}

- (nullable instancetype)initWithSocketServiceUUID:(CBUUID *)socketServiceUUID {
  return [self initWithSocketServiceUUID:socketServiceUUID queue:dispatch_get_main_queue()];
}

- (void)dealloc {
  _cbCentralManager.delegate = nil;
}

- (void)startScanWithAdvertisedName:(nullable NSString *)advertisedName
              advertisedServiceUUID:(nullable CBUUID *)advertisedServiceUUID {
  NSAssert(_cbCentralManager, @"CBCentralManager not created.");
  if (_scanning) {
    return;
  }
  _advertisedServiceUUID = advertisedServiceUUID;
  _advertisedName = advertisedName;
  _scanning = YES;
  if (_cbCentralManager.state == CBManagerStatePoweredOn) {
    [self startCBScan];
  }
}

- (void)startScanWithAdvertisedServiceUUIDs:(nullable NSArray<CBUUID *> *)advertisedServiceUUIDs {
  NSAssert(_cbCentralManager, @"CBCentralManager not created.");
  if (_scanning) {
    return;
  }
  [self startNoScanModeWithAdvertisedServiceUUIDs:_advertisedServiceUUIDs];
  _scanning = YES;
  if (_cbCentralManager.state == CBManagerStatePoweredOn) {
    [self startCBScan];
  }
}

- (void)stopScan {
  if (!_scanning) {
    return;
  }
  _advertisedName = nil;
  if (_cbCentralScanStarted) {
    [self stopCBScan];
  }
  _scanning = NO;
  [self stopNoScanMode];
}

- (void)startNoScanModeWithAdvertisedServiceUUIDs:
    (nullable NSArray<CBUUID *> *)advertisedServiceUUIDs {
  _advertisedServiceUUIDs = [advertisedServiceUUIDs copy];
}

- (void)retrievePeripheralWithIdentifier:(NSUUID *)identifier
                       advertisementData:(nonnull NSDictionary<NSString *, id> *)advertisementData {
  NSArray<CBPeripheral *> *peripherals =
      [_cbCentralManager retrievePeripheralsWithIdentifiers:@[ identifier ]];
  if (peripherals.count > 0) {
    [self centralManager:_cbCentralManager
        didDiscoverPeripheral:peripherals[0]
            advertisementData:advertisementData
                         RSSI:@(127)];  // RSSI not available
  }
}

- (nullable CBPeripheral *)retrievePeripheralWithIdentifier:(NSUUID *)identifier {
  NSAssert(_cbCentralManager, @"CBCentralManager not created.");
  NSAssert(identifier, @"Should have an identifier, self: %@", self);

  NSArray<CBPeripheral *> *peripherals =
      [_cbCentralManager retrievePeripheralsWithIdentifiers:@[ identifier ]];

  for (CBPeripheral *peripheral in peripherals) {
    if ([peripheral.identifier isEqual:identifier]) {
      return peripheral;
    }
  }

  return nil;
}

- (void)stopNoScanMode {
  _advertisedServiceUUIDs = nil;
}

- (nullable GNSCentralPeerManager *)retrieveCentralPeerWithIdentifier:(NSUUID *)identifier {
  NSAssert(_cbCentralManager, @"CBCentralManager not created.");
  NSAssert(identifier, @"Should have an identifier, self: %@", self);
  GNSCentralPeerManager *peerManager = [_centralPeerManagers objectForKey:identifier];
  if (peerManager) {
    // GNCFATAL will cause tests terminating. Change to GNCLoggerError.
    GNCLoggerError(@"Previous GNSCentralPeerManager still alive, self: %@, peer manager: %@", self,
                   peerManager);
    return nil;
  }
  NSArray<CBPeripheral *> *peripherals =
      [_cbCentralManager retrievePeripheralsWithIdentifiers:@[ identifier ]];
  for (CBPeripheral *peripheral in peripherals) {
    if ([peripheral.identifier isEqual:identifier]) {
      peerManager = [self createCentralPeerManagerWithPeripheral:peripheral];
      [_centralPeerManagers setObject:peerManager forKey:identifier];
      return peerManager;
    }
  }
  GNCLoggerError(@"CBPeripheral not found, self: %@, identifier %@, peripherals %@", self,
                 identifier, peripherals);
  return nil;
}

- (CBManagerState)cbManagerState {
  return _cbCentralManager.state;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"<%@: %p, socket service UUID: %@, advertised name: %@, "
                                    @"scanning: %@, central scan started: %@, bluetooth state %@, "
                                    @"CBCentralManager %@, peer managers: %@>",
                                    NSStringFromClass([self class]), self, _socketServiceUUID,
                                    _advertisedName, _scanning ? @"YES" : @"NO",
                                    _cbCentralScanStarted ? @"YES" : @"NO",
                                    CBManagerStateString([self cbManagerState]), _cbCentralManager,
                                    _centralPeerManagers];
}

#pragma mark - Private

- (void)startCBScan {
  if (_cbCentralScanStarted) {
    return;
  }
  _cbCentralScanStarted = YES;
  // CBCentralManagerScanOptionAllowDuplicatesKey has to be set to YES. There are some cases where
  // CoreBluetooth never discovers the phone if the setup mode on the phone is enable after opening
  // the setup window on OS X.
  // The drawback is the same CBPeripheral is discovered several times per second.
  NSDictionary<NSString *, id> *options = @{CBCentralManagerScanOptionAllowDuplicatesKey : @YES};
  if (_advertisedName) {
    GNCLoggerInfo(@"Start scanning for all peripherals. Filter peripherals with advertised "
                   "service UUID %@ or advertised name %@.",
                  [_socketServiceUUID UUIDString], _advertisedName);
    [_cbCentralManager scanForPeripheralsWithServices:nil options:options];
  } else if (_advertisedServiceUUID) {
    // Not logging this case to avoid spamming the logs.
    [_cbCentralManager scanForPeripheralsWithServices:@[ _advertisedServiceUUID ] options:options];
  } else if (_advertisedServiceUUIDs) {
    // Not logging this case to avoid spamming the logs.
    [_cbCentralManager scanForPeripheralsWithServices:_advertisedServiceUUIDs options:options];
  } else {
    GNCLoggerInfo(@"Start scanning for all peripherals.");
    [_cbCentralManager scanForPeripheralsWithServices:nil options:options];
  }
}

- (void)stopCBScan {
  if (!_cbCentralScanStarted) {
    return;
  }
  _cbCentralScanStarted = NO;
  [_cbCentralManager stopScan];
}

- (void)peripheralDisconnected:(CBPeripheral *)peripheral withError:(NSError *)error {
  NSAssert(peripheral.state == CBPeripheralStateDisconnected,
           @"Peripheral should be disconnected %@", peripheral);
  GNSCentralPeerManager *peerManager = [self centralPeerForPeripheral:peripheral];
  if (peerManager) {
    [peerManager bleDisconnectedWithError:error];
  }
}

- (void)connectPeripheralForPeer:(GNSCentralPeerManager *)peer
                         options:(nullable NSDictionary<NSString *, id> *)options {
  GNCLoggerInfo(@"Connect peer %@ options %@", peer, options);
  [_cbCentralManager connectPeripheral:peer.cbPeripheral options:options];
}

- (void)cancelPeripheralConnectionForPeer:(GNSCentralPeerManager *)peer {
  GNCLoggerInfo(@"Cancel peer connection %@", peer);
  [_cbCentralManager cancelPeripheralConnection:peer.cbPeripheral];
}

- (void)centralPeerManagerDidDisconnect:(GNSCentralPeerManager *)peer {
  GNCLoggerInfo(@"Central manager removing central peer manager, central manager: %@, peer %@",
                self, peer);
  if (peer.cbPeripheral.state != CBPeripheralStateDisconnected) {
    GNCLoggerInfo(@"Unexpected peripheral state %@", peer.cbPeripheral);
  }
}

- (CBCentralManager *)cbCentralManager {
  return _cbCentralManager;
}

- (GNSCentralPeerManager *)createCentralPeerManagerWithPeripheral:(CBPeripheral *)peripheral {
  return [[GNSCentralPeerManager alloc] initWithPeripheral:peripheral
                                            centralManager:self
                                                     queue:_queue];
}

- (GNSCentralPeerManager *)centralPeerForPeripheral:(CBPeripheral *)peripheral {
  NSParameterAssert(peripheral);
  GNSCentralPeerManager *peerManager = [_centralPeerManagers objectForKey:peripheral.identifier];
  if (!peerManager) {
    GNCLoggerDebug(@"No peer manager found for peripheral %@", peripheral);
    return nil;
  }
  if (peerManager.cbPeripheral != peripheral) {
    // There is a peer manager with a different peripheral object than |peripheral|, but with the
    // same identifier. Something really wrong happened in CoreBluetooth here.
    GNCLoggerError(@"Peer Manager %@ has a different peripheral than %@ [CentralManager = %@]",
                   peerManager, peripheral, self);
    return nil;
  }
  return peerManager;
}

#pragma mark - CBCentralManagerDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
  NSAssert(central == _cbCentralManager, @"Wrong peripheral manager.");
  GNCLoggerInfo(@"CoreBluetooth state: %@", CBManagerStateString([self cbManagerState]));
  switch (central.state) {
    case CBManagerStatePoweredOn: {
      if (_scanning) {
        [self startCBScan];
      }
      break;
    }
    case CBManagerStatePoweredOff:
    case CBManagerStateResetting:
    case CBManagerStateUnauthorized:
    case CBManagerStateUnsupported:
    case CBManagerStateUnknown:
      [self stopCBScan];
      break;
  }
  [_delegate centralManagerDidUpdateBleState:self];
  for (GNSCentralPeerManager *peerManager in _centralPeerManagers.objectEnumerator) {
    [peerManager cbManagerStateDidUpdate];
  }
}

- (void)centralManager:(CBCentralManager *)central
    didDiscoverPeripheral:(CBPeripheral *)peripheral
        advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                     RSSI:(NSNumber *)RSSI {
  // This method can be called several times per second since
  // CBCentralManagerScanOptionAllowDuplicatesKey is set to YES. Therefore, don't log.
  NSArray<CBUUID *> *advertisedServiceUUIDs = advertisementData[CBAdvertisementDataServiceUUIDsKey];
  NSString *advertisedName = advertisementData[CBAdvertisementDataLocalNameKey];
  BOOL noAdvertisementFilter = !_advertisedServiceUUID && !_advertisedName;
  if (noAdvertisementFilter || [advertisedServiceUUIDs containsObject:_advertisedServiceUUID] ||
      [_advertisedName isEqualToString:advertisedName]) {
    NSUUID *identifier = peripheral.identifier;
    GNSCentralPeerManager *peerManager = [_centralPeerManagers objectForKey:identifier];
    if (!peerManager) {
      GNCLoggerInfo(@"Discovered peer peripheral %@", peripheral);
      peerManager = [self createCentralPeerManagerWithPeripheral:peripheral];
      [_centralPeerManagers setObject:peerManager forKey:identifier];
      [_delegate centralManager:self
                didDiscoverPeer:peerManager
              advertisementData:advertisementData];
    }
  }
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {
  NSAssert(central == _cbCentralManager, @"Unexpected central manager");
  GNCLoggerInfo(@"Connected to %@", peripheral);
  GNSCentralPeerManager *peerManager = [self centralPeerForPeripheral:peripheral];
  if (!peerManager) {
    GNCLoggerError(
        @"No peer manager found for connected peripheral %@. Cancel peripheral connection",
        peripheral);
    [_cbCentralManager cancelPeripheralConnection:peripheral];
    return;
  }
  [peerManager bleConnected];
}

- (void)centralManager:(CBCentralManager *)central
    didFailToConnectPeripheral:(CBPeripheral *)peripheral
                         error:(nullable NSError *)error {
  NSAssert(central == _cbCentralManager, @"Unexpected central manager");
  GNCLoggerInfo(@"Fail to connect to %@, error %@", peripheral, error);
  [self peripheralDisconnected:peripheral withError:error];
}

- (void)centralManager:(CBCentralManager *)central
    didDisconnectPeripheral:(CBPeripheral *)peripheral
                      error:(nullable NSError *)error {
  NSAssert(central == _cbCentralManager, @"Unexpected central manager");
  GNCLoggerInfo(@"Did disconnect %@, error %@", peripheral, error);
  [self peripheralDisconnected:peripheral withError:error];
}

@end

@implementation GNSCentralManager (VisibleForTesting)

- (CBCentralManager *)testing_cbCentralManager {
  return _cbCentralManager;
}

@end

NS_ASSUME_NONNULL_END
