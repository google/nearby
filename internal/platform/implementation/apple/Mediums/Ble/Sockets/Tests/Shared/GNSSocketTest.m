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

#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Shared/GNSSocket+Private.h"
#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Shared/GNSWeavePacket.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@interface GNSSocketTest : XCTestCase {
  GNSSocket *_socket;
  id<GNSSocketOwner> _socketOwner;
  CBCentral *_centralPeerMock;
  id<GNSSocketDelegate> _socketDelegate;
}
@end

@implementation GNSSocketTest

- (void)setUp {
  _centralPeerMock = OCMStrictClassMock([CBCentral class]);
  NSUUID *identifier = [NSUUID UUID];
  OCMStub([_centralPeerMock identifier]).andReturn(identifier);
  _socketOwner = OCMStrictProtocolMock(@protocol(GNSSocketOwner));
  _socket = [[GNSSocket alloc] initWithOwner:_socketOwner centralPeer:_centralPeerMock];
  _socketDelegate = OCMStrictProtocolMock(@protocol(GNSSocketDelegate));
  _socket.delegate = _socketDelegate;
  XCTAssertFalse(_socket.connected);
}

- (void)tearDown {
  __weak GNSSocket *weakSocket = _socket;
  OCMExpect([_socketOwner socketWillBeDeallocated:[OCMArg isNotNil]]);
  _socket = nil;
  XCTAssertNil(weakSocket);
  OCMVerifyAll((id)_centralPeerMock);
  OCMVerifyAll((id)_socketOwner);
  OCMVerifyAll((id)_socketDelegate);
}

- (void)connectSocket {
  OCMExpect([_socketDelegate socketDidConnect:_socket]);
  [_socket didConnect];
  XCTAssertTrue(_socket.connected);
}

- (void)testPeripheralPeer {
  CBPeripheral *peripheral = OCMStrictClassMock([CBPeripheral class]);
  GNSSocket *socket = [[GNSSocket alloc] initWithOwner:_socketOwner peripheralPeer:peripheral];
  XCTAssertEqual(socket.peerAsPeripheral, peripheral);
  OCMExpect([_socketOwner socketWillBeDeallocated:[OCMArg isNotNil]]);
}

- (void)testCentralPeer {
  CBCentral *central = OCMStrictClassMock([CBCentral class]);
  GNSSocket *socket = [[GNSSocket alloc] initWithOwner:_socketOwner centralPeer:central];
  XCTAssertEqual(socket.peerAsCentral, central);
  OCMExpect([_socketOwner socketWillBeDeallocated:[OCMArg isNotNil]]);
}

- (void)testDisconnect {
  [self connectSocket];
  OCMExpect([_socketOwner disconnectSocket:_socket]);
  [_socket disconnect];
  XCTAssertTrue(_socket.connected);
  OCMExpect([_socketDelegate socket:_socket didDisconnectWithError:nil]);
  [_socket didDisconnectWithError:nil];
  XCTAssertFalse(_socket.connected);
}

- (void)testDisconnectWithError {
  [self connectSocket];
  OCMExpect([_socketOwner disconnectSocket:_socket]);
  [_socket disconnect];
  XCTAssertTrue(_socket.connected);
  NSError *error = [NSError errorWithDomain:@"domain" code:-42 userInfo:nil];
  OCMExpect([_socketDelegate socket:_socket didDisconnectWithError:error]);
  [_socket didDisconnectWithError:error];
  XCTAssertFalse(_socket.connected);
}

#pragma mark - Receive Data

- (void)testReceiveDataWithOnePacket {
  [self connectSocket];
  XCTAssertFalse([_socket waitingForIncomingData]);
  NSData *data = [@"Some data to receive" dataUsingEncoding:NSUTF8StringEncoding];
  OCMExpect([_socketDelegate socket:_socket didReceiveData:data]);
  GNSWeaveDataPacket *packet =
      [[GNSWeaveDataPacket alloc] initWithPacketCounter:1 firstPacket:YES lastPacket:YES data:data];
  [_socket didReceiveIncomingWeaveDataPacket:packet];
  XCTAssertFalse([_socket waitingForIncomingData]);
}

- (void)testReceiveDataWithTwoPackets {
  [self connectSocket];
  XCTAssertFalse([_socket waitingForIncomingData]);
  NSData *firstChunk = [@"Some data " dataUsingEncoding:NSUTF8StringEncoding];
  NSData *secondChunk = [@"to receive" dataUsingEncoding:NSUTF8StringEncoding];
  NSMutableData *totalData = [NSMutableData data];
  [totalData appendData:firstChunk];
  GNSWeaveDataPacket *firstPacket = [[GNSWeaveDataPacket alloc] initWithPacketCounter:1
                                                                          firstPacket:YES
                                                                           lastPacket:NO
                                                                                 data:firstChunk];
  [_socket didReceiveIncomingWeaveDataPacket:firstPacket];
  XCTAssertTrue([_socket waitingForIncomingData]);
  OCMExpect([_socketDelegate socket:_socket didReceiveData:totalData]);
  [totalData appendData:secondChunk];
  GNSWeaveDataPacket *secondPacket = [[GNSWeaveDataPacket alloc] initWithPacketCounter:1
                                                                           firstPacket:NO
                                                                            lastPacket:YES
                                                                                  data:secondChunk];
  [_socket didReceiveIncomingWeaveDataPacket:secondPacket];
  XCTAssertFalse([_socket waitingForIncomingData]);
}

- (void)testReceiveDataWithOneDataPacketAndDisconnect {
  [self connectSocket];
  XCTAssertFalse([_socket waitingForIncomingData]);
  NSData *firstChunk = [@"Some data " dataUsingEncoding:NSUTF8StringEncoding];
  GNSWeaveDataPacket *firstPacket = [[GNSWeaveDataPacket alloc] initWithPacketCounter:1
                                                                          firstPacket:YES
                                                                           lastPacket:NO
                                                                                 data:firstChunk];
  [_socket didReceiveIncomingWeaveDataPacket:firstPacket];
  XCTAssertTrue([_socket waitingForIncomingData]);
  OCMExpect([_socketDelegate socket:_socket didDisconnectWithError:nil]);
  [_socket didDisconnectWithError:nil];
  XCTAssertFalse(_socket.connected);
  XCTAssertFalse([_socket waitingForIncomingData]);
}

#pragma mark - Send Data

- (void)testSendDataWithOnePacket {
  _socket.packetSize = 100;
  UInt8 sendPacketCounter = _socket.sendPacketCounter;
  [self connectSocket];
  NSData *data = [@"Some data to send" dataUsingEncoding:NSUTF8StringEncoding];
  OCMStub([_socketOwner socketMaximumUpdateValueLength:[self checkSocketBlock]]).andReturn(100);
  __block GNSUpdateValueHandler handler = nil;
  OCMExpect([_socketOwner socket:_socket
    addOutgoingCharUpdateHandler:[OCMArg checkWithBlock:^BOOL(id obj) {
    handler = obj;
    return YES;
  }]]);
  __block BOOL completionCalledCount = 0;
  __block BOOL progressHandlerCount = 0;
  [_socket sendData:data
    progressHandler:^void(float progress) {
      progressHandlerCount++;
    }
         completion:^(NSError *error) {
           XCTAssertNil(error);
           completionCalledCount++;
         }];
  XCTAssertTrue([_socket isSendOperationInProgress]);
  XCTAssertNotNil(handler);
  if (!handler) {
    return;
  }
  NSUInteger offset = 0;
  GNSWeaveDataPacket *packet = [GNSWeaveDataPacket dataPacketWithPacketCounter:sendPacketCounter
                                                                    packetSize:_socket.packetSize
                                                                          data:data
                                                                        offset:&offset];
  NSData *packetData = [packet serialize];
  OCMExpect([_socketOwner socket:_socket sendData:packetData]).andReturn(YES);
  XCTAssertEqual(handler(), GNSOutgoingCharUpdateNoReschedule);
  XCTAssertEqual(completionCalledCount, 1);
  XCTAssertEqual(progressHandlerCount, 1);
  XCTAssertFalse([_socket isSendOperationInProgress]);
}

- (void)testSendDataWithTwoPackets {
  _socket.packetSize = 20;
  UInt8 sendPacketCounter = _socket.sendPacketCounter;
  [self connectSocket];
  NSData *data = [@"Some larger data to send" dataUsingEncoding:NSUTF8StringEncoding];
  OCMStub([_socketOwner socketMaximumUpdateValueLength:[self checkSocketBlock]]).andReturn(20);
  __block GNSUpdateValueHandler handler = nil;
  OCMExpect([_socketOwner socket:_socket
    addOutgoingCharUpdateHandler:[OCMArg checkWithBlock:^BOOL(id obj) {
    handler = obj;
    return YES;
  }]]);
  __block BOOL completionCalledCount = 0;
  [_socket sendData:data progressHandler:nil completion:^(NSError *error) {
    XCTAssertNil(error);
    completionCalledCount++;
  }];
  XCTAssertNotNil(handler);
  if (!handler) {
    return;
  }
  NSUInteger offset = 0;
  GNSWeaveDataPacket *firstPacket =
      [GNSWeaveDataPacket dataPacketWithPacketCounter:sendPacketCounter
                                           packetSize:_socket.packetSize
                                                 data:data
                                               offset:&offset];
  NSData *firstPacketData = [firstPacket serialize];
  OCMExpect([_socketOwner socket:_socket sendData:firstPacketData]).andReturn(YES);
  OCMExpect([_socketOwner socket:_socket
      addOutgoingCharUpdateHandler:[OCMArg checkWithBlock:^BOOL(id obj) {
        handler = obj;
        return YES;
      }]]);
  XCTAssertEqual(handler(), GNSOutgoingCharUpdateNoReschedule);
  XCTAssertNotNil(handler);
  if (!handler) {
    return;
  }
  XCTAssertEqual(completionCalledCount, 0);

  GNSWeaveDataPacket *secondPacket =
      [GNSWeaveDataPacket dataPacketWithPacketCounter:sendPacketCounter + 1
                                           packetSize:_socket.packetSize
                                                 data:data
                                               offset:&offset];
  NSData *secondPacketData = [secondPacket serialize];
  OCMExpect([_socketOwner socket:_socket sendData:secondPacketData]).andReturn(YES);
  XCTAssertEqual(handler(), GNSOutgoingCharUpdateNoReschedule);
  XCTAssertEqual(completionCalledCount, 1);
  XCTAssertFalse([_socket isSendOperationInProgress]);
}

- (void)testSendDataWithBluetoothRetry {
  _socket.packetSize = 100;
  UInt8 sentPacketCounter = _socket.sendPacketCounter;
  [self connectSocket];
  NSData *data = [@"Some data to send" dataUsingEncoding:NSUTF8StringEncoding];
  OCMStub([_socketOwner socketMaximumUpdateValueLength:[self checkSocketBlock]]).andReturn(100);
  __block GNSUpdateValueHandler handler = nil;
  OCMExpect([_socketOwner socket:_socket
    addOutgoingCharUpdateHandler:[OCMArg checkWithBlock:^BOOL(id obj) {
    handler = obj;
    return YES;
  }]]);
  __block BOOL completionCalledCount = 0;
  [_socket sendData:data progressHandler:nil completion:^(NSError *error) {
    XCTAssertNil(error);
    completionCalledCount++;
  }];
  XCTAssertTrue([_socket isSendOperationInProgress]);
  XCTAssertNotNil(handler);
  if (!handler) {
    return;
  }
  NSUInteger offset = 0;
  GNSWeaveDataPacket *packet = [GNSWeaveDataPacket dataPacketWithPacketCounter:sentPacketCounter
                                                                    packetSize:_socket.packetSize
                                                                          data:data
                                                                        offset:&offset];
  NSData *packetData = [packet serialize];
  OCMExpect([_socketOwner socket:_socket sendData:packetData]).andReturn(NO);
  XCTAssertEqual(handler(), GNSOutgoingCharUpdateScheduleLater);
  XCTAssertEqual(completionCalledCount, 0);
  XCTAssertTrue([_socket isSendOperationInProgress]);
  OCMExpect([_socketOwner socket:_socket sendData:packetData]).andReturn(YES);
  XCTAssertEqual(handler(), GNSOutgoingCharUpdateNoReschedule);
  XCTAssertEqual(completionCalledCount, 1);
  XCTAssertFalse([_socket isSendOperationInProgress]);
}

- (void)testDisconnectAndSendDataWithOneChunk {
  [self connectSocket];
  OCMExpect([_socketDelegate socket:_socket didDisconnectWithError:nil]);
  [_socket didDisconnectWithError:nil];
  XCTAssertFalse(_socket.connected);
  NSData *data = [@"Some data to send" dataUsingEncoding:NSUTF8StringEncoding];
  OCMStub([_socketOwner socketMaximumUpdateValueLength:[self checkSocketBlock]]).andReturn(100);
  __block BOOL completionCalledCount = 0;
  [_socket sendData:data progressHandler:nil completion:^(NSError *error) {
    XCTAssertNotNil(error);
    XCTAssertEqualObjects(error.domain, kGNSSocketsErrorDomain);
    XCTAssertEqual(error.code, GNSErrorNoConnection);
    completionCalledCount++;
  }];
  XCTAssertEqual(completionCalledCount, 1);
  XCTAssertFalse([_socket isSendOperationInProgress]);
}

- (void)testSendDataWithTwoChunksAndDisconnectBetweenChunk {
  _socket.packetSize = 20;
  UInt8 sentPacketCounter = _socket.sendPacketCounter;
  [self connectSocket];
  NSData *data = [@"Some larger data to send" dataUsingEncoding:NSUTF8StringEncoding];
  OCMStub([_socketOwner socketMaximumUpdateValueLength:[self checkSocketBlock]]).andReturn(20);
  __block GNSUpdateValueHandler handler = nil;
  OCMExpect([_socketOwner socket:_socket
    addOutgoingCharUpdateHandler:[OCMArg checkWithBlock:^BOOL(id obj) {
    handler = obj;
    return YES;
  }]]);
  __block BOOL completionCalledCount = 0;
  [_socket sendData:data progressHandler:nil completion:^(NSError *error) {
    XCTAssertNotNil(error);
    XCTAssertEqualObjects(error.domain, kGNSSocketsErrorDomain);
    XCTAssertEqual(error.code, GNSErrorNoConnection);
    completionCalledCount++;
  }];
  XCTAssertNotNil(handler);
  if (!handler) {
    return;
  }
  NSUInteger offset = 0;
  GNSWeaveDataPacket *packet = [GNSWeaveDataPacket dataPacketWithPacketCounter:sentPacketCounter
                                                                    packetSize:_socket.packetSize
                                                                          data:data
                                                                        offset:&offset];
  NSData *packetData = [packet serialize];
  OCMExpect([_socketOwner socket:_socket sendData:packetData]).andReturn(YES);
  OCMExpect([_socketOwner socket:_socket
      addOutgoingCharUpdateHandler:[OCMArg checkWithBlock:^BOOL(id obj) {
        handler = obj;
        return YES;
      }]]);
  XCTAssertEqual(handler(), GNSOutgoingCharUpdateNoReschedule);
  XCTAssertNotNil(handler);
  if (!handler) {
    return;
  }
  XCTAssertEqual(completionCalledCount, 0);
  OCMExpect([_socketDelegate socket:_socket didDisconnectWithError:nil]);
  [_socket didDisconnectWithError:nil];
  XCTAssertFalse(_socket.connected);
  XCTAssertEqual(handler(), GNSOutgoingCharUpdateNoReschedule);
  XCTAssertEqual(completionCalledCount, 1);
  XCTAssertFalse([_socket isSendOperationInProgress]);
}

#pragma mark - Helpers

- (id)checkSocketBlock {
  // This method create a OCMArg without retaining the parameter.
  __weak GNSSocket *weakSocket = _socket;
  return [OCMArg checkWithBlock:^BOOL(id object) {
    return object == weakSocket;
  }];
}

@end
