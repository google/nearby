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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralServiceManager+Private.h"

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSUtils.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSWeavePacket.h"
#import "GoogleToolboxForMac/GTMLogger.h"

// The Weave BLE protocol has only one valid version.
static const UInt16 kWeaveVersionSupported = 1;

static NSString *GetBluetoothServiceStateDescription(GNSBluetoothServiceState state) {
  switch (state) {
    case GNSBluetoothServiceStateNotAdded:
      return @"NotAdded";
    case GNSBluetoothServiceStateAddInProgress:
      return @"AddInProgress";
    case GNSBluetoothServiceStateAdded:
      return @"Added";
  }
  NSCAssert(NO, @"Wrong GNSBluetoothServiceState %ld", (long)state);
  return @"";
}

@interface GNSPeripheralServiceManager ()<GNSWeavePacketHandler> {
  GNSPeripheralManager *_peripheralManager;
  BOOL _addPairingCharacteristic;
  CBMutableService *_cbService;

  // Weave characteristics UUIDs are different from legagy ones.
  CBMutableCharacteristic *_weaveIncomingChar;
  CBMutableCharacteristic *_weaveOutgoingChar;

  // Pairing chacteristic is used by both Weave and legacy protocols. Changing the UUID of this
  // characteristic has no impact in both Weave and legacy protocols (CrOS doesn't need it).
  CBMutableCharacteristic *_pairingChar;

  // key: central identifier, value: GNSPeripheral
  NSMutableDictionary<NSUUID *, GNSSocket *> *_sockets;
  GNSShouldAcceptSocketHandler _shouldAcceptSocketHandler;
  GNSErrorHandler _bleServiceAddedCompletion;

  dispatch_queue_t _queue;
}

@end

static CBMutableCharacteristic *CreateWeaveIncomingCharacteristic() {
  return [[CBMutableCharacteristic alloc]
      initWithType:[CBUUID UUIDWithString:kGNSWeaveToPeripheralCharUUIDString]
        properties:CBCharacteristicPropertyWrite
             value:nil
       permissions:CBAttributePermissionsWriteable];
}

static CBMutableCharacteristic *CreateWeaveOutgoingCharacteristic() {
  return [[CBMutableCharacteristic alloc]
      initWithType:[CBUUID UUIDWithString:kGNSWeaveFromPeripheralCharUUIDString]
        properties:CBCharacteristicPropertyIndicate
             value:nil
       permissions:CBAttributePermissionsReadable];
}

static CBMutableCharacteristic *CreatePairingCharacteristic() {
  return [[CBMutableCharacteristic alloc]
      initWithType:[CBUUID UUIDWithString:kGNSPairingCharUUIDString]
        properties:CBCharacteristicPropertyRead
             value:nil
       permissions:CBAttributePermissionsReadEncryptionRequired];
}

@implementation GNSPeripheralServiceManager

- (instancetype)init {
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

- (instancetype)initWithBleServiceUUID:(CBUUID *)serviceUUID
              addPairingCharacteristic:(BOOL)addPairingCharacteristic
             shouldAcceptSocketHandler:(GNSShouldAcceptSocketHandler)shouldAcceptSocketHandler
                                 queue:(dispatch_queue_t)queue {
  self = [super init];
  if (self) {
    NSParameterAssert(shouldAcceptSocketHandler);
    _advertising = YES;
    _serviceUUID = serviceUUID;
    _addPairingCharacteristic = addPairingCharacteristic;
    _shouldAcceptSocketHandler = [shouldAcceptSocketHandler copy];
    _sockets = [NSMutableDictionary dictionary];
    _queue = queue;
  }
  return self;
}

- (instancetype)initWithBleServiceUUID:(CBUUID *)serviceUUID
              addPairingCharacteristic:(BOOL)addPairingCharacteristic
             shouldAcceptSocketHandler:(GNSShouldAcceptSocketHandler)shouldAcceptSocketHandler {
  return [self initWithBleServiceUUID:serviceUUID
             addPairingCharacteristic:addPairingCharacteristic
            shouldAcceptSocketHandler:shouldAcceptSocketHandler
                                queue:dispatch_get_main_queue()];
}

- (NSString *)description {
  return [NSString
      stringWithFormat:
          @"<%@: %p, %@, CBService state: %@, serviceUUID: %@, add pairing char: %@, sockets: %ld>",
          [self class], self, _advertising ? @"advertising" : @"not advertising",
          GetBluetoothServiceStateDescription(_cbServiceState), _serviceUUID,
          _addPairingCharacteristic ? @"YES" : @"NO", (unsigned long)_sockets.count];
}

- (void)willAddCBService {
  NSAssert(_cbServiceState == GNSBluetoothServiceStateNotAdded, @"Unexpected CBService state %@ %@",
           GetBluetoothServiceStateDescription(_cbServiceState), self);
  NSAssert(_cbService == nil, @"Service should not exist before it is actually added %@", self);

  _cbServiceState = GNSBluetoothServiceStateAddInProgress;
  NSAssert(!_weaveIncomingChar, @"Should have no weave incoming characteristic %@.", self);
  _weaveIncomingChar = CreateWeaveIncomingCharacteristic();
  NSAssert(!_weaveOutgoingChar, @"Should have no weave outgoing characteristic %@.", self);
  _weaveOutgoingChar = CreateWeaveOutgoingCharacteristic();
  NSAssert(!_pairingChar, @"Should have no pairing characteristic %@.", self);
  if (_addPairingCharacteristic) {
    _pairingChar = CreatePairingCharacteristic();
  }
  _cbService = [[CBMutableService alloc] initWithType:_serviceUUID primary:YES];
  if (_pairingChar) {
    _cbService.characteristics = @[ _weaveIncomingChar, _weaveOutgoingChar, _pairingChar ];
  } else {
    _cbService.characteristics = @[ _weaveIncomingChar, _weaveOutgoingChar ];
  }
  GTMLoggerInfo(@"Will add BLE service: %@", _cbService);
}

- (void)didAddCBServiceWithError:(NSError *)error {
  if (_cbServiceState == GNSBluetoothServiceStateNotAdded) {
    // Ignored as the service was probably removed while the add CBService operation was in
    // progress.
    GTMLoggerError(@"Ignore adding BLE service %@ result (error = %@) %@", _cbService, error, self);
    return;
  }

  if (error) {
    GTMLoggerError(@"Failed adding BLE service %@ (error = %@) %@", _cbService, error, self);
    [self didRemoveCBService];
  } else {
    GTMLoggerInfo(@"Finished adding BLE service %@", _cbService);
    _cbServiceState = GNSBluetoothServiceStateAdded;
  }
  if (_bleServiceAddedCompletion) {
    _bleServiceAddedCompletion(error);
  }
}

- (void)didRemoveCBService {
  _cbServiceState = GNSBluetoothServiceStateNotAdded;
  _cbService = nil;
  _weaveIncomingChar = nil;
  _weaveOutgoingChar = nil;
  _pairingChar = nil;
}

- (void)restoredCBService:(CBMutableService *)service {
  if (![service.UUID isEqual:_serviceUUID]) {
    GTMLoggerError(@"Cannot restore from bluetooth service %@.", service);
    return;
  }
  for (CBMutableCharacteristic *characteristic in service.characteristics) {
    if (_addPairingCharacteristic &&
        [characteristic.UUID isEqual:[CBUUID UUIDWithString:kGNSPairingCharUUIDString]]) {
      _pairingChar = characteristic;
    } else if ([characteristic.UUID
                   isEqual:[CBUUID UUIDWithString:kGNSWeaveToPeripheralCharUUIDString]]) {
      _weaveIncomingChar = characteristic;
    } else if ([characteristic.UUID
                   isEqual:[CBUUID UUIDWithString:kGNSWeaveFromPeripheralCharUUIDString]]) {
      _weaveOutgoingChar = characteristic;
    }
  }
  // TODO(jlebel): Need to find out how the service can be restored if a characteritic is missing.
  // b/25561363
  NSAssert(_weaveIncomingChar, @"Weave incoming characteristic missing");
  NSAssert(_weaveOutgoingChar, @"Weave outgoing characteristic missing");
  NSAssert(_pairingChar || !_addPairingCharacteristic, @"Pairing characteristic missing");
  _cbService = service;
  _cbServiceState = GNSBluetoothServiceStateAdded;
  GTMLoggerInfo(@"Service restored: %@", _cbService);
}

- (void)setAdvertising:(BOOL)advertising {
  if (advertising == _advertising) {
    return;
  }
  _advertising = advertising;
  [_peripheralManager updateAdvertisedServices];
}

#pragma mark - Private

- (GNSShouldAcceptSocketHandler)shouldAcceptSocketHandler {
  return _shouldAcceptSocketHandler;
}

- (GNSPeripheralManager *)peripheralManager {
  return _peripheralManager;
}

- (CBMutableService *)cbService {
  return _cbService;
}

- (CBMutableCharacteristic *)weaveIncomingCharacteristic {
  return _weaveIncomingChar;
}

- (CBMutableCharacteristic *)weaveOutgoingCharacteristic {
  return _weaveOutgoingChar;
}

- (CBMutableCharacteristic *)pairingCharacteristic {
  return _pairingChar;
}

- (GNSErrorHandler)bleServiceAddedCompletion {
  return _bleServiceAddedCompletion;
}

- (void)addedToPeripheralManager:(GNSPeripheralManager *)peripheralManager
       bleServiceAddedCompletion:(GNSErrorHandler)completion {
  NSAssert(_peripheralManager == nil,
           @"Do not reuse. Already associated with another peripheral manager %@",
           _peripheralManager);
  _peripheralManager = peripheralManager;
  _bleServiceAddedCompletion = [completion copy];
}

- (CBATTError)canProcessReadRequest:(CBATTRequest *)request {
  if ([request.characteristic.UUID isEqual:_weaveOutgoingChar.UUID] ||
      [request.characteristic.UUID isEqual:_weaveIncomingChar.UUID]) {
    return CBATTErrorReadNotPermitted;
  }
  if (_pairingChar && [request.characteristic.UUID isEqual:_pairingChar.UUID]) {
    return CBATTErrorSuccess;
  }
  return CBATTErrorAttributeNotFound;
}

- (void)processReadRequest:(CBATTRequest *)request {
  if (!_pairingChar || ![request.characteristic.UUID isEqual:_pairingChar.UUID]) {
    NSAssert(NO, @"Cannot read characteristic %@", request);
    return;
  }
  request.value = [NSData data];
}

- (CBATTError)canProcessWriteRequest:(CBATTRequest *)request {
  if ([request.characteristic.UUID isEqual:_weaveIncomingChar.UUID]) {
    return CBATTErrorSuccess;
  } else if ([request.characteristic.UUID isEqual:_weaveOutgoingChar.UUID]) {
    return CBATTErrorWriteNotPermitted;
  }
  return CBATTErrorAttributeNotFound;
}

- (void)handleWeaveError:(GNSError)errorCode socket:(GNSSocket *)socket {
  if (!socket) {
    return;
  }
  // Only send the error if no error packet was previously received.
  if (errorCode != GNSErrorWeaveErrorPacketReceived) {
    GTMLoggerInfo(@"Sending error packet for socket: %@", socket);
    GNSWeaveErrorPacket *errorPacket =
        [[GNSWeaveErrorPacket alloc] initWithPacketCounter:socket.sendPacketCounter];
    [self sendPacket:errorPacket toSocket:socket completion:^{
      GTMLoggerInfo(@"Error packet sent to socket: %@", socket);
    }];
  }
  NSError *error = GNSErrorWithCode(errorCode);
  [self removeSocket:socket withError:error];
}

- (void)processWeaveWriteRequest:(CBATTRequest *)request {
  if (![request.characteristic.UUID isEqual:_weaveIncomingChar.UUID]) {
    GTMLoggerError(@"Cannot process %@ on characteristic %@ (%@)", request,
                   request.characteristic.UUID,
                   GNSCharacteristicName(request.characteristic.UUID.UUIDString));
    return;
  }
  GNSSocket *socket = _sockets[request.central.identifier];
  // We truncate the packet to size we are expecting: kGNSMinSupportedPacketSize (for the first
  // packet) and |socket.packetSize| (for the rest).
  NSUInteger truncatedSize =
      MIN(socket ? socket.packetSize : kGNSMinSupportedPacketSize, request.value.length);
  if (truncatedSize != request.value.length) {
    GTMLoggerInfo(@"Packet with %ld bytes truncated to %ld.", (long)request.value.length,
                  (long)truncatedSize);
  }
  NSData *packetData = [request.value subdataWithRange:NSMakeRange(0, truncatedSize)];
  NSError *parsingError = nil;
  GNSWeavePacket *packet = [GNSWeavePacket parseData:packetData error:&parsingError];
  if (!packet) {
    GTMLoggerError(@"Error parsing weave packet (error = %@).", parsingError);
    [self handleWeaveError:GNSErrorParsingWeavePacket socket:socket];
    return;
  }
  if (!socket && ![packet isKindOfClass:[GNSWeaveConnectionRequestPacket class]]) {
    GTMLoggerInfo(@"Non-request weave packet received when no socket exists -- ignoring");
    return;
  }
  UInt8 expectedCounter = [packet isKindOfClass:[GNSWeaveConnectionRequestPacket class]] ?
      0 : socket.receivePacketCounter;
  if (packet.packetCounter != expectedCounter) {
    GTMLoggerError(@"Wrong packet counter, [received %d, expected %d].", packet.packetCounter,
                   expectedCounter);
    [self handleWeaveError:GNSErrorWrongWeavePacketCounter socket:socket];
    return;
  }
  [packet visitWithHandler:self context:request];
  // A new socket was created if |request| contains a connection request packet. So we need to
  // update |socket| before incrementing the packet counter.
  socket = _sockets[request.central.identifier];

  // Only an error packet can cause the socket to have been removed at this point.
  NSAssert(socket || [packet isKindOfClass:[GNSWeaveErrorPacket class]],
           @"Socket missing after receiving non-error weave packet");
  [socket incrementReceivePacketCounter];
}

- (void)processWriteRequest:(CBATTRequest *)request {
  if (![request.characteristic.UUID isEqual:_weaveIncomingChar.UUID]) {
    GTMLoggerError(@"Cannot process %@ on characteristic %@ (%@)", request,
                   request.characteristic.UUID,
                   GNSCharacteristicName(request.characteristic.UUID.UUIDString));
    return;
  }
  GTMLoggerInfo(@"Write request on Weave characteristic.");
  [self processWeaveWriteRequest:request];
}

// This method sends |packet| fitting a single characteristic write to |socket|. All packets send by
// this class (not the socket) must use this method.
- (void)sendPacket:(GNSWeavePacket *)packet
          toSocket:(GNSSocket *)socket
        completion:(void (^)(void))completion {
  NSAssert(packet.packetCounter == socket.sendPacketCounter, @"Wrong packet counter.");
  [socket incrementSendPacketCounter];
  // Do not check if the socket before retrying to send the packet if it's a connection confirm
  // packet, as the socket is only connected after that packet is sent.
  BOOL checkConnected = ![packet isKindOfClass:[GNSWeaveConnectionConfirmPacket class]];
  NSData *data = [packet serialize];
  [self sendData:data toSocket:socket checkConnected:checkConnected completion:completion];
}

- (void)sendData:(NSData *)data
        toSocket:(GNSSocket *)socket
  checkConnected:(BOOL)checkConnected
      completion:(void (^)(void))completion {
  __weak __typeof__(self) weakSelf = self;
  [_peripheralManager
      updateOutgoingCharOnSocket:socket
                     withHandler:^() {
                       __typeof__(weakSelf) strongSelf = weakSelf;
                       if (!strongSelf || (checkConnected && !socket.isConnected)) {
                         // Socket is gone or disconnected; don't reschedule.
                         return YES;
                       }
                       if (![strongSelf.peripheralManager updateOutgoingCharacteristic:data
                                                                              onSocket:socket]) {
                         GTMLoggerInfo(@"Failed to update characteristic value; reschedule");
                         return NO;
                       }
                       if (completion) {
                         dispatch_async(_queue, ^{ completion(); });
                       }
                       return YES;
                     }];
}

- (void)socketReady:(GNSSocket *)socket {
  [socket didConnect];
}

- (void)removeSocket:(GNSSocket *)socket withError:(NSError *)error {
  if (error) {
    GTMLoggerInfo(@"Socket disconnected with error, socket: %@, error: %@", socket, error);
  } else {
    GTMLoggerInfo(@"Socket disconnected %@", socket);
  }
  [_sockets removeObjectForKey:socket.peerIdentifier];
  [socket didDisconnectWithError:error];
  [_peripheralManager socketDidDisconnect:socket];
}

- (void)central:(CBCentral *)central
    didSubscribeToCharacteristic:(CBCharacteristic *)characteristic {
  if ([characteristic.UUID isEqual:_weaveOutgoingChar.UUID]) {
    // TODO(sacomoto): The latency should be changed only with CrOS. When iOS is talking with OS X,
    // changing the latency increases the authentication time. See b/23719877.
    [_peripheralManager.cbPeripheralManager
        setDesiredConnectionLatency:CBPeripheralManagerConnectionLatencyLow
                         forCentral:central];
  }
}

- (void)central:(CBCentral *)central
    didUnsubscribeFromCharacteristic:(CBCharacteristic *)characteristic {
  GNSSocket *socket = _sockets[central.identifier];
  if (socket && [characteristic.UUID isEqual:_weaveOutgoingChar.UUID]) {
    // Disconnect signal is optional. If the central unsubscribe to _outgoingChar, the socket
    // is disconnected.
    GTMLoggerInfo(@"%@ unsubscribe to outgoing characteristic", central);
    [self removeSocket:socket withError:nil];
  }
}

#pragma mark - GNSSocketOwner

- (NSUInteger)socketMaximumUpdateValueLength:(GNSSocket *)socket {
  return [socket.peerAsCentral maximumUpdateValueLength];
}

- (void)sendData:(NSData *)data socket:(GNSSocket *)socket completion:(GNSErrorHandler)completion {
  GTMLoggerInfo(@"Updating value in characteristic");
  [_peripheralManager updateOutgoingCharOnSocket:socket withHandler:^{
    BOOL wasSent = [_peripheralManager updateOutgoingCharacteristic:data onSocket:socket];
    if (wasSent) {
      GTMLoggerInfo(@"Successfully updated value in characteristic");
      dispatch_async(_queue, ^{ completion(nil); });
    } else {
      GTMLoggerInfo(@"Failed to update characteristic value; reschedule");
    }
    return wasSent;
  }];
}

- (NSUUID *)socketServiceIdentifier:(GNSSocket *)socket {
  return [[NSUUID alloc] initWithUUIDString:_serviceUUID.UUIDString];
}

- (void)disconnectSocket:(GNSSocket *)socket {
  if (!socket.isConnected) {
    return;
  }
  // The Weave protocol has no disconnect signal. As a temporary solution, we send an error
  // packet, as this will cause the central to disconnect.
  GNSWeaveErrorPacket *errorPacket =
      [[GNSWeaveErrorPacket alloc] initWithPacketCounter:socket.sendPacketCounter];
  __weak __typeof__(self) weakSelf = self;
  [self sendPacket:errorPacket
          toSocket:socket
        completion:^{
          if (weakSelf) {
            GTMLoggerInfo(@"Error packet sent (to force the central to disconnect).");
            [weakSelf removeSocket:socket withError:nil];
          }
        }];
}

- (void)socketWillBeDeallocated:(GNSSocket *)socket {
}

#pragma mark - GNSWeavePacketHandler

- (void)handleConnectionRequestPacket:(GNSWeaveConnectionRequestPacket *)packet
                              context:(id)request {
  NSAssert([request isKindOfClass:[CBATTRequest class]], @"The context should be a request.");
  GNSSocket *socket = _sockets[((CBATTRequest *)request).central.identifier];
  if (socket) {
    GTMLoggerInfo(@"Receiving a connection request from an already connected socket %@.", socket);
    // The peripheral considers the previous socket as being disconnected.
    NSError *error = GNSErrorWithCode(GNSErrorNewInviteToConnectReceived);
    [self removeSocket:socket withError:error];
    socket = nil;
  }
  socket = [[GNSSocket alloc] initWithOwner:self
                                centralPeer:((CBATTRequest *)request).central
                                      queue:_queue];
  if (packet.maxVersion < kWeaveVersionSupported || packet.minVersion > kWeaveVersionSupported) {
    GTMLoggerError(@"Unsupported Weave version range: [%d, %d].", packet.minVersion,
                   packet.maxVersion);
    [self handleWeaveError:GNSErrorUnsupportedWeaveProtocolVersion socket:socket];
    return;
  }
  if (packet.maxPacketSize == 0) {
    socket.packetSize = [self socketMaximumUpdateValueLength:socket];
  } else {
    socket.packetSize = MIN(packet.maxPacketSize, [self socketMaximumUpdateValueLength:socket]);
  }
  if (_shouldAcceptSocketHandler(socket)) {
    _sockets[socket.peerIdentifier] = socket;

    dispatch_async(_queue, ^{
      GTMLoggerInfo(@"Sending connection confirm packet.");
      GNSWeaveConnectionConfirmPacket *confirm =
          [[GNSWeaveConnectionConfirmPacket alloc] initWithVersion:kWeaveVersionSupported
                                                        packetSize:socket.packetSize
                                                              data:nil];
      __weak __typeof__(self) weakSelf = self;
      [self sendPacket:confirm toSocket:socket completion:^{
        if (weakSelf) {
          GTMLoggerInfo(@"Connection confirm packet sent.");
          [weakSelf socketReady:socket];
        }
      }];
    });
  }
}

- (void)handleConnectionConfirmPacket:(GNSWeaveConnectionConfirmPacket *)packet
                              context:(id)request {
  NSAssert([request isKindOfClass:[CBATTRequest class]], @"The context should be a request.");
  GNSSocket *socket = _sockets[((CBATTRequest *)request).central.identifier];
  GTMLoggerError(@"Unexpected connection confirm packet received.");
  [self handleWeaveError:GNSErrorUnexpectedWeaveControlPacket socket:socket];
}

- (void)handleErrorPacket:(GNSWeaveErrorPacket *)packet context:(id)request {
  NSAssert([request isKindOfClass:[CBATTRequest class]], @"The context should be a request.");
  GNSSocket *socket = _sockets[((CBATTRequest *)request).central.identifier];
  GTMLoggerInfo(@"Error packet received.");
  [self handleWeaveError:GNSErrorWeaveErrorPacketReceived socket:socket];
}

- (void)handleDataPacket:(GNSWeaveDataPacket *)packet context:(id)request {
  NSAssert([request isKindOfClass:[CBATTRequest class]], @"The context should be a request.");
  GNSSocket *socket = _sockets[((CBATTRequest *)request).central.identifier];
  if (packet.isFirstPacket && socket.waitingForIncomingData) {
    GTMLoggerError(@"There is already a receive operation in progress");
    [self handleWeaveError:GNSErrorWeaveDataTransferInProgress socket:socket];
    return;
  }
  [socket didReceiveIncomingWeaveDataPacket:packet];
}

@end
