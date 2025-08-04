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
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPFakeInputOutputStream.h"

// TODO: b/399815436 - More tests for GNCBLEL2CAPConnection.
@interface GNCBLEL2CAPConnection (Testing)
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
@end

@implementation GNCBLEL2CAPConnectionTest

- (void)setUp {
  [super setUp];
  _fakeInputOutputStream = [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:100];
  _stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];
  _testCallbackQueue = dispatch_queue_create("com.google.test.callback", DISPATCH_QUEUE_SERIAL);
  _serviceID = @"testServiceID";
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

- (NSData *)createDataWithLength:(NSUInteger)length prefix:(NSData *)prefix {
  NSMutableData *data = [NSMutableData dataWithLength:length];
  if (prefix) {
    [data appendData:prefix];
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

- (void)waitForCallbackQueue {
  XCTestExpectation *expectation = [self expectationWithDescription:@"Wait for callback queue"];
  dispatch_async(_testCallbackQueue, ^{
    [expectation fulfill];
  });
  [self waitForExpectations:@[ expectation ] timeout:1];
}

#pragma mark - Tests

- (void)testExtractRealDataFromData_validData {
  _connection = [self createConnectionWithIncoming:YES];
  NSData *testData = [self createDataWithLength:10 prefix:nil];
  NSData *prefixData = [self prefixLengthData:[self createDataWithLength:testData.length
                                                                  prefix:nil]];
  _connection.expectedDataLength = testData.length;
  NSData *realData = [_connection extractRealDataFromData:prefixData];
  XCTAssertEqualObjects(realData, testData);
}

- (void)testExtractRealDataFromData_notEnoughData {
  _connection = [self createConnectionWithIncoming:YES];
  NSData *prefixData = [self createDataWithLength:2 prefix:nil];
  NSData *realData = [_connection extractRealDataFromData:prefixData];
  XCTAssertNil(realData);
}

- (void)testExtractRealDataFromData_moreThanExpectedData {
  _connection = [self createConnectionWithIncoming:YES];
  NSData *testData = [self createDataWithLength:10 prefix:nil];
  NSData *moreData = [self createDataWithLength:5 prefix:nil];
  NSMutableData *combinedData = [NSMutableData dataWithData:testData];
  [combinedData appendData:moreData];
  NSData *prefixData = [self prefixLengthData:[self createDataWithLength:combinedData.length
                                                                  prefix:nil]];
  _connection.expectedDataLength = testData.length;
  NSData *receivedData = [NSMutableData dataWithData:prefixData];
  NSData *realData = [_connection extractRealDataFromData:receivedData];
  XCTAssertEqualObjects(realData, testData);
}

@end
