// Copyright 2025 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSSocket+Private.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSSocket.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSWeavePacket.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@protocol GNSSocketTestDelegate <GNSSocketDelegate, GNSWeavePacketHandler>
- (void)socketDidConnect:(GNSSocket *)socket;
- (void)socket:(GNSSocket *)socket didReceiveWeavePacket:(GNSWeavePacket *)packet;
- (void)socket:(GNSSocket *)socket didDisconnectWithError:(NSError *)error;
@end

@interface GNSSocketTest : XCTestCase
@property(nonatomic) GNSSocket *socket;
@property(nonatomic) id<GNSSocketOwner> mockOwner;
@property(nonatomic) id<GNSSocketTestDelegate> mockDelegate;
@property(nonatomic) dispatch_queue_t testQueue;

@end

@implementation GNSSocketTest

- (void)setUp {
  [super setUp];
  self.mockOwner = OCMProtocolMock(@protocol(GNSSocketOwner));
  self.mockDelegate = OCMProtocolMock(@protocol(GNSSocketTestDelegate));
  self.testQueue = dispatch_queue_create("com.google.nearby.GNSSocketTest", DISPATCH_QUEUE_SERIAL);
}

- (void)tearDown {
  self.socket = nil;
  self.mockOwner = nil;
  self.mockDelegate = nil;
  self.testQueue = nil;
  [super tearDown];
}

#pragma mark - Initialization

- (void)testInitWithCentralPeer {
  CBCentral *centralPeer = OCMClassMock([CBCentral class]);
  OCMStub([centralPeer identifier]).andReturn([NSUUID UUID]);
  self.socket = [[GNSSocket alloc] initWithOwner:self.mockOwner
                                     centralPeer:centralPeer
                                           queue:self.testQueue];
  XCTAssertNotNil(self.socket);
  XCTAssertEqualObjects(self.socket.owner, self.mockOwner);
  XCTAssertEqual(self.socket.packetSize, kGNSMinSupportedPacketSize);
  XCTAssertNotNil(self.socket.socketIdentifier);
  XCTAssertEqual(self.socket.queue, self.testQueue);
}

- (void)testInitWithPeripheralPeer {
  CBPeripheral *peripheralPeer = OCMClassMock([CBPeripheral class]);
  OCMStub([peripheralPeer identifier]).andReturn([NSUUID UUID]);
  self.socket = [[GNSSocket alloc] initWithOwner:self.mockOwner
                                  peripheralPeer:peripheralPeer
                                           queue:self.testQueue];
  XCTAssertNotNil(self.socket);
  XCTAssertEqualObjects(self.socket.owner, self.mockOwner);
  XCTAssertEqual(self.socket.packetSize, kGNSMinSupportedPacketSize);
  XCTAssertNotNil(self.socket.socketIdentifier);
  XCTAssertEqual(self.socket.queue, self.testQueue);
}

- (void)testDealloc {
  OCMExpect([self.mockOwner socketWillBeDeallocated:OCMOCK_ANY]);
  GNSSocket *__weak weakSocket;
  @autoreleasepool {
    CBPeripheral *peripheralPeer = OCMClassMock([CBPeripheral class]);
    OCMStub([peripheralPeer identifier]).andReturn([NSUUID UUID]);
    GNSSocket *socket = [[GNSSocket alloc] initWithOwner:self.mockOwner
                                          peripheralPeer:peripheralPeer
                                                   queue:self.testQueue];
    weakSocket = socket;
  }
  OCMVerifyAll(self.mockOwner);
  XCTAssertNil(weakSocket);
}

#pragma mark - Peer Type

- (void)connectSocket {
  self.socket = [self createSocket];
  self.socket.delegate = self.mockDelegate;
  OCMExpect([self.mockDelegate socketDidConnect:self.socket]);
  [self.socket didConnect];
}

- (void)testPeerAsPeripheral {
  CBPeripheral *peripheralPeer = OCMClassMock([CBPeripheral class]);
  OCMStub([peripheralPeer identifier]).andReturn([NSUUID UUID]);
  self.socket = [[GNSSocket alloc] initWithOwner:self.mockOwner
                                  peripheralPeer:peripheralPeer
                                           queue:self.testQueue];
  CBPeripheral *result = [self.socket peerAsPeripheral];
  XCTAssertEqual(result, peripheralPeer);
}

- (void)testPeerAsCentral {
  CBCentral *centralPeer = OCMClassMock([CBCentral class]);
  OCMStub([centralPeer identifier]).andReturn([NSUUID UUID]);
  self.socket = [[GNSSocket alloc] initWithOwner:self.mockOwner
                                     centralPeer:centralPeer
                                           queue:self.testQueue];
  CBCentral *result = [self.socket peerAsCentral];
  XCTAssertEqual(result, centralPeer);
}

#pragma mark - Connection

- (void)testDidConnect {
  [self connectSocket];
  XCTAssertTrue(self.socket.isConnected);
  OCMVerifyAll(self.mockDelegate);
}

- (void)testDisconnect {
  [self connectSocket];
  OCMExpect([self.mockOwner disconnectSocket:self.socket]);
  [self.socket disconnect];
  XCTAssertTrue(self.socket.isConnected);
  OCMExpect([self.mockDelegate socket:self.socket didDisconnectWithError:nil]);
  [self.socket didDisconnectWithError:nil];
  XCTAssertFalse(self.socket.isConnected);
  OCMVerifyAll(self.mockOwner);
}

- (void)testDidDisconnectWithError {
  [self connectSocket];
  OCMExpect([self.mockOwner disconnectSocket:self.socket]);
  [self.socket disconnect];
  NSError *testError = [NSError errorWithDomain:@"TestDomain" code:1 userInfo:nil];
  OCMExpect([self.mockDelegate socket:self.socket didDisconnectWithError:testError]);
  [self.socket didDisconnectWithError:testError];
  XCTAssertFalse(self.socket.isConnected);
  OCMVerifyAll(self.mockDelegate);
}

- (void)testDisconnectWhenAlreadyDisconnected {
  self.socket = [self createSocket];
  OCMReject([self.mockOwner disconnectSocket:self.socket]);
  [self.socket disconnect];
  OCMVerifyAll(self.mockOwner);
}


#pragma mark - Receive Data

- (void)testReceiveDataWithOnePacket {
  [self connectSocket];
  XCTAssertTrue(self.socket.isConnected);
  XCTAssertFalse([self.socket waitingForIncomingData]);
  NSData *data = [@"Some data to receive" dataUsingEncoding:NSUTF8StringEncoding];
  OCMExpect([self.mockDelegate socket:self.socket didReceiveData:data]);
  GNSWeaveDataPacket *packet = [[GNSWeaveDataPacket alloc] initWithPacketCounter:1
                                                                     firstPacket:YES
                                                                      lastPacket:YES
                                                                            data:data];
  [self.socket didReceiveIncomingWeaveDataPacket:packet];

  OCMVerifyAll(self.mockDelegate);
}

- (void)testReceiveDataWithTwoPackets {
  [self connectSocket];
  XCTAssertTrue(self.socket.isConnected);
  XCTAssertFalse([_socket waitingForIncomingData]);
  NSData *firstChunk = [@"Some data " dataUsingEncoding:NSUTF8StringEncoding];
  NSData *secondChunk = [@"to receive" dataUsingEncoding:NSUTF8StringEncoding];
  NSMutableData *totalData = [NSMutableData data];
  [totalData appendData:firstChunk];
  GNSWeaveDataPacket *firstPacket = [[GNSWeaveDataPacket alloc] initWithPacketCounter:1
                                                                          firstPacket:YES
                                                                           lastPacket:NO
                                                                                 data:firstChunk];
  [self.socket didReceiveIncomingWeaveDataPacket:firstPacket];
  XCTAssertTrue([self.socket waitingForIncomingData]);
  OCMExpect([self.mockDelegate socket:self.socket didReceiveData:totalData]);
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
  XCTAssertTrue(self.socket.isConnected);
  XCTAssertFalse([self.socket waitingForIncomingData]);
  NSData *firstChunk = [@"Some data " dataUsingEncoding:NSUTF8StringEncoding];
  GNSWeaveDataPacket *firstPacket = [[GNSWeaveDataPacket alloc] initWithPacketCounter:1
                                                                          firstPacket:YES
                                                                           lastPacket:NO
                                                                                 data:firstChunk];
  [self.socket didReceiveIncomingWeaveDataPacket:firstPacket];
  XCTAssertTrue([self.socket waitingForIncomingData]);
  OCMExpect([self.mockDelegate socket:self.socket didDisconnectWithError:nil]);
  [self.socket didDisconnectWithError:nil];
  XCTAssertFalse(self.socket.isConnected);
  XCTAssertFalse([self.socket waitingForIncomingData]);
}

- (void)testReceiveDataWhenNotConnected {
  self.socket = [self createSocket];
  self.socket.delegate = self.mockDelegate;
  NSData *testData = [@"Hello, World!" dataUsingEncoding:NSUTF8StringEncoding];
  GNSWeaveDataPacket *packet = [[GNSWeaveDataPacket alloc] initWithPacketCounter:0
                                                                     firstPacket:YES
                                                                      lastPacket:YES
                                                                            data:testData];
  OCMReject([self.mockDelegate socket:self.socket didReceiveWeavePacket:[OCMArg any]]);
  [self.socket didReceiveIncomingWeaveDataPacket:packet];
  OCMVerifyAll(self.mockDelegate);
}

#pragma mark - Send Data

- (void)testSendData {
  self.socket = [self createSocket];
  [self.socket didConnect];
  NSData *testData = [@"Hello, World!" dataUsingEncoding:NSUTF8StringEncoding];

  OCMExpect([self.mockOwner sendData:[OCMArg any] socket:self.socket completion:[OCMArg any]])
      .andDo(^(id<GNSSocketOwner> localSelf, NSData *data, GNSSocket *socket,
               GNSErrorHandler completion) {
        NSError *error;
        GNSWeavePacket *packet = [GNSWeavePacket parseData:data error:&error];
        XCTAssertNil(error);
        XCTAssertTrue([packet isKindOfClass:[GNSWeaveDataPacket class]]);
        GNSWeaveDataPacket *dataPacket = (GNSWeaveDataPacket *)packet;
        XCTAssertEqualObjects(dataPacket.data, testData);
        completion(nil);
      });

  XCTestExpectation *expectation = [self expectationWithDescription:@"Data sent"];
  [self.socket sendData:testData
      progressHandler:^(float progress) {
        XCTAssertTrue(progress <= 1.0);
      }
      completion:^(NSError *error) {
        XCTAssertNil(error);
        [expectation fulfill];
      }];
  [self waitForExpectations:@[ expectation ] timeout:1.0];
  OCMVerifyAll(self.mockOwner);
}

- (void)testSendLargeData {
  self.socket = [self createSocket];
  [self.socket didConnect];
  self.socket.packetSize = 20;
  NSMutableString *longString = [NSMutableString string];
  for (int i = 0; i < 100; i++) {
    [longString appendString:@"A"];
  }
  NSData *testData = [longString dataUsingEncoding:NSUTF8StringEncoding];
  __block NSUInteger totalBytesSent = 0;
  __block int numberOfChunks = 0;
  OCMStub([self.mockOwner sendData:[OCMArg any] socket:self.socket completion:[OCMArg any]])
      .andDo(^(id<GNSSocketOwner> localSelf, NSData *data, GNSSocket *socket,
               GNSErrorHandler completion) {
        NSError *error;
        GNSWeavePacket *packet = [GNSWeavePacket parseData:data error:&error];
        XCTAssertNil(error);
        XCTAssertTrue([packet isKindOfClass:[GNSWeaveDataPacket class]]);
        GNSWeaveDataPacket *dataPacket = (GNSWeaveDataPacket *)packet;
        totalBytesSent += dataPacket.data.length;
        numberOfChunks++;
        completion(nil);
      });

  XCTestExpectation *expectation = [self expectationWithDescription:@"Large data sent"];
  [self.socket sendData:testData
      progressHandler:^(float progress) {
      }
      completion:^(NSError *error) {
        XCTAssertNil(error);
        XCTAssertEqual(totalBytesSent, testData.length);
        XCTAssertGreaterThan(numberOfChunks, 1);
        [expectation fulfill];
      }];

  [self waitForExpectations:@[ expectation ] timeout:1.0];
}

- (void)testSendDataWhenAlreadySending {
  self.socket = [self createSocket];
  [self.socket didConnect];
  NSData *testData1 = [@"First data" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *testData2 = [@"Second data" dataUsingEncoding:NSUTF8StringEncoding];

  // Mock the owner to simulate a slow send operation
  OCMStub([self.mockOwner sendData:[OCMArg any] socket:self.socket completion:[OCMArg any]])
      .andDo(^(id<GNSSocketOwner> localSelf, NSData *data, GNSSocket *socket,
               GNSErrorHandler completion) {
        // Simulate a delay
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
                       self.testQueue, ^{
                         completion(nil);
                       });
      });

  XCTestExpectation *expectation1 = [self expectationWithDescription:@"First data started"];
  [self.socket sendData:testData1
        progressHandler:nil
             completion:^(NSError *error) {
               XCTAssertNil(error);
               [expectation1 fulfill];
             }];

  XCTestExpectation *expectation2 = [self expectationWithDescription:@"Second data failed"];
  [self.socket sendData:testData2
        progressHandler:nil
             completion:^(NSError *error) {
               XCTAssertNotNil(error);
               XCTAssertEqual(error.code, GNSErrorOperationInProgress);
               [expectation2 fulfill];
             }];

  // The enforceOrder parameter has been removed.
  [self waitForExpectations:@[ expectation1, expectation2 ] timeout:1.0];
}

- (void)testSendDataWhenDisconnected {
  self.socket = [self createSocket];
  NSData *testData = [@"Test Data" dataUsingEncoding:NSUTF8StringEncoding];
  OCMReject([self.mockOwner sendData:[OCMArg any] socket:self.socket completion:[OCMArg any]]);
  XCTestExpectation *expectation = [self expectationWithDescription:@"Data send failed"];

  [self.socket sendData:testData
        progressHandler:nil
             completion:^(NSError *error) {
               XCTAssertNotNil(error);
               XCTAssertEqual(error.code, GNSErrorNoConnection);
               [expectation fulfill];
             }];
  [self waitForExpectations:@[ expectation ] timeout:1.0];
  OCMVerifyAll(self.mockOwner);
}

#pragma mark - Packet Counter

- (void)testIncrementReceivePacketCounter {
  self.socket = [self createSocket];
  // Assuming the counter is 3-bit and wraps around at 8.
  const int kPacketCounterWrapValue = 8;
  for (int i = 0; i < kGNSMaxPacketCounterValue; i++) {
    [self.socket incrementReceivePacketCounter];
    // The expected value should account for the wrap-around.
    XCTAssertEqual(self.socket.receivePacketCounter, (UInt8)((i + 1) % kPacketCounterWrapValue));
  }
}

- (void)testIncrementSendPacketCounter {
  self.socket = [self createSocket];
  const int kPacketCounterWrapValue = 8;
  for (int i = 0; i < kGNSMaxPacketCounterValue; i++) {
    [self.socket incrementSendPacketCounter];
    XCTAssertEqual(self.socket.sendPacketCounter, (UInt8)((i + 1) % kPacketCounterWrapValue));
  }
}

#pragma mark - Helpers

- (GNSSocket *)createSocket {
  CBPeripheral *peripheralPeer = OCMClassMock([CBPeripheral class]);
  OCMStub([peripheralPeer identifier]).andReturn([NSUUID UUID]);
  return [[GNSSocket alloc] initWithOwner:self.mockOwner
                           peripheralPeer:peripheralPeer
                                    queue:self.testQueue];
}

@end
