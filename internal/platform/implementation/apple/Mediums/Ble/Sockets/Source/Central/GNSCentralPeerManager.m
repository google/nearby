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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralPeerManager.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralPeerManager+Private.h"

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralManager.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSUtils.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSUtils+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSWeavePacket.h"
#import "GoogleToolboxForMac/GTMLogger.h"

static const NSTimeInterval kMaxConnectionConfirmWaitTimeInSeconds = 2;
static const NSTimeInterval kPeripheralFailedToConnectTimeout = 5.0;
static const NSTimeInterval kChracteristicWriteTimeoutInSeconds = 0.5;

// The Weave BLE protocol has only one valid version.
static const UInt16 kWeaveVersionSupported = 1;

typedef NS_ENUM(NSInteger, GNSCentralPeerManagerState) {
  GNSCentralPeerManagerStateNotConnected = 0,
  GNSCentralPeerManagerStateBleConnecting = 1,
  GNSCentralPeerManagerStateBleConnected = 2,
  GNSCentralPeerManagerStateDiscoveringService = 3,
  GNSCentralPeerManagerStateDiscoveringCharacteristic = 4,
  GNSCentralPeerManagerStateSettingNotifications = 5,
  GNSCentralPeerManagerStateSocketCommunication = 6,
  // "BLEDisconnecting" is optional. If CoreBluetooth notifies that the central is disconnected,
  // the state will switch from any state to "NotConnected" without going through
  // "BLEDisconnecting".
  GNSCentralPeerManagerStateBleDisconnecting = 7,
};

typedef void (^GNSCentralPeerManagerDiscoverCharacteristicsCallback)(NSError *error);

static NSUInteger gGNSCentralPeerManagerMaximumUpdateValue = 100;

static NSString *StateDescription(GNSCentralPeerManagerState state) {
  switch (state) {
    case GNSCentralPeerManagerStateNotConnected:
      return @"Not connected";
    case GNSCentralPeerManagerStateBleConnecting:
      return @"BLE Connecting";
    case GNSCentralPeerManagerStateBleConnected:
      return @"BLE Connected";
    case GNSCentralPeerManagerStateDiscoveringService:
      return @"Discovering service";
    case GNSCentralPeerManagerStateDiscoveringCharacteristic:
      return @"Discovering characteristics";
    case GNSCentralPeerManagerStateSettingNotifications:
      return @"Setting characteristic notification";
    case GNSCentralPeerManagerStateSocketCommunication:
      return @"Socket communication";
    case GNSCentralPeerManagerStateBleDisconnecting:
      return @"BLE Disconnecting";
  }
  return @"Unknown";
}

static NSString *PeripheralStateString(CBPeripheralState state) {
  switch (state) {
    case CBPeripheralStateDisconnected:
      return @"CBPeripheralStateDisconnected";
    case CBPeripheralStateConnecting:
      return @"CBPeripheralStateConnecting";
    case CBPeripheralStateConnected:
      return @"CBPeripheralStateConnected";
    case CBPeripheralStateDisconnecting:
      return @"CBPeripheralStateDisconnecting";
  }
  return [NSString stringWithFormat:@"CBPeripheralState Unknown (%ld)", (long)state];
}

@interface GNSCentralPeerManager ()<GNSWeavePacketHandler>
@property(nonatomic) GNSCentralPeerManagerState state;
@property(nonatomic) GNSCentralManager *centralManager;
@property(nonatomic) dispatch_queue_t queue;
@property(nonatomic) int indexOfServiceToCheck;
@property(nonatomic) CBService *socketService;
@property(nonatomic) CBCharacteristic *outgoingChar;
@property(nonatomic) CBCharacteristic *incomingChar;
@property(nonatomic) CBCharacteristic *pairingChar;
@property(nonatomic, weak) GNSSocket *socket;
@property(nonatomic) GNSCentralSocketCompletion discoveringServiceSocketCompletion;
@property(nonatomic) BOOL shouldAddPairingCharacteristic;
@property(nonatomic) GNSPairingCompletion pairingCompletion;
@property(nonatomic) NSMutableArray<GNSReadRRSIValueCompletion> *readRSSIValueCompletions;
@property(nonatomic) NSError *disconnectedError;
@property(nonatomic) NSData *connectionRequestData;
@property(nonatomic) NSDate *startConnectionTime;

// This timer is scheduled right after the connection request packet is sent, and fires if no
// connection confirm packet is received before |kMaxConnectionConfirmWaitTimeInSeconds|.
@property(nonatomic) NSTimer *connectionConfirmTimer;

// The completion of a write-with-response to peripheral; nil if none is in progress.
@property(nonatomic) GNSErrorHandler dataWriteCompletion;
@end

@implementation GNSCentralPeerManager

@synthesize cbPeripheral = _cbPeripheral;

// Used for testing. A test subclass can override this method so a mock timer can be returned.
+ (NSTimer *)scheduledTimerWithTimeInterval:(NSTimeInterval)timeInterval
                                     target:(id)target
                                   selector:(SEL)selector
                                   userInfo:(nullable id)userInfo
                                    repeats:(BOOL)yesOrNo {
  return [NSTimer scheduledTimerWithTimeInterval:timeInterval
                                          target:target
                                        selector:selector
                                        userInfo:userInfo
                                         repeats:yesOrNo];
}

- (instancetype)initWithPeripheral:(CBPeripheral *)peripheral
                    centralManager:(GNSCentralManager *)centralManager
                             queue:(dispatch_queue_t)queue {
  self = [super init];
  if (self) {
    _cbPeripheral = peripheral;
    _cbPeripheral.delegate = self;
    _centralManager = centralManager;
    _queue = queue;
    _socketMaximumUpdateValueLength = gGNSCentralPeerManagerMaximumUpdateValue;
    _state = GNSCentralPeerManagerStateNotConnected;
    GTMLoggerInfo(@"Peripheral %@", _cbPeripheral);
  }
  return self;
}

- (instancetype)initWithPeripheral:(CBPeripheral *)peripheral
                    centralManager:(GNSCentralManager *)centralManager {
  return [self initWithPeripheral:peripheral
                   centralManager:centralManager
                            queue:dispatch_get_main_queue()];
}

- (void)dealloc {
  GTMLoggerDebug(@"Dealloc CentralPeerManager with _cbPeripheral %@", _cbPeripheral);
  _cbPeripheral.delegate = nil;
  if (_cbPeripheral.state != CBPeripheralStateDisconnected) {
    [_centralManager cancelPeripheralConnectionForPeer:self];
  }
}

- (void)socketWithHandshakeData:(NSData *)handshakeData
          pairingCharacteristic:(BOOL)hasPairingCharacteristic
                     completion:(GNSCentralSocketCompletion)completion {
  NSAssert(handshakeData.length <= kGNSMaxCentralHandshakeDataSize, @"Handshake data is too large");
  _connectionRequestData = handshakeData;
  [self socketWithPairingCharacteristic:hasPairingCharacteristic completion:completion];
}

- (void)socketWithPairingCharacteristic:(BOOL)shouldAddPairingCharacteristics
                             completion:(GNSCentralSocketCompletion)completion {
  GTMLoggerInfo(@"Request socket %@", self);
  if (_state != GNSCentralPeerManagerStateNotConnected) {
    GTMLoggerInfo(@"There is a pending socket request");
    if (completion) {
      dispatch_async(_queue, ^{
        completion(nil, GNSErrorWithCode(GNSErrorOperationInProgress));
      });
    }
    return;
  }
  _shouldAddPairingCharacteristic = shouldAddPairingCharacteristics;
  _discoveringServiceSocketCompletion = completion;
  [self bleConnect];
}

- (void)cancelPendingSocket {
  if (_discoveringServiceSocketCompletion == nil) {
    GTMLoggerInfo(@"No pending socket, current socket: %@", _socket);
    return;
  }
  GTMLoggerInfo(@"Cancelling pending socket");
  NSError *cancelPendingSocketRequested = GNSErrorWithCode(GNSErrorCancelPendingSocketRequested);
  [self disconnectingWithError:cancelPendingSocketRequested];
}

- (void)startBluetoothPairingWithCompletion:(GNSPairingCompletion)completion {
  NSAssert(_socket, @"Peer socket should have been created.");
  NSAssert(!_pairingCompletion, @"Pairing already pending.");
  NSAssert(_pairingChar, @"No a pairing characteristic available.");
  _pairingCompletion = completion;
  [_cbPeripheral readValueForCharacteristic:_pairingChar];
}

- (NSUUID *)identifier {
  return _cbPeripheral.identifier;
}

- (void)readRSSIWithCompletion:(GNSReadRRSIValueCompletion)completion {
  NSAssert(completion, @"Completion cannot be nil");
  if (![self isBLEConnected]) {
    NSAssert(!_readRSSIValueCompletions, @"Should not have pending RSSI completions.");
    dispatch_async(_queue, ^{ completion(nil, GNSErrorWithCode(GNSErrorNoConnection)); });
    return;
  }
  if (!_readRSSIValueCompletions) {
    [_cbPeripheral readRSSI];
    _readRSSIValueCompletions = [NSMutableArray array];
  }
  [_readRSSIValueCompletions addObject:completion];
}

- (NSString *)description {
  return [NSString stringWithFormat:@"<%@: %p, state: \"%@\", peripheral identifier: %@, "
                                    @"peripheral state: %@, peripheral: %@>",
                                    [self class], self, StateDescription(_state),
                                    _cbPeripheral.identifier.UUIDString,
                                    PeripheralStateString(_cbPeripheral.state), _cbPeripheral];
}

#pragma mark - Private

- (void)bleConnect {
  GTMLoggerInfo(@"BLE connection started.");
  _startConnectionTime = [NSDate date];

  _state = GNSCentralPeerManagerStateBleConnecting;
  if (_centralManager.cbCentralManagerState == CBCentralManagerStatePoweredOn) {
    NSDictionary<NSString *, id> *options = @{
      CBConnectPeripheralOptionNotifyOnDisconnectionKey : @YES,
#if TARGET_OS_IPHONE
      CBConnectPeripheralOptionNotifyOnConnectionKey : @YES,
      CBConnectPeripheralOptionNotifyOnNotificationKey : @YES,
#endif
    };
    [_centralManager connectPeripheralForPeer:self options:options];

    // Workaround for an apparent Core Bluetooth problem: Either -didConnectPeripheral or
    // -didFailToConnectPeripheral should be called at this point, but sometimes neither is called.
    // In this case, report a timeout error.
    __weak __typeof__(self) weakSelf = self;
    dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kPeripheralFailedToConnectTimeout * NSEC_PER_SEC)),
      _queue, ^{
        __typeof__(self) strongSelf = weakSelf;
        if (!strongSelf) return;
        if (strongSelf.state == GNSCentralPeerManagerStateBleConnecting) {
          GTMLoggerInfo(@"Timed out trying to connect to peripheral: %@", strongSelf.cbPeripheral);
          [strongSelf.centralManager cancelPeripheralConnectionForPeer:self];
          [strongSelf bleDisconnectedWithError:GNSErrorWithCode(GNSErrorConnectionTimedOut)];
        }
      });
  }
}

- (void)bleConnected {
  if (_cbPeripheral.state != CBPeripheralStateConnected) {
    GTMLoggerInfo(@"Unexpected peripheral state: %ld", (long)_cbPeripheral.state);
  }
  GTMLoggerInfo(@"BLE connected. Elapsed time: %f",
                [[NSDate date] timeIntervalSinceDate:_startConnectionTime]);
  if (_state != GNSCentralPeerManagerStateBleConnecting) {
    GTMLoggerInfo(@"Should not be connected with %@ when in %@ state", _cbPeripheral,
                  StateDescription(_state));
    return;
  }
  GTMLoggerInfo(@"BLE connected, peripheral: %@", _cbPeripheral);
  [self discoverService];
}

- (void)bleDisconnectedWithError:(NSError *)error {
  GTMLoggerInfo(@"BLE disconnected, peripheral: %@", _cbPeripheral);
  if (_state == GNSCentralPeerManagerStateNotConnected) {
    return;
  } else {
    if (!_disconnectedError) {
      _disconnectedError = error;
    }
    [_connectionConfirmTimer invalidate];
    _connectionConfirmTimer = nil;
    [self bleDisconnected];
  }
}

- (void)cbCentralManagerStateDidUpdate {
  if (_centralManager.cbCentralManagerState == CBCentralManagerStatePoweredOn &&
      _state == GNSCentralPeerManagerStateBleConnecting) {
    [self bleConnect];
  }
}

- (void)disconnectingWithError:(NSError *)error {
  if (error) {
    GTMLoggerInfo(@"Disconnected with error: %@, peripheral: %@", error, _cbPeripheral);
  } else {
    GTMLoggerInfo(@"Disconnected from peripheral: %@", _cbPeripheral);
  }
  [_connectionConfirmTimer invalidate];
  _connectionConfirmTimer = nil;
  _connectionRequestData = nil;
  _disconnectedError = error;

  // If there is a pending characteristic write, call the completion.
  [self callDataWriteCompletionWithError:GNSErrorWithCode(GNSErrorLostConnection)];

  [self cleanRSSICompletionAfterDisconnectionWithError:error];
  _state = GNSCentralPeerManagerStateBleDisconnecting;
  if (_cbPeripheral.state != CBPeripheralStateDisconnected) {
    [_centralManager cancelPeripheralConnectionForPeer:self];
  } else {
    [self bleDisconnected];
  }
}

- (void)bleDisconnected {
  GTMLoggerInfo(@"BLE Disconnected %@", _cbPeripheral);
  if (_cbPeripheral.state != CBPeripheralStateDisconnected) {
    GTMLoggerInfo(@"Unexpected peripheral state: %ld", (long)_cbPeripheral.state);
  }
  NSAssert(_cbPeripheral.delegate == self, @"Self = %@ should be the delegate of %@", self,
           _cbPeripheral);

  // Clean-up this peer manager.
  [_centralManager centralPeerManagerDidDisconnect:self];
  GNSSocket *currentSocket = _socket;
  _socketService = nil;
  _socket = nil;
  _state = GNSCentralPeerManagerStateNotConnected;

  // Inform all interested parties that the peripheral is disconnected.
  [self cleanRSSICompletionAfterDisconnectionWithError:_disconnectedError];
  if (_discoveringServiceSocketCompletion) {
    NSAssert(!_socket, @"Should not have a socket yet.");
    if (!_disconnectedError) {
      _disconnectedError = GNSErrorWithCode(GNSErrorNoConnection);
    }
    GNSCentralSocketCompletion completion = _discoveringServiceSocketCompletion;
    _discoveringServiceSocketCompletion = nil;
    dispatch_async(_queue, ^{ completion(nil, _disconnectedError); });
  } else if (currentSocket) {
    [currentSocket didDisconnectWithError:_disconnectedError];
  }
}

- (void)discoverService {
  _state = GNSCentralPeerManagerStateDiscoveringService;
  CBUUID *serviceUUID = _centralManager.socketServiceUUID;
  GTMLoggerInfo(@"Discover service %@ on peripheral: %@", serviceUUID, _cbPeripheral);
  NSArray<CBUUID *> *servicesToDiscover = @[ serviceUUID ];
  [_cbPeripheral discoverServices:servicesToDiscover];
}

- (void)discoverCharacteristics {
  if (_cbPeripheral.services.count == 0) return;
  GTMLoggerInfo(@"Discover characteristics, peer manager %@", self);
  NSMutableArray<CBUUID *> *characteristics = [NSMutableArray
      arrayWithObjects:[CBUUID UUIDWithString:kGNSWeaveToPeripheralCharUUIDString],
                       [CBUUID UUIDWithString:kGNSWeaveFromPeripheralCharUUIDString], nil];
  if (_shouldAddPairingCharacteristic) {
    [characteristics addObject:[CBUUID UUIDWithString:kGNSPairingCharUUIDString]];
  }
  [_cbPeripheral discoverCharacteristics:characteristics
                              forService:_cbPeripheral.services[_indexOfServiceToCheck]];
}

- (BOOL)findCharacteristicsFromService:(CBService *)service {
  CBCharacteristic *outgoingChar = nil;
  CBCharacteristic *incomingChar = nil;
  CBCharacteristic *pairingChar = nil;
  for (CBCharacteristic *characteristic in service.characteristics) {
    if ([characteristic.UUID isEqual:[CBUUID UUIDWithString:kGNSWeaveToPeripheralCharUUIDString]]) {
      outgoingChar = characteristic;
    } else if ([characteristic.UUID
                   isEqual:[CBUUID UUIDWithString:kGNSWeaveFromPeripheralCharUUIDString]]) {
      incomingChar = characteristic;
    } else if ([characteristic.UUID isEqual:[CBUUID UUIDWithString:kGNSPairingCharUUIDString]]) {
      pairingChar = characteristic;
    }
  }
  if (!outgoingChar || !incomingChar || (_shouldAddPairingCharacteristic && !pairingChar)) {
    return NO;
  }
  _incomingChar = incomingChar;
  _outgoingChar = outgoingChar;
  _pairingChar = pairingChar;
  return YES;
}

- (void)setCharacteristicNotification {
  NSAssert(_incomingChar, @"Should have found incoming characteristic");
  _state = GNSCentralPeerManagerStateSettingNotifications;
  [_cbPeripheral setNotifyValue:YES forCharacteristic:_incomingChar];
}

- (CBPeripheral *)cbPeripheral {
  return _cbPeripheral;
}

// If error is nil, kGNSNoConnection |error| is passed to the rssi completion.
- (void)cleanRSSICompletionAfterDisconnectionWithError:(NSError *)error {
  if (_readRSSIValueCompletions) {
    NSError *rssiCompletionError = error;
    if (rssiCompletionError) {
      rssiCompletionError = GNSErrorWithCode(GNSErrorNoConnection);
    }
    [self callRSSICompletionWithRSSIValue:nil error:rssiCompletionError];
  }
}

- (void)callRSSICompletionWithRSSIValue:(NSNumber *)rssiValue error:(NSError *)error {
  NSArray<GNSReadRRSIValueCompletion> *completions = _readRSSIValueCompletions;
  _readRSSIValueCompletions = nil;
  for (GNSReadRRSIValueCompletion completion in completions) {
    dispatch_async(_queue, ^{ completion(rssiValue, error); });
  }
}

- (BOOL)isBLEConnected {
  switch (_state) {
    case GNSCentralPeerManagerStateNotConnected:
    case GNSCentralPeerManagerStateBleConnecting:
      return NO;
    case GNSCentralPeerManagerStateBleConnected:
    case GNSCentralPeerManagerStateDiscoveringService:
    case GNSCentralPeerManagerStateDiscoveringCharacteristic:
    case GNSCentralPeerManagerStateSettingNotifications:
    case GNSCentralPeerManagerStateSocketCommunication:
    case GNSCentralPeerManagerStateBleDisconnecting:
      return YES;
  }
}

- (void)timeOutConnectionForTimer:(NSTimer *)timer {
  NSAssert(_state == GNSCentralPeerManagerStateSocketCommunication,
           @"Timer (%@) fired on the wrong state. [self = %@]", _connectionConfirmTimer, self);
  GTMLoggerInfo(@"Timing out %@ socket connection [self = %@].", _socket, self);
  _connectionConfirmTimer = nil;
  NSError *error = GNSErrorWithCode(GNSErrorConnectionTimedOut);
  [self disconnectingWithError:error];
}

- (void)callDataWriteCompletionWithError:(NSError *)error {
  if (_dataWriteCompletion) {
    GNSErrorHandler dataWriteCompletion = _dataWriteCompletion;  // tail call for reentrancy
    _dataWriteCompletion = nil;
    dispatch_async(_queue, ^{ dataWriteCompletion(error); });
  }
}

#pragma mark - CBPeripheralDelegate

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error {
  GTMLoggerInfo(@"BLE services discovered. Elapsed time: %f",
                [[NSDate date] timeIntervalSinceDate:_startConnectionTime]);
  if (_state != GNSCentralPeerManagerStateDiscoveringService) {
    GTMLoggerInfo(@"Ignoring services discovered from %@ when in %@ state", _cbPeripheral,
                  StateDescription(_state));
    return;
  }
  GTMLoggerInfo(@"Service discovered for %@, error: %@", self, error);
  if (error) {
    [self disconnectingWithError:error];
    return;
  }

  // Multiple services may have the same UUID, so find the socket service by checking for the
  // expected characteristics.
  _indexOfServiceToCheck = 0;
  _state = GNSCentralPeerManagerStateDiscoveringCharacteristic;
  [self discoverCharacteristics];
}

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(NSError *)error {
  GTMLoggerInfo(@"BLE characteristics discovered. Elapsed time: %f",
                [[NSDate date] timeIntervalSinceDate:_startConnectionTime]);
  if (_state != GNSCentralPeerManagerStateDiscoveringCharacteristic) {
    GTMLoggerInfo(@"Ignoring characteristics discovered from %@ when in %@ state", _cbPeripheral,
                  StateDescription(_state));
    return;
  }
  GTMLoggerInfo(@"Characteristics discovered for service: %@, error: %@", service, error);
  CBService *serviceBeingChecked = _cbPeripheral.services[_indexOfServiceToCheck];
  NSAssert([service.UUID isEqual:serviceBeingChecked.UUID], @"Unknown service %@ %@",
           service, serviceBeingChecked.UUID);
  if (error) {
    [self disconnectingWithError:error];
    return;
  }
  if (![self findCharacteristicsFromService:serviceBeingChecked]) {
    if (++_indexOfServiceToCheck == _cbPeripheral.services.count) {
      // Didn't find the characteristics in any of the services.
      NSError *missingCharacteristicError = GNSErrorWithCode(GNSErrorMissingCharacteristics);
      [self disconnectingWithError:missingCharacteristicError];
    } else {
      // Check the next service in the list.
      [self discoverCharacteristics];
    }
    return;
  }
  [self setCharacteristicNotification];
}

- (void)handleWeaveError:(GNSError)errorCode socket:(GNSSocket *)socket {
  if (!socket) {
    return;
  }
  NSAssert(socket == _socket, @"Wrong socket, socket = %@, _socket = %@.", socket, _socket);
  NSError *error = GNSErrorWithCode(errorCode);
  // Only send the error if we didn't already received an error packet.
  if (errorCode != GNSErrorWeaveErrorPacketReceived) {
    GNSWeaveErrorPacket *errorPacket =
        [[GNSWeaveErrorPacket alloc] initWithPacketCounter:socket.sendPacketCounter];
    [self sendPacket:errorPacket];
  }
  [self disconnectingWithError:error];
}

- (void)peripheral:(CBPeripheral *)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(NSError *)error {
  if (_state != GNSCentralPeerManagerStateSocketCommunication) {
    GTMLoggerInfo(@"Ignoring data received from %@ when in %@ state", _cbPeripheral,
                  StateDescription(_state));
    return;
  }
  NSAssert(peripheral == _cbPeripheral, @"Unknown peripheral.");
  if ([characteristic.UUID isEqual:_pairingChar.UUID]) {
    GNSPairingCompletion completion = _pairingCompletion;
    _pairingCompletion = nil;
    if (completion) dispatch_async(_queue, ^{ completion(error == nil, error); });
    return;
  }
  NSAssert(characteristic == _incomingChar, @"Should read only from the incoming characteristic.");
  NSAssert(_socket, @"Should have a socket in the %@ state", StateDescription(_state));
  NSData *charValue = characteristic.value;
  // Truncate the packet to expected size: |kGNSMinSupportedPacketSize| for the first packet and
  // |socket.packetSize| for the rest.
  NSUInteger truncatedSize = MIN(_socket.packetSize, charValue.length);
  if (truncatedSize != charValue.length) {
    GTMLoggerInfo(@"Packet with %ld bytes trucanted to %ld", (long)charValue.length,
                  (long)truncatedSize);
  }
  NSData *packetData = [charValue subdataWithRange:NSMakeRange(0, truncatedSize)];
  NSError *parsingError = nil;
  GNSWeavePacket *packet = [GNSWeavePacket parseData:packetData error:&parsingError];
  if (!packet) {
    GTMLoggerError(@"Error parsing Weave packet (error = %@).", parsingError);
    [self handleWeaveError:GNSErrorParsingWeavePacket socket:_socket];
    return;
  }
  if (packet.packetCounter != _socket.receivePacketCounter) {
    GTMLoggerError(@"Wrong packet counter, [received %d, expected %d].", packet.packetCounter,
                   _socket.receivePacketCounter);
    [self handleWeaveError:GNSErrorWrongWeavePacketCounter socket:_socket];
    return;
  }
  [packet visitWithHandler:self context:nil];
  [_socket incrementReceivePacketCounter];
}

- (void)peripheral:(CBPeripheral *)peripheral
    didWriteValueForCharacteristic:(CBCharacteristic *)characteristic
                             error:(NSError *)error {
  // Note: Avoid using |characteristic.value| here as it seems it is always nil.
  if (error) {
    GTMLoggerInfo(@"Characteristic write failed with error: %@", error);
    [self disconnectingWithError:error];
  } else {
    GTMLoggerInfo(@"Characteristic write succeeded");
  }
  [self callDataWriteCompletionWithError:error];
}

// This method sends |packet| fitting a single characteristic write to |socket|. All packets sent by
// this class (not the socket) must use this method.
- (void)sendPacket:(GNSWeavePacket *)packet {
  GTMLoggerInfo(@"Writing value to characteristic (internal)");
  NSAssert(packet.packetCounter == _socket.sendPacketCounter, @"Wrong packet counter.");
  [_cbPeripheral writeValue:[packet serialize]
          forCharacteristic:_outgoingChar
                       type:CBCharacteristicWriteWithResponse];
  [_socket incrementSendPacketCounter];
}

- (void)peripheral:(CBPeripheral *)peripheral
    didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic
                                          error:(NSError *)error {
  GTMLoggerInfo(@"BLE characteristic notifications started. Elapsed time: %f",
                [[NSDate date] timeIntervalSinceDate:_startConnectionTime]);
  if (_state != GNSCentralPeerManagerStateSettingNotifications) {
    GTMLoggerInfo(@"Ignoring notification status change for %@ when in %@ state", _cbPeripheral,
                  StateDescription(_state));
    return;
  }
  GTMLoggerInfo(@"Characteristic notification update: %@, error: %@", characteristic, error);
  if (error) {
    [self disconnectingWithError:error];
    return;
  }
  NSAssert([characteristic.UUID isEqual:_incomingChar.UUID], @"Wrong characteristic");
  NSAssert(_state == GNSCentralPeerManagerStateSettingNotifications, @"Wrong state");
  _state = GNSCentralPeerManagerStateSocketCommunication;
  GTMLoggerInfo(@"Socket ready %@", self);
  GNSSocket *socket =
      [[GNSSocket alloc] initWithOwner:self peripheralPeer:_cbPeripheral queue:_queue];
  GNSCentralSocketCompletion completion = _discoveringServiceSocketCompletion;
  _discoveringServiceSocketCompletion = nil;
  _socket = socket;
  dispatch_async(_queue, ^{ completion(socket, nil); });
  if (!_socket) {
    [self disconnectingWithError:nil];
    return;
  }
  GTMLoggerInfo(@"Sending connection request packet.");
  // On iOS/OS X the central (client) doesn't have access to the negotiated BLE connection MTU. So,
  // according to the Weave BLE protocol specs, it should send 0. The peripheral will then choose
  // the appropriated value for the packet size and send it back on the connection confirm packet.
  GNSWeaveConnectionRequestPacket *connectionRequest =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:kWeaveVersionSupported
                                                       maxVersion:kWeaveVersionSupported
                                                    maxPacketSize:0
                                                             data:_connectionRequestData];
  [self sendPacket:connectionRequest];
  _connectionConfirmTimer =
      [[self class] scheduledTimerWithTimeInterval:kMaxConnectionConfirmWaitTimeInSeconds
                                            target:self
                                          selector:@selector(timeOutConnectionForTimer:)
                                          userInfo:nil
                                           repeats:NO];
}

- (void)peripheral:(CBPeripheral *)peripheral didReadRSSI:(NSNumber *)RSSI error:(NSError *)error {
  if (error) {
    GTMLoggerError(@"Error to read RSSI %@", error);
  }
  [self callRSSICompletionWithRSSIValue:RSSI error:error];
}

#pragma mark - GNSSocketOwner

- (NSUInteger)socketMaximumUpdateValueLength:(GNSSocket *)socket {
  return _socketMaximumUpdateValueLength;
}

- (void)sendData:(NSData *)data socket:(GNSSocket *)socket completion:(GNSErrorHandler)completion {
  GTMLoggerInfo(@"Writing value to characteristic");
  if (_dataWriteCompletion != nil) {
    // This shouldn't happen because writes should be serialized by the socket code. But log it
    // in case there's a bug that causes it to happen.
    GTMLoggerInfo(@"Previous characteristic data write didn't complete");
  }

  // Sometimes -didWriteValueForCharacteristic: isn't called, leaving the write operation hanging.
  // If it times out, call the completion with an error.
  __weak __typeof__(self) weakSelf = self;
  dispatch_block_t dataWriteTimeoutBlock = dispatch_block_create(0, ^{
    __typeof__(self) strongSelf = weakSelf;
    if (!strongSelf || !socket.isConnected) return;
    GTMLoggerInfo(@"Characteristic data write timed out");
    [strongSelf callDataWriteCompletionWithError:GNSErrorWithCode(GNSErrorConnectionTimedOut)];
  });
  _dataWriteCompletion = ^(NSError *error) {
    // -didWriteValueForCharacteristic: was called, so cancel the timeout
    dispatch_block_cancel(dataWriteTimeoutBlock);
    completion(error);
  };
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                               (int64_t)(kChracteristicWriteTimeoutInSeconds * NSEC_PER_SEC)),
                 _queue,
                 dataWriteTimeoutBlock);

  [_cbPeripheral writeValue:data
          forCharacteristic:_outgoingChar
                       type:CBCharacteristicWriteWithResponse];
}

- (NSUUID *)socketServiceIdentifier:(GNSSocket *)socket {
  return _cbPeripheral.identifier;
}

- (void)disconnectSocket:(GNSSocket *)socket {
  if (!socket.isConnected || _state == GNSCentralPeerManagerStateNotConnected ||
      _state == GNSCentralPeerManagerStateBleDisconnecting) {
    return;
  }
  GTMLoggerInfo(@"Disconnect socket %@", socket);
  [self disconnectingWithError:nil];
}

- (void)socketWillBeDeallocated:(GNSSocket *)socket {
  if ((_state == GNSCentralPeerManagerStateBleDisconnecting) ||
      (_state == GNSCentralPeerManagerStateNotConnected)) {
    // Already disconnecting or disconnected. Nothing to do.
    return;
  }
  // The socket owner is dropping the socket without notice. The BLE has to be disconnected.
  _socket = nil;
  [self disconnectingWithError:nil];
}

#pragma mark - GNSWeavePacketHandler

- (void)handleConnectionRequestPacket:(GNSWeaveConnectionRequestPacket *)packet
                              context:(id)context {
  GTMLoggerError(@"Unexpected connection request packet received.");
  [self handleWeaveError:GNSErrorUnexpectedWeaveControlPacket socket:_socket];
}

- (void)handleConnectionConfirmPacket:(GNSWeaveConnectionConfirmPacket *)packet
                              context:(id)context {
  // The protocol specs guarantee that a connection confirm packet should contain a valid
  // version for the client, i.e. inside the version interval sent in the connection request
  // packet. If the server doesn't support any versions in the client interval an error packet
  // should be sent, not a connection confirm.
  NSAssert(packet.version == kWeaveVersionSupported, @"Unsupported Weave protocol version: %d.",
           packet.version);
  [_connectionConfirmTimer invalidate];
  _connectionConfirmTimer = nil;
  _socket.packetSize = packet.packetSize;
  [_socket didConnect];
  if (packet.data) {
    // According to the Weave BLE protocol the data received during the connection handshake should
    // be treated as the first message of the socket.
    //
    // Simulate a data packet being received. The packet counter value is not checked by the socket.
    GNSWeaveDataPacket *dataPacket = [[GNSWeaveDataPacket alloc] initWithPacketCounter:0
                                                                           firstPacket:YES
                                                                            lastPacket:YES
                                                                                  data:packet.data];
    [_socket didReceiveIncomingWeaveDataPacket:dataPacket];
  }
}

- (void)handleErrorPacket:(GNSWeaveErrorPacket *)packet context:(id)context {
  GTMLoggerInfo(@"Error packet received.");
  [self handleWeaveError:GNSErrorWeaveErrorPacketReceived socket:_socket];
}

- (void)handleDataPacket:(GNSWeaveDataPacket *)packet context:(id)context {
  if (packet.isFirstPacket && _socket.waitingForIncomingData) {
    GTMLoggerError(@"There is already a receive operation in progress");
    [self handleWeaveError:GNSErrorWeaveDataTransferInProgress socket:_socket];
    return;
  }
  [_socket didReceiveIncomingWeaveDataPacket:packet];
}

#pragma mark - TestingHelpers

- (NSTimer *)testing_connectionConfirmTimer {
  return _connectionConfirmTimer;
}

@end
