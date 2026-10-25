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
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkError.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWConnection.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

NS_ASSUME_NONNULL_BEGIN

typedef void (^GNCLateSendCompletionHandler)(nw_error_t _Nullable error);

@interface GNCDelayedSendFakeNWConnection : GNCFakeNWConnection
- (void)invokePendingSendHandlerWithError:(nullable nw_error_t)error;
@end

@implementation GNCDelayedSendFakeNWConnection {
  GNCLateSendCompletionHandler _pendingSendHandler;
}

- (void)sendData:(dispatch_data_t)content
              context:(nw_content_context_t)context
           isComplete:(BOOL)isComplete
    completionHandler:(GNCLateSendCompletionHandler)handler {
  if (self.simulateSendFailure) {
    @synchronized(self) {
      _pendingSendHandler = [handler copy];
    }
  } else {
    [super sendData:content context:context isComplete:isComplete completionHandler:handler];
  }
}

- (void)invokePendingSendHandlerWithError:(nullable nw_error_t)error {
  GNCLateSendCompletionHandler handler = nil;
  @synchronized(self) {
    handler = _pendingSendHandler;
    _pendingSendHandler = nil;
  }
  if (handler != nil) {
    handler(error);
  }
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

- (void)testWrite_TimeoutPopulatesPosixTimedOutAndHandlesLateCompletionSafely_WhenFlagEnabled {
  id featureFlagsMock = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([featureFlagsMock synchronizeNWFrameworkSocketWriteTimeoutEnabled]).andReturn(YES);

  GNCDelayedSendFakeNWConnection *delayedConnection =
      [[GNCDelayedSendFakeNWConnection alloc] init];
  delayedConnection.simulateSendFailure = YES;
  GNCNWFrameworkSocket *socket =
      [[GNCNWFrameworkSocket alloc] initWithConnection:delayedConnection];

  NSError *error = nil;
  NSData *testData = [@"testData" dataUsingEncoding:NSUTF8StringEncoding];
  BOOL result = [socket write:testData error:&error];
  [delayedConnection invokePendingSendHandlerWithError:nil];
  [featureFlagsMock stopMocking];

  XCTAssertFalse(result);
  XCTAssertNotNil(error);
  XCTAssertEqualObjects(error.domain, NSPOSIXErrorDomain);
  XCTAssertEqual(error.code, ETIMEDOUT);
}

- (void)testWrite_TimeoutPopulatesFrameworkTimedOut_WhenFlagDisabled {
  id featureFlagsMock = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([featureFlagsMock synchronizeNWFrameworkSocketWriteTimeoutEnabled]).andReturn(NO);

  GNCDelayedSendFakeNWConnection *delayedConnection =
      [[GNCDelayedSendFakeNWConnection alloc] init];
  delayedConnection.simulateSendFailure = YES;
  GNCNWFrameworkSocket *socket =
      [[GNCNWFrameworkSocket alloc] initWithConnection:delayedConnection];

  NSError *error = nil;
  NSData *testData = [@"testData" dataUsingEncoding:NSUTF8StringEncoding];
  BOOL result = [socket write:testData error:&error];
  [featureFlagsMock stopMocking];

  XCTAssertFalse(result);
  XCTAssertNotNil(error);
  XCTAssertEqualObjects(error.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(error.code, GNCNWFrameworkErrorTimedOut);
}

@end

NS_ASSUME_NONNULL_END
