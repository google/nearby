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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPConnection.h"

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPStream.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCMBleUtils.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCMConnection.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPFakeInputOutputStream.h"

static const NSTimeInterval kTestTimeout = 1.0;

@interface GNCBLEL2CAPConnection (Testing) <GNCBLEL2CAPStreamDelegate>
- (NSData *_Nullable)extractRealDataFromData:(NSData *)data;
@property(nonatomic) NSUInteger expectedDataLength;
@property(nonatomic) dispatch_queue_t selfQueue;
@end

@interface GNCBLEL2CAPConnectionTest : XCTestCase
@property(nonatomic) GNCBLEL2CAPFakeInputOutputStream *fakeInputOutputStream;
@property(nonatomic) GNCBLEL2CAPStream *stream;
@property(nonatomic) GNCBLEL2CAPConnection *connection;
@property(nonatomic) dispatch_queue_t testCallbackQueue;
@property(nonatomic) NSString *serviceID;
@property(nonatomic) NSData *serviceIDHash;
@end

@implementation GNCBLEL2CAPConnectionTest

- (void)setUp {
  [super setUp];
  _fakeInputOutputStream = [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1024];
  _stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];
  _testCallbackQueue = dispatch_queue_create("com.google.test.callback", DISPATCH_QUEUE_SERIAL);
  _serviceID = @"testServiceID";
  _serviceIDHash = GNCMServiceIDHash(_serviceID);
}

- (void)tearDown {
  _connection = nil;
  [_stream tearDown];
  [_fakeInputOutputStream tearDown];
  _stream = nil;
  [super tearDown];
}

#pragma mark - Test Helpers

- (GNCBLEL2CAPConnection *)createConnectionWithIncoming:(BOOL)incoming {
  GNCBLEL2CAPConnection *connection =
      [GNCBLEL2CAPConnection connectionWithStream:_stream
                                        serviceID:_serviceID
                               incomingConnection:incoming
                                    callbackQueue:_testCallbackQueue];
  // Wait for the selfQueue to be created
  dispatch_sync(connection.selfQueue, ^{
                });
  return connection;
}

- (NSData *)createDataWithLength:(NSUInteger)length {
  NSMutableData *data = [NSMutableData data];
  for (NSUInteger i = 0; i < length; ++i) {
    uint8_t byte = i % 256;
    [data appendBytes:&byte length:1];
  }
  return data;
}

- (NSData *)prefixLengthData:(NSData *)data {
  uint32_t dataLength = (uint32_t)data.length;
  uint32_t lengthBigEndian = CFSwapInt32HostToBig(dataLength);

  NSMutableData *packet = [NSMutableData dataWithCapacity:sizeof(uint32_t)];
  [packet appendBytes:&lengthBigEndian length:sizeof(uint32_t)];
  [packet appendData:data];

  return packet;
}

- (NSData *)prefixDataWithServiceIDHash:(NSData *)serviceIDHash data:(NSData *)data {
  NSMutableData *combinedData = [NSMutableData dataWithCapacity:serviceIDHash.length + data.length];
  [combinedData appendData:serviceIDHash];
  [combinedData appendData:data];
  return combinedData;
}

- (void)waitForCallbackQueue {
  XCTestExpectation *expectation = [self expectationWithDescription:@"Wait for callback queue"];
  dispatch_async(_testCallbackQueue, ^{
    [expectation fulfill];
  });
  [self waitForExpectations:@[ expectation ] timeout:kTestTimeout];
}

- (void)waitForSelfQueueWithConnection:(GNCBLEL2CAPConnection *)connection {
  XCTestExpectation *expectation = [self expectationWithDescription:@"Wait for self queue"];
  dispatch_async(connection.selfQueue, ^{
    [expectation fulfill];
  });
  [self waitForExpectations:@[ expectation ] timeout:kTestTimeout];
}

#pragma mark - Tests

- (void)testExtractRealDataFromData_validData {
  _connection = [self createConnectionWithIncoming:YES];
  NSData *testData = [self createDataWithLength:10];
  NSData *prefixData = [self prefixLengthData:testData];
  NSMutableData *streamData = [NSMutableData dataWithData:prefixData];
  [streamData appendData:[self createDataWithLength:5]];
  _connection.expectedDataLength = 0;
  NSData *realData = [_connection extractRealDataFromData:streamData];
  XCTAssertEqualObjects(realData, testData);
  XCTAssertEqual(_connection.expectedDataLength, testData.length);
}

- (void)testExtractRealDataFromData_notEnoughData {
  _connection = [self createConnectionWithIncoming:YES];
  NSData *prefixData = [self createDataWithLength:2];
  NSData *realData = [_connection extractRealDataFromData:prefixData];
  XCTAssertNil(realData);
}

- (void)testExtractRealDataFromData_moreThanExpectedData {
  _connection = [self createConnectionWithIncoming:YES];
  NSData *testData = [self createDataWithLength:10];
  NSData *moreData = [self createDataWithLength:5];
  NSMutableData *combinedData = [NSMutableData dataWithData:testData];
  [combinedData appendData:moreData];
  NSData *prefixData = [self prefixLengthData:combinedData];
  _connection.expectedDataLength = testData.length;
  NSData *receivedData = [NSMutableData dataWithData:prefixData];
  NSData *realData = [_connection extractRealDataFromData:receivedData];
  XCTAssertEqualObjects(realData, testData);
}

- (void)testSendData {
  _connection = [self createConnectionWithIncoming:YES];
  NSData *payload = [@"test data" dataUsingEncoding:NSUTF8StringEncoding];
  XCTestExpectation *expectation = [self expectationWithDescription:@"send data completion"];

  [_connection sendData:payload
             completion:^(BOOL result) {
               XCTAssertTrue(result);
               [expectation fulfill];
             }];

  [self waitForExpectationsWithTimeout:kTestTimeout handler:nil];

  NSData *expectedPacket = [self prefixLengthData:[self prefixDataWithServiceIDHash:_serviceIDHash
                                                                               data:payload]];
  NSData *writtenData = [_fakeInputOutputStream dataSentToWatchWithMaxBytes:expectedPacket.length];
  XCTAssertEqualObjects(writtenData, expectedPacket);
}

- (void)testRequestDataConnectionSuccess {
  _connection = [self createConnectionWithIncoming:NO];
  XCTestExpectation *expectation =
      [self expectationWithDescription:@"request data connection completion"];
  __block BOOL success = NO;
  [_connection requestDataConnectionWithCompletion:^(BOOL result) {
    success = result;
    [expectation fulfill];
  }];

  // Verify RequestDataConnection packet is sent.
  NSData *requestPacket = [self
      prefixLengthData:GNCMGenerateBLEL2CAPPacket(GNCMBLEL2CAPCommandRequestDataConnection, nil)];
  NSData *writtenReqData =
      [_fakeInputOutputStream dataSentToWatchWithMaxBytes:requestPacket.length];
  XCTAssertEqualObjects(writtenReqData, requestPacket);

  // Simulate receiving ResponseDataConnectionReady packet.
  NSData *responsePacket =
      [self prefixLengthData:GNCMGenerateBLEL2CAPPacket(
                                 GNCMBLEL2CAPCommandResponseDataConnectionReady, nil)];
  [_fakeInputOutputStream writeFromDevice:responsePacket];

  [self waitForExpectationsWithTimeout:kTestTimeout handler:nil];
  XCTAssertTrue(success);

  // Verify Introduction packet is sent.
  NSData *introPacket =
      [self prefixLengthData:GNCMGenerateBLEFramesIntroductionPacket(_serviceIDHash)];
  NSData *writtenIntroData =
      [_fakeInputOutputStream dataSentToWatchWithMaxBytes:introPacket.length];
  XCTAssertEqualObjects(writtenIntroData, introPacket);
}

- (void)testRequestDataConnectionTimeout {
  _connection = [self createConnectionWithIncoming:NO];
  XCTestExpectation *expectation =
      [self expectationWithDescription:@"request data connection completion"];
  __block BOOL success = YES;
  [_connection requestDataConnectionWithCompletion:^(BOOL result) {
    success = result;
    [expectation fulfill];
  }];

  // Verify RequestDataConnection packet is sent.
  NSData *requestPacket = [self
      prefixLengthData:GNCMGenerateBLEL2CAPPacket(GNCMBLEL2CAPCommandRequestDataConnection, nil)];
  NSData *writtenReqData =
      [_fakeInputOutputStream dataSentToWatchWithMaxBytes:requestPacket.length];
  XCTAssertEqualObjects(writtenReqData, requestPacket);

  // Don't send ResponseDataConnectionReady packet to simulate timeout.

  [self waitForExpectationsWithTimeout:kTestTimeout * 30 handler:nil];
  XCTAssertFalse(success);
}

- (void)testIncomingConnectionReceiveRequestDataConnection {
  _connection = [self createConnectionWithIncoming:YES];

  // Simulate receiving RequestDataConnection packet.
  NSData *requestPacket = [self
      prefixLengthData:GNCMGenerateBLEL2CAPPacket(GNCMBLEL2CAPCommandRequestDataConnection, nil)];
  [_fakeInputOutputStream writeFromDevice:requestPacket];

  // Verify ResponseDataConnectionReady packet is sent.
  NSData *responsePacket =
      [self prefixLengthData:GNCMGenerateBLEL2CAPPacket(
                                 GNCMBLEL2CAPCommandResponseDataConnectionReady, nil)];
  NSData *writtenData = [_fakeInputOutputStream dataSentToWatchWithMaxBytes:responsePacket.length];
  XCTAssertEqualObjects(writtenData, responsePacket);
}

- (void)testReceivePayload {
  _connection = [self createConnectionWithIncoming:YES];
  NSData *payload = [@"test data" dataUsingEncoding:NSUTF8StringEncoding];
  XCTestExpectation *payloadExpectation = [self expectationWithDescription:@"payload handler"];
  _connection.connectionHandlers = [GNCMConnectionHandlers
           payloadHandler:^(NSData *data) {
             XCTAssertEqualObjects(data, payload);
             [payloadExpectation fulfill];
           }
      disconnectedHandler:^{
      }];

  // For incoming connection, need to receive RequestDataConnection and Introduction packet first.
  NSData *requestConnectionPacket = [self
      prefixLengthData:GNCMGenerateBLEL2CAPPacket(GNCMBLEL2CAPCommandRequestDataConnection, nil)];
  [_fakeInputOutputStream writeFromDevice:requestConnectionPacket];
  [self waitForSelfQueueWithConnection:_connection];

  NSData *introPacket =
      [self prefixLengthData:GNCMGenerateBLEFramesIntroductionPacket(_serviceIDHash)];
  [_fakeInputOutputStream writeFromDevice:introPacket];
  [self waitForSelfQueueWithConnection:_connection];

  // Send payload packet.
  NSData *payloadPacket = [self prefixLengthData:[self prefixDataWithServiceIDHash:_serviceIDHash
                                                                              data:payload]];
  [_fakeInputOutputStream writeFromDevice:payloadPacket];

  [self waitForExpectationsWithTimeout:kTestTimeout handler:nil];
}

- (void)testDidDisconnectWithError {
  _connection = [self createConnectionWithIncoming:YES];
  XCTestExpectation *disconnectionExpectation =
      [self expectationWithDescription:@"disconnection handler"];
  _connection.connectionHandlers = [GNCMConnectionHandlers
      payloadHandler:^(NSData *data) {
      }
      disconnectedHandler:^{
        [disconnectionExpectation fulfill];
      }];

  [_connection stream:_stream didDisconnectWithError:nil];

  [self waitForExpectationsWithTimeout:kTestTimeout handler:nil];
}

@end
