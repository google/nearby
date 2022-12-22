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

#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Peripheral/GNSPeripheralManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Peripheral/GNSPeripheralServiceManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Shared/GNSSocket+Private.h"
#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Shared/GNSUtils.h"
#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Shared/GNSWeavePacket.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@interface GNSPeripheralServiceManagerTest : XCTestCase {
  GNSPeripheralServiceManager *_peripheralServiceManager;
  CBUUID *_serviceUUID;
  BOOL _shouldAcceptSocket;
  GNSSocket *_receivedSocket;
  id _peripheralManagerMock;
  id _cbPeripheralManagerMock;
  id _socketDelegateMock;
  NSMutableArray *_mocksToVerify;
  NSUInteger _centralMaximumUpdateValueLength;
  NSUInteger _packetSize;
}
@end

@implementation GNSPeripheralServiceManagerTest

- (void)setUp {
  _mocksToVerify = [NSMutableArray array];
  _serviceUUID = [CBUUID UUIDWithString:@"3C672799-2B3F-4D93-9E57-29D5C5B01092"];;
  _peripheralManagerMock = OCMStrictClassMock([GNSPeripheralManager class]);
  _cbPeripheralManagerMock = OCMStrictClassMock([CBPeripheralManager class]);
  _socketDelegateMock = OCMStrictProtocolMock(@protocol(GNSSocketDelegate));
  _centralMaximumUpdateValueLength = 100;
  _packetSize = 100;
  OCMStub([_peripheralManagerMock cbPeripheralManager]).andReturn(_cbPeripheralManagerMock);
  _peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:_serviceUUID
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        _receivedSocket = socket;
        socket.delegate = _socketDelegateMock;
        return self->_shouldAcceptSocket;
      }];
  XCTAssertEqualObjects(_peripheralServiceManager.serviceUUID, _serviceUUID);
  [_peripheralServiceManager addedToPeripheralManager:_peripheralManagerMock
                            bleServiceAddedCompletion:nil];
  XCTAssertEqual(GNSBluetoothServiceStateNotAdded, _peripheralServiceManager.cbServiceState);
  XCTAssertNil(_peripheralServiceManager.cbService);
  [_peripheralServiceManager willAddCBService];
  XCTAssertNotNil(_peripheralServiceManager.cbService);
  XCTAssertEqual(GNSBluetoothServiceStateAddInProgress, _peripheralServiceManager.cbServiceState);
  [_peripheralServiceManager didAddCBServiceWithError:nil];
  XCTAssertEqual(GNSBluetoothServiceStateAdded, _peripheralServiceManager.cbServiceState);
}

- (void)tearDown {
  OCMVerifyAll(_peripheralManagerMock);
  OCMVerifyAll(_cbPeripheralManagerMock);
  OCMVerifyAll(_socketDelegateMock);
  for (id mock in _mocksToVerify) {
    OCMVerifyAll(mock);
  }
}

- (void)testServiceManagerAdded {
  GNSPeripheralServiceManager *peripheralServiceManager = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:_serviceUUID
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        _receivedSocket = socket;
        return _shouldAcceptSocket;
      }];
  __block BOOL completionCalled = NO;
  [peripheralServiceManager addedToPeripheralManager:_peripheralManagerMock
                           bleServiceAddedCompletion:^(NSError *error) {
                             completionCalled = YES;
                           }];
  XCTAssertEqual(peripheralServiceManager.peripheralManager, _peripheralManagerMock);
  XCTAssertFalse(completionCalled);
  [peripheralServiceManager willAddCBService];
  XCTAssertFalse(completionCalled);
  [peripheralServiceManager didAddCBServiceWithError:nil];
  XCTAssertTrue(completionCalled);
}

- (void)testPeripheralServiceManagerRestored {
  GNSPeripheralServiceManager *manager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:_serviceUUID
                                         addPairingCharacteristic:NO
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return NO;
                                        }];
  XCTAssertEqual(GNSBluetoothServiceStateNotAdded, manager.cbServiceState);

  CBMutableCharacteristic *weaveOutgoingChar = OCMStrictClassMock([CBMutableCharacteristic class]);
  CBUUID *weaveOutgoingCharUUID = [CBUUID UUIDWithString:@"00000100-0004-1000-8000-001A11000102"];
  OCMStub([weaveOutgoingChar UUID]).andReturn(weaveOutgoingCharUUID);
  CBMutableCharacteristic *weaveIncomingChar = OCMStrictClassMock([CBMutableCharacteristic class]);
  CBUUID *weaveIncomingCharUUID = [CBUUID UUIDWithString:@"00000100-0004-1000-8000-001A11000101"];
  OCMStub([weaveIncomingChar UUID]).andReturn(weaveIncomingCharUUID);

  CBMutableService *cbService = OCMStrictClassMock([CBMutableService class]);
  OCMStub([cbService UUID]).andReturn(_serviceUUID);
  NSArray *restoredCharacteristics = @[ weaveOutgoingChar, weaveIncomingChar ];
  OCMStub([cbService characteristics]).andReturn(restoredCharacteristics);
  [manager restoredCBService:cbService];
  XCTAssertEqual(GNSBluetoothServiceStateAdded, manager.cbServiceState);
  XCTAssertEqual(manager.cbService, cbService);
  XCTAssertEqual(manager.weaveOutgoingCharacteristic, weaveOutgoingChar);
  XCTAssertEqual(manager.weaveIncomingCharacteristic, weaveIncomingChar);
}

- (void)testDefaultAdvertisingValue {
  XCTAssertTrue(_peripheralServiceManager.isAdvertising);
}

- (void)testSetAdvertisingValueToTrue {
  // Nothing should happen since it is already to YES
  _peripheralServiceManager.advertising = YES;
}

- (void)testSetAdvertisingValueToFalse {
  OCMExpect([_peripheralManagerMock updateAdvertisedServices]);
  _peripheralServiceManager.advertising = NO;
}

- (void)testCanProcessReadRequest {
  CBATTRequest *request = OCMStrictClassMock([CBATTRequest class]);
  OCMStubRecorder *recorder = OCMStub([request characteristic]);

  recorder.andReturn(_peripheralServiceManager.weaveOutgoingCharacteristic);
  XCTAssertEqual([_peripheralServiceManager canProcessReadRequest:request],
                 CBATTErrorReadNotPermitted);
  recorder.andReturn(_peripheralServiceManager.weaveIncomingCharacteristic);
  XCTAssertEqual([_peripheralServiceManager canProcessReadRequest:request],
                 CBATTErrorReadNotPermitted);

  OCMVerifyAll((id)request);
}

- (void)testCanProcessReadRequestOnWrongCharacteristic {
  CBATTRequest *request = OCMStrictClassMock([CBATTRequest class]);
  CBUUID *uuid = [CBUUID UUIDWithNSUUID:[NSUUID UUID]];
  id characteristicMock = OCMStrictClassMock([CBMutableCharacteristic class]);
  OCMStub([characteristicMock UUID]).andReturn(uuid);
  OCMStub([request characteristic]).andReturn(characteristicMock);
  XCTAssertEqual([_peripheralServiceManager canProcessReadRequest:request],
                 CBATTErrorAttributeNotFound);
  OCMVerifyAll((id)request);
  OCMVerifyAll(characteristicMock);
}

- (void)testCanProcessWriteRequestOnOutgoingCharacteristic {
  CBATTRequest *request = OCMStrictClassMock([CBATTRequest class]);
  OCMStubRecorder *recorder = OCMStub([request characteristic]);

  recorder.andReturn(_peripheralServiceManager.weaveOutgoingCharacteristic);
  XCTAssertEqual([_peripheralServiceManager canProcessWriteRequest:request],
                 CBATTErrorWriteNotPermitted);
  OCMVerifyAll((id)request);
}

- (void)testCanProcessWriteRequestOnIncomingCharacteristic {
  CBATTRequest *request = OCMStrictClassMock([CBATTRequest class]);
  OCMStubRecorder *recorder = OCMStub([request characteristic]);

  recorder.andReturn(_peripheralServiceManager.weaveIncomingCharacteristic);
  XCTAssertEqual([_peripheralServiceManager canProcessWriteRequest:request], CBATTErrorSuccess);
  OCMVerifyAll((id)request);
}

- (void)testCanProcessWriteRequestOnWrongCharacteristic {
  CBATTRequest *request = OCMStrictClassMock([CBATTRequest class]);
  CBUUID *uuid = [CBUUID UUIDWithNSUUID:[NSUUID UUID]];
  id characteristicMock = OCMStrictClassMock([CBMutableCharacteristic class]);
  OCMStub([characteristicMock UUID]).andReturn(uuid);
  OCMStub([request characteristic]).andReturn(characteristicMock);
  XCTAssertEqual([_peripheralServiceManager canProcessWriteRequest:request],
                 CBATTErrorAttributeNotFound);
  OCMVerifyAll((id)request);
  OCMVerifyAll(characteristicMock);
}

- (CBCentral *)setupRequest:(id)request
         withCharacteristic:(CBMutableCharacteristic *)characteristic
                    central:(CBCentral *)central {
  if (!central) {
    NSUUID *identifier = [NSUUID UUID];
    central = OCMStrictClassMock([CBCentral class]);
    OCMStub([central identifier]).andReturn(identifier);
    OCMStub([central maximumUpdateValueLength]).andReturn(_centralMaximumUpdateValueLength);
    [_mocksToVerify addObject:central];
  }
  OCMStub([request characteristic]).andReturn(characteristic);
  OCMStub([request central]).andReturn(central);
  return central;
}

- (void)testProcessEmptyData {
  NSMutableData *data = [NSMutableData data];
  CBATTRequest *request = OCMStrictClassMock([CBATTRequest class]);
  OCMStub([request value]).andReturn(data);

  [self setupRequest:request
      withCharacteristic:_peripheralServiceManager.weaveIncomingCharacteristic
                 central:nil];
  [_peripheralServiceManager processWriteRequest:request];
  // should not crash, should do nothing.
  OCMVerifyAll((id)request);
}

#pragma mark - Characteristics

- (void)testPairingChar {
  GNSPeripheralServiceManager *manager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:_serviceUUID
                                         addPairingCharacteristic:YES
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return NO;
                                        }];
  [manager willAddCBService];
  [manager didAddCBServiceWithError:nil];
  CBMutableCharacteristic *pairingCharacteristic = manager.pairingCharacteristic;
  XCTAssertNotNil(pairingCharacteristic);
  XCTAssertEqualObjects(pairingCharacteristic.UUID.UUIDString,
                        @"17836FBD-8C6A-4B81-83CE-8560629E834B",
                        @"Wrong pairing characteristic UUID.");
  XCTAssertEqual(pairingCharacteristic.properties, CBCharacteristicPropertyRead,
                 @"Wrong property for pairing characteristic.");
  XCTAssertEqual(pairingCharacteristic.permissions, CBAttributePermissionsReadEncryptionRequired,
                 @"Wrong permission for pairing characteristic.");
}

- (void)testNoPairingChar {
  GNSPeripheralServiceManager *manager =
      [[GNSPeripheralServiceManager alloc] initWithBleServiceUUID:_serviceUUID
                                         addPairingCharacteristic:NO
                                        shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
                                          return NO;
                                        }];
  [manager willAddCBService];
  [manager didAddCBServiceWithError:nil];
  XCTAssertNil(manager.pairingCharacteristic);
}

- (void)testOutgoingChar {
  CBMutableCharacteristic *weaveOutgoingCharacteristic =
      _peripheralServiceManager.weaveOutgoingCharacteristic;
  XCTAssertNotNil(weaveOutgoingCharacteristic);
  XCTAssertEqualObjects(weaveOutgoingCharacteristic.UUID.UUIDString,
                        @"00000100-0004-1000-8000-001A11000102",
                        @"Wrong weave outgoing characteristic UUID.");
  XCTAssertEqual(weaveOutgoingCharacteristic.properties, CBCharacteristicPropertyIndicate,
                 @"Wrong property for weave outgoing characteristic.");
  XCTAssertEqual(weaveOutgoingCharacteristic.permissions, CBAttributePermissionsReadable,
                 @"Wrong permission for weave outgoing characteristic.");
}

- (void)testIncomingChar {
  CBMutableCharacteristic *weaveIncomingCharacteristic =
      _peripheralServiceManager.weaveIncomingCharacteristic;
  XCTAssertNotNil(weaveIncomingCharacteristic);
  XCTAssertEqualObjects(weaveIncomingCharacteristic.UUID.UUIDString,
                        @"00000100-0004-1000-8000-001A11000101",
                        @"Wrong weave incoming characteristic UUID.");
  XCTAssertEqual(weaveIncomingCharacteristic.properties, CBCharacteristicPropertyWrite,
                 @"Wrong property for weave incoming characteristic.");
  XCTAssertEqual(weaveIncomingCharacteristic.permissions, CBAttributePermissionsWriteable,
                 @"Wrong permission for weave incoming characteristic.");
}

#pragma mark - Socket connection

- (void)checkOpenSocketWithShouldAccept:(BOOL)shouldAccept {
  [self checkOpenSocketWithShouldAccept:shouldAccept central:nil];
}

- (void)checkOpenSocketWithShouldAccept:(BOOL)shouldAccept
                                central:(CBCentral *)central {
  CBMutableCharacteristic *characteristic;
  NSMutableData *data = [NSMutableData data];
  _shouldAcceptSocket = shouldAccept;

  // This is necessary to ensure that negotiated packet size is |_packetSize|. It should be set here
  // and in the connection request packet.
  _centralMaximumUpdateValueLength = _packetSize;
  characteristic = _peripheralServiceManager.weaveIncomingCharacteristic;
  GNSWeaveConnectionRequestPacket *connectionRequest =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:_packetSize
                                                             data:nil];
  [data appendData:[connectionRequest serialize]];
  XCTAssertNotNil(characteristic);
  CBATTRequest *request = OCMStrictClassMock([CBATTRequest class]);
  OCMStub([request value]).andReturn(data);
  [self setupRequest:request
      withCharacteristic:characteristic
                 central:central];
  __block GNSUpdateValueHandler updateValueHandler = nil;
  // Nothing is sent when the socket is refused in the Weave protocol.
  if (!shouldAccept) {
    return;
  }
  OCMExpect([_peripheralManagerMock
      updateOutgoingCharOnSocket:[OCMArg checkWithBlock:^BOOL(id obj) {
        // The socket has not been created yet. So the socket check has to be done with a block.
        return _receivedSocket == obj;
      }]
                     withHandler:[OCMArg checkWithBlock:^(id handler) {
                       updateValueHandler = [handler copy];
                       return YES;
                     }]]);
  [_peripheralServiceManager processWriteRequest:request];
  XCTAssertNotNil(updateValueHandler);
  if (!updateValueHandler) {
    return;
  }
  NSMutableData *expectedData = [NSMutableData data];
  GNSWeaveConnectionConfirmPacket *connectionConfirm =
      [[GNSWeaveConnectionConfirmPacket alloc] initWithVersion:1 packetSize:_packetSize data:nil];
  [expectedData appendData:[connectionConfirm serialize]];
  OCMExpect(
      [_peripheralManagerMock updateOutgoingCharacteristic:expectedData onSocket:_receivedSocket])
      .andReturn(YES);
  XCTAssertNotNil(_receivedSocket);
  XCTAssertFalse(_receivedSocket.isConnected);
  if (shouldAccept) {
    OCMExpect([_socketDelegateMock socketDidConnect:_receivedSocket]);
  }
  updateValueHandler(_peripheralManagerMock);
  XCTAssertEqual(_receivedSocket.isConnected, shouldAccept);
  OCMVerifyAll((id)request);
}

- (void)testRefuseSocket {
  [self checkOpenSocketWithShouldAccept:NO];
}

- (void)testAcceptSocket {
  [self checkOpenSocketWithShouldAccept:YES];
}

- (void)testTwoConnectionRequests {
  [self checkOpenSocketWithShouldAccept:YES];
  GNSSocket *firstSocket = _receivedSocket;
  OCMExpect([_socketDelegateMock socket:firstSocket
                 didDisconnectWithError:[OCMArg checkWithBlock:^BOOL(NSError *error) {
                   return [error.domain isEqualToString:kGNSSocketsErrorDomain] &&
                          error.code == GNSErrorNewInviteToConnectReceived;
                 }]]);
  OCMExpect([_peripheralManagerMock socketDidDisconnect:firstSocket]);
  [self checkOpenSocketWithShouldAccept:YES
                                central:firstSocket.peerAsCentral];
  XCTAssertNotEqual(firstSocket, _receivedSocket);
  XCTAssertFalse(firstSocket.isConnected);
}

#pragma mark - Socket receive data

- (NSData *)generateDataWithSize:(uint32_t)length {
  NSMutableData *result = [NSMutableData dataWithCapacity:length];
  unsigned char byte = 0;
  for (NSInteger ii = 0; ii < length; ii++) {
    [result appendBytes:&byte length:sizeof(byte)];
    byte++;
  }
  return result;
}

- (void)testReceiveEmptyMessage {
  [self checkOpenSocketWithShouldAccept:YES];

  NSData *expectedMessage = [self generateDataWithSize:0];
  OCMExpect([_socketDelegateMock socket:_receivedSocket didReceiveData:expectedMessage]);

  GNSWeaveDataPacket *dataPacket =
      [[GNSWeaveDataPacket alloc] initWithPacketCounter:_receivedSocket.receivePacketCounter
                                            firstPacket:YES
                                             lastPacket:YES
                                                   data:expectedMessage];
  CBATTRequest *request = OCMStrictClassMock([CBATTRequest class]);
  OCMStub([request value]).andReturn([dataPacket serialize]);
  [self setupRequest:request
      withCharacteristic:_peripheralServiceManager.weaveIncomingCharacteristic
                 central:_receivedSocket.peerAsCentral];
  [_peripheralServiceManager processWriteRequest:request];
  XCTAssertTrue(_receivedSocket.isConnected);
  OCMVerifyAll((id)request);
}

- (void)simulateReceiveMessageWithSize:(uint32_t)size
                            packetSize:(NSUInteger)packetSize
                               counter:(UInt8)counter {
  NSData *expectedMessage = [self generateDataWithSize:size];
  OCMExpect([_socketDelegateMock socket:_receivedSocket didReceiveData:expectedMessage]);

  NSUInteger offset = 0;
  UInt8 receivePacketCounter = counter;
  while (offset < size) {
    GNSWeaveDataPacket *dataPacket =
        [GNSWeaveDataPacket dataPacketWithPacketCounter:receivePacketCounter
                                             packetSize:packetSize
                                                   data:expectedMessage
                                                 offset:&offset];
    CBATTRequest *request = OCMStrictClassMock([CBATTRequest class]);
    OCMStub([request value]).andReturn([dataPacket serialize]);
    [self setupRequest:request
        withCharacteristic:_peripheralServiceManager.weaveIncomingCharacteristic
                   central:_receivedSocket.peerAsCentral];
    [_peripheralServiceManager processWriteRequest:request];
    XCTAssertTrue(_receivedSocket.isConnected);
    OCMVerifyAll((id)request);
    receivePacketCounter = (receivePacketCounter + 1) % kGNSMaxPacketCounterValue;
  }
}

- (void)testReceiveSinglePacketMessage {
  _packetSize = 100;
  [self checkOpenSocketWithShouldAccept:YES];
  [self simulateReceiveMessageWithSize:99
                            packetSize:_packetSize
                               counter:_receivedSocket.receivePacketCounter];
}

- (void)testReceiveTwoSinglePacketMessages {
  _packetSize = 100;
  [self checkOpenSocketWithShouldAccept:YES];
  [self simulateReceiveMessageWithSize:1
                            packetSize:_packetSize
                               counter:_receivedSocket.receivePacketCounter];
  [self simulateReceiveMessageWithSize:98
                            packetSize:_packetSize
                               counter:_receivedSocket.receivePacketCounter];
}

- (void)testReceiveMultiplePacketMessage {
  _packetSize = 100;
  [self checkOpenSocketWithShouldAccept:YES];
  [self simulateReceiveMessageWithSize:101
                            packetSize:_packetSize
                               counter:_receivedSocket.receivePacketCounter];
}

- (void)testReceiveSeveralMultiplePacketMessage {
  _packetSize = 100;
  [self checkOpenSocketWithShouldAccept:YES];
  [self simulateReceiveMessageWithSize:101
                            packetSize:_packetSize
                               counter:_receivedSocket.receivePacketCounter];
  [self simulateReceiveMessageWithSize:198
                            packetSize:_packetSize
                               counter:_receivedSocket.receivePacketCounter];
  [self simulateReceiveMessageWithSize:1000
                            packetSize:_packetSize
                               counter:_receivedSocket.receivePacketCounter];
}

#pragma mark - Socket send data

- (BOOL)simulateSendMessageWithExpectedData:(NSData *)expectedData
                                 packetSize:(NSUInteger)packetSize
                                 completion:(GNSErrorHandler)completion {
  BOOL sendHasBeenCompleted = YES;
  // The expectedData.length divided by the amount of data that fits a packet, rounded up.
  NSUInteger expectedPacketNumber =
      (expectedData.length / (packetSize - 1)) + (expectedData.length % (packetSize - 1) != 0);
  if (expectedData.length == 0) {
    expectedPacketNumber = 1;
  }
  _packetSize = packetSize;

  [self checkOpenSocketWithShouldAccept:YES];
  UInt8 sendPacketCounter = _receivedSocket.sendPacketCounter;

  __block GNSUpdateValueHandler updateValueHandler = nil;
  OCMStub([_peripheralManagerMock updateOutgoingCharOnSocket:_receivedSocket
                                                 withHandler:[OCMArg checkWithBlock:^(id obj) {
                                                   updateValueHandler = obj;
                                                   return YES;
                                                 }]]);
  // Send the data
  [_receivedSocket sendData:expectedData progressHandler:nil completion:completion];
  XCTAssertNotNil(updateValueHandler);
  if (!updateValueHandler) {
    return NO;
  }
  __block NSData *packetSent = nil;
  OCMStub([_peripheralManagerMock
              updateOutgoingCharacteristic:[OCMArg checkWithBlock:^BOOL(id obj) {
                packetSent = obj;
                return YES;
              }]
                                  onSocket:_receivedSocket])
      .andReturn(YES);

  NSMutableData *sentData = [NSMutableData data];
  for (NSUInteger i = 0; i < expectedPacketNumber; i++) {
    // Cleanup |updateValueHandler| before calling it. So a new handler can be received if needed.
    GNSUpdateValueHandler tmpUpdateValueHandler = updateValueHandler;
    updateValueHandler = nil;
    XCTAssertNotNil(tmpUpdateValueHandler);
    tmpUpdateValueHandler(_peripheralManagerMock);
    NSError *error = nil;
    XCTAssertLessThanOrEqual(packetSent.length, packetSize);
    GNSWeavePacket *packet = [GNSWeavePacket parseData:packetSent error:&error];
    XCTAssertNil(error);
    XCTAssertNotNil(packet);
    XCTAssertTrue([packet isKindOfClass:[GNSWeaveDataPacket class]]);
    GNSWeaveDataPacket *dataPacket = (GNSWeaveDataPacket *)packet;
    NSLog(@"packetCounter = %d", dataPacket.packetCounter);
    XCTAssertEqual(dataPacket.packetCounter, sendPacketCounter);
    sendPacketCounter = (sendPacketCounter + 1) % kGNSMaxPacketCounterValue;
    if (i == 0) {
      XCTAssertTrue(dataPacket.isFirstPacket);
    } else {
      XCTAssertFalse(dataPacket.isFirstPacket);
    }
    [sentData appendData:dataPacket.data];
    if (i == expectedPacketNumber - 1) {
      XCTAssertTrue(dataPacket.isLastPacket);
    } else {
      XCTAssertFalse(dataPacket.isLastPacket);
    }
  }
  XCTAssertEqual((int)_receivedSocket.sendPacketCounter, (int)sendPacketCounter);
  if (sendHasBeenCompleted) {
    XCTAssertEqualObjects(sentData, expectedData);
  }
  return sendHasBeenCompleted;
}

- (void)simulateMessageWithSize:(uint32_t)size packetSize:(NSUInteger)packetSize {
  __block NSInteger completionCalled = 0;
  NSData *expectedData = [self generateDataWithSize:size];
  BOOL completed = [self simulateSendMessageWithExpectedData:expectedData
                                                  packetSize:packetSize
                                                  completion:^(NSError *error) {
                                                    XCTAssertNil(error);
                                                    completionCalled++;
                                                  }];
  XCTAssertTrue(completed);
  XCTAssertEqual(completionCalled, 1);
}

- (void)testSendEmptyMessage {
  [self simulateMessageWithSize:0 packetSize:30];
}

// The header has 1 byte.
- (void)testSendSinglePacketMessage {
  [self simulateMessageWithSize:29 packetSize:30];
}

// The header has 1 byte.
- (void)testSendTwoPacketMessage {
  [self simulateMessageWithSize:30 packetSize:30];
}

- (void)testSendMessagesWithSeveralPacketSizes {
  for (NSUInteger packetSize = 20; packetSize < 50; packetSize++) {
    [self simulateMessageWithSize:200 packetSize:packetSize];
  }
}

- (void)testSendMessagesWithSeveralSizes {
  for (uint32_t size = 100; size < 1000; size += 50) {
    [self simulateMessageWithSize:size packetSize:120];
  }
}

#pragma mark - Socket disconnect

- (void)testUnsubscribeToDisconnect {
  [self checkOpenSocketWithShouldAccept:YES];
  XCTAssertTrue(_receivedSocket.isConnected);
  // Unsubscribing to the incoming characteristic: nothing should happen.
  [_peripheralServiceManager central:_receivedSocket.peerAsCentral
      didUnsubscribeFromCharacteristic:_peripheralServiceManager.weaveIncomingCharacteristic];
  XCTAssertTrue(_receivedSocket.isConnected);
  // Unsubscribing to the outgoing characteristic: the socket should be disconnected.
  OCMExpect([_socketDelegateMock socket:_receivedSocket didDisconnectWithError:nil]);
  OCMExpect([_peripheralManagerMock socketDidDisconnect:_receivedSocket]);
  [_peripheralServiceManager central:_receivedSocket.peerAsCentral
      didUnsubscribeFromCharacteristic:_peripheralServiceManager.weaveOutgoingCharacteristic];
  XCTAssertFalse(_receivedSocket.isConnected);
}

- (void)testDisconnect {
  [self checkOpenSocketWithShouldAccept:YES];
  __block GNSUpdateValueHandler updateValueHandler = nil;
  OCMExpect([_peripheralManagerMock
      updateOutgoingCharOnSocket:_receivedSocket
                     withHandler:[OCMArg checkWithBlock:^(id handler) {
                       updateValueHandler = handler;
                       return YES;
                     }]]);
  OCMExpect([_peripheralManagerMock socketDidDisconnect:_receivedSocket]);
  UInt8 sendPacketCounter = _receivedSocket.sendPacketCounter;
  [_receivedSocket disconnect];
  XCTAssertNotNil(updateValueHandler);
  if (!updateValueHandler) {
    return;
  }
  GNSWeaveErrorPacket *errorPacket =
      [[GNSWeaveErrorPacket alloc] initWithPacketCounter:sendPacketCounter];
  OCMExpect([_peripheralManagerMock updateOutgoingCharacteristic:[errorPacket serialize]
                                                        onSocket:_receivedSocket])
      .andReturn(YES);
  OCMExpect([_socketDelegateMock socket:_receivedSocket didDisconnectWithError:nil]);
  XCTAssertEqual(updateValueHandler(_peripheralManagerMock), GNSOutgoingCharUpdateNoReschedule);
  XCTAssertFalse(_receivedSocket.isConnected);
}

- (void)testDisconnectWithErrorToSendDisconnectPacket {
  [self checkOpenSocketWithShouldAccept:YES];
  __block GNSUpdateValueHandler updateValueHandler = nil;
  OCMExpect([_peripheralManagerMock
      updateOutgoingCharOnSocket:_receivedSocket
                     withHandler:[OCMArg checkWithBlock:^(id handler) {
                       updateValueHandler = handler;
                       return YES;
                     }]]);
  OCMExpect([_peripheralManagerMock socketDidDisconnect:_receivedSocket]);
  UInt8 sendPacketCounter = _receivedSocket.sendPacketCounter;
  [_receivedSocket disconnect];
  XCTAssertNotNil(updateValueHandler);
  if (!updateValueHandler) {
    return;
  }
  GNSWeaveErrorPacket *errorPacket =
      [[GNSWeaveErrorPacket alloc] initWithPacketCounter:sendPacketCounter];
  OCMExpect([_peripheralManagerMock updateOutgoingCharacteristic:[errorPacket serialize]
                                                        onSocket:_receivedSocket])
      .andReturn(NO);
  XCTAssertEqual(updateValueHandler(_peripheralManagerMock), GNSOutgoingCharUpdateScheduleLater);
  XCTAssertTrue(_receivedSocket.isConnected);
  OCMExpect([_peripheralManagerMock updateOutgoingCharacteristic:[errorPacket serialize]
                                                        onSocket:_receivedSocket])
      .andReturn(YES);
  OCMExpect([_socketDelegateMock socket:_receivedSocket didDisconnectWithError:nil]);
  XCTAssertEqual(updateValueHandler(_peripheralManagerMock), GNSOutgoingCharUpdateNoReschedule);
  XCTAssertFalse(_receivedSocket.isConnected);
}

- (void)testSubscribeToOutgoingCharacteristic {
  id centralMock = OCMStrictClassMock([CBCentral class]);
  id characteristic = OCMStrictClassMock([CBMutableCharacteristic class]);
  CBUUID *uuid = _peripheralServiceManager.weaveOutgoingCharacteristic.UUID;
  OCMStub([characteristic UUID]).andReturn(uuid);
  OCMStub([_cbPeripheralManagerMock
      setDesiredConnectionLatency:CBPeripheralManagerConnectionLatencyLow
                       forCentral:centralMock]);
  [_peripheralServiceManager central:centralMock didSubscribeToCharacteristic:characteristic];
  OCMVerifyAll(centralMock);
}

@end
