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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"

#import <Network/Network.h>
#import <XCTest/XCTest.h>

#include <optional>
#include <string>

#import "internal/platform/implementation/apple/Flags/GNCFeatureFlags.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWConnection.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

NS_ASSUME_NONNULL_BEGIN

@interface GNCNWFrameworkSocket (Testing)
@property(nonatomic, readonly) NSCondition *condition;
@end

@interface GNCSpuriousWakeupFakeNWConnection : GNCFakeNWConnection
@property(nonatomic, nullable) NSCondition *targetCondition;
@end

@implementation GNCSpuriousWakeupFakeNWConnection

- (void)receiveMessageWithMinLength:(uint32_t)minIncompleteLength
                          maxLength:(uint32_t)maxLength
                  completionHandler:(void (^)(dispatch_data_t _Nullable content,
                                              nw_content_context_t _Nullable context,
                                              bool isComplete, nw_error_t _Nullable error))handler {
  NSCondition *condition = self.targetCondition;
  dispatch_data_t data = self.dataToReceive;
  BOOL failure = self.simulateReceiveFailure;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    if (condition != nil) {
      [condition lock];
      [condition broadcast];
      [condition unlock];
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(50 * NSEC_PER_MSEC)),
                   dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
                     if (failure) {
                       handler(nil, nil, YES, nil);
                     } else {
                       handler(data, nil, YES, nil);
                     }
                   });
  });
}

@end

@interface GNCNWFrameworkSocketTests : XCTestCase
@end

@implementation GNCNWFrameworkSocketTests {
  GNCFakeNWConnection *_fakeConnection;
  GNCNWFrameworkSocket *_socket;
}

- (void)setUp {
  [super setUp];
  _fakeConnection = [[GNCFakeNWConnection alloc] init];
  _socket = [[GNCNWFrameworkSocket alloc] initWithConnection:_fakeConnection];
}

- (void)tearDown {
  _socket = nil;
  _fakeConnection = nil;
  [super tearDown];
}

- (void)testInit {
  XCTAssertNotNil(_socket);
}

- (void)testReadMaxLength_Success {
  NSError *error = nil;
  NSString *testString = @"testData";
  NSData *testData = [testString dataUsingEncoding:NSUTF8StringEncoding];
  _fakeConnection.dataToReceive = (dispatch_data_t)testData;

  NSData *receivedData = [_socket readMaxLength:testData.length error:&error];

  XCTAssertEqualObjects(receivedData, testData);
  XCTAssertNil(error);
}

- (void)testReadMaxLength_Error {
  NSError *error = nil;
  _fakeConnection.simulateReceiveFailure = YES;

  NSData *receivedData = [_socket readMaxLength:10 error:&error];

  XCTAssertNil(receivedData);
  XCTAssertNil(error);  // Fake doesn't produce an NSError
}

- (void)testReadMaxLength_Zero {
  NSError *error = nil;
  NSData *receivedData = [_socket readMaxLength:0 error:&error];

  XCTAssertNil(receivedData);
  XCTAssertNil(error);
}

- (void)testReadStringWithMaxLength_Success {
  NSError *error = nil;
  NSString *testString = @"testData";
  NSData *testData = [testString dataUsingEncoding:NSUTF8StringEncoding];
  dispatch_data_t dispatchData = dispatch_data_create(testData.bytes, testData.length, dispatch_get_main_queue(), ^{});
  _fakeConnection.dataToReceive = dispatchData;

  std::optional<std::string> receivedString = [_socket readStringWithMaxLength:testData.length error:&error];

  XCTAssertTrue(receivedString.has_value());
  XCTAssertEqualObjects(@(receivedString.value().c_str()), testString);
  XCTAssertNil(error);
}

- (void)testReadStringWithMaxLength_Error {
  NSError *error = nil;
  _fakeConnection.simulateReceiveFailure = YES;

  std::optional<std::string> receivedString = [_socket readStringWithMaxLength:10 error:&error];

  XCTAssertFalse(receivedString.has_value());
  XCTAssertNil(error);  // Fake doesn't produce an NSError
}

- (void)testReadStringWithMaxLength_Zero {
  NSError *error = nil;
  std::optional<std::string> receivedString = [_socket readStringWithMaxLength:0 error:&error];

  XCTAssertFalse(receivedString.has_value());
  XCTAssertNil(error);
}

- (void)testWrite_Success {
  NSError *error = nil;
  NSString *testString = @"testData";
  NSData *testData = [testString dataUsingEncoding:NSUTF8StringEncoding];

  BOOL result = [_socket write:testData error:&error];

  XCTAssertTrue(result);
  XCTAssertNil(error);
}

- (void)testWrite_Error {
  NSError *error = nil;
  NSString *testString = @"testData";
  NSData *testData = [testString dataUsingEncoding:NSUTF8StringEncoding];
  _fakeConnection.simulateSendFailure = YES;

  BOOL result = [_socket write:testData error:&error];

  XCTAssertFalse(result);
}

- (void)testClose {
  XCTAssertFalse(_fakeConnection.cancelCalled);
  [_socket close];
  XCTAssertTrue(_fakeConnection.cancelCalled);
  // Also test that subsequent operations fail
  NSError *error = nil;
  XCTAssertNil([_socket readMaxLength:10 error:&error]);
  XCTAssertFalse([_socket write:[NSData data] error:&error]);
}

- (void)testReadWaitsAcrossSpuriousConditionWakeupAndHandlesCleanEOF_WhenFlagEnabled {
  id featureFlagsMock = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([featureFlagsMock guardNWFrameworkSocketReadConditionEnabled]).andReturn(YES);

  GNCSpuriousWakeupFakeNWConnection *spuriousConnection =
      [[GNCSpuriousWakeupFakeNWConnection alloc] init];
  GNCNWFrameworkSocket *socket =
      [[GNCNWFrameworkSocket alloc] initWithConnection:spuriousConnection];
  spuriousConnection.targetCondition = socket.condition;

  NSString *testString = @"testData";
  NSData *testData = [testString dataUsingEncoding:NSUTF8StringEncoding];
  dispatch_data_t dispatchData =
      dispatch_data_create(testData.bytes, testData.length, dispatch_get_main_queue(), ^{});
  spuriousConnection.dataToReceive = dispatchData;

  NSError *error = nil;
  NSData *receivedData = [socket readMaxLength:testData.length error:&error];
  XCTAssertEqualObjects(receivedData, testData);
  XCTAssertNil(error);

  std::optional<std::string> receivedString =
      [socket readStringWithMaxLength:testData.length error:&error];
  XCTAssertTrue(receivedString.has_value());
  XCTAssertEqualObjects(@(receivedString.value().c_str()), testString);
  XCTAssertNil(error);

  spuriousConnection.dataToReceive = nil;
  NSData *eofData = [socket readMaxLength:testData.length error:&error];
  XCTAssertNil(eofData);
  XCTAssertNil(error);

  std::optional<std::string> eofString =
      [socket readStringWithMaxLength:testData.length error:&error];
  XCTAssertFalse(eofString.has_value());
  XCTAssertNil(error);

  [featureFlagsMock stopMocking];
}

- (void)testReadReturnsNilOnSpuriousConditionWakeup_WhenFlagDisabled {
  id featureFlagsMock = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([featureFlagsMock guardNWFrameworkSocketReadConditionEnabled]).andReturn(NO);

  GNCSpuriousWakeupFakeNWConnection *spuriousConnection =
      [[GNCSpuriousWakeupFakeNWConnection alloc] init];
  GNCNWFrameworkSocket *socket =
      [[GNCNWFrameworkSocket alloc] initWithConnection:spuriousConnection];
  spuriousConnection.targetCondition = socket.condition;

  NSString *testString = @"testData";
  NSData *testData = [testString dataUsingEncoding:NSUTF8StringEncoding];
  dispatch_data_t dispatchData =
      dispatch_data_create(testData.bytes, testData.length, dispatch_get_main_queue(), ^{});
  spuriousConnection.dataToReceive = dispatchData;

  NSError *error = nil;
  NSData *receivedData = [socket readMaxLength:testData.length error:&error];
  XCTAssertNil(receivedData);

  // Wait briefly for the delayed callback in the fake connection to finish before tearing down.
  [NSThread sleepForTimeInterval:0.1];
  [featureFlagsMock stopMocking];
}

@end

NS_ASSUME_NONNULL_END
