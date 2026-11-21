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

@interface GNCNWFrameworkSocket (Testing)
- (NSCondition *)createReceiveCondition;
@end

@interface GNCTestNWFrameworkSocket : GNCNWFrameworkSocket
@property(nonatomic, nullable) NSCondition *testCondition;
@end

@implementation GNCTestNWFrameworkSocket

- (NSCondition *)createReceiveCondition {
  return self.testCondition ?: [super createReceiveCondition];
}

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

typedef void (^GNCReceiveCompletionHandler)(dispatch_data_t _Nullable content,
                                            nw_content_context_t _Nullable context,
                                            bool isComplete,
                                            nw_error_t _Nullable error);
typedef void (^GNCSendCompletionHandler)(nw_error_t _Nullable error);

@interface GNCBlockingFakeNWConnection : GNCFakeNWConnection
@property(nonatomic, nullable) XCTestExpectation *receiveStartedExpectation;
@property(nonatomic, nullable) XCTestExpectation *sendStartedExpectation;
- (void)completePendingHandlers;
@end

@implementation GNCBlockingFakeNWConnection {
  NSMutableArray<GNCReceiveCompletionHandler> *_pendingReceiveHandlers;
  NSMutableArray<GNCSendCompletionHandler> *_pendingSendHandlers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _pendingReceiveHandlers = [[NSMutableArray alloc] init];
    _pendingSendHandlers = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)receiveMessageWithMinLength:(uint32_t)minIncompleteLength
                          maxLength:(uint32_t)maxLength
                  completionHandler:(GNCReceiveCompletionHandler)handler {
  @synchronized(self) {
    [_pendingReceiveHandlers addObject:[handler copy]];
  }
  [self.receiveStartedExpectation fulfill];
}

- (void)sendData:(dispatch_data_t)content
              context:(nw_content_context_t)context
           isComplete:(BOOL)isComplete
    completionHandler:(GNCSendCompletionHandler)handler {
  @synchronized(self) {
    [_pendingSendHandlers addObject:[handler copy]];
  }
  [self.sendStartedExpectation fulfill];
}

- (void)completePendingHandlers {
  NSArray<GNCReceiveCompletionHandler> *receiveHandlers = nil;
  NSArray<GNCSendCompletionHandler> *sendHandlers = nil;
  @synchronized(self) {
    receiveHandlers = [_pendingReceiveHandlers copy];
    [_pendingReceiveHandlers removeAllObjects];
    sendHandlers = [_pendingSendHandlers copy];
    [_pendingSendHandlers removeAllObjects];
  }
  for (GNCReceiveCompletionHandler handler in receiveHandlers) {
    handler(nil, nil, YES, nil);
  }
  for (GNCSendCompletionHandler handler in sendHandlers) {
    handler(nil);
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

  GNCDelayedSendFakeNWConnection *delayedConnection = [[GNCDelayedSendFakeNWConnection alloc] init];
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

  GNCDelayedSendFakeNWConnection *delayedConnection = [[GNCDelayedSendFakeNWConnection alloc] init];
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

- (void)testReadWaitsAcrossSpuriousConditionWakeupAndHandlesCleanEOF_WhenFlagEnabled {
  id featureFlagsMock = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([featureFlagsMock guardNWFrameworkSocketReadConditionEnabled]).andReturn(YES);

  GNCSpuriousWakeupFakeNWConnection *spuriousConnection =
      [[GNCSpuriousWakeupFakeNWConnection alloc] init];
  NSCondition *condition = [[NSCondition alloc] init];
  spuriousConnection.targetCondition = condition;
  GNCTestNWFrameworkSocket *socket =
      [[GNCTestNWFrameworkSocket alloc] initWithConnection:spuriousConnection];
  socket.testCondition = condition;

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

- (void)testReadWaitsViaBaselineSemaphore_WhenFlagDisabled {
  id featureFlagsMock = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([featureFlagsMock guardNWFrameworkSocketReadConditionEnabled]).andReturn(NO);

  GNCSpuriousWakeupFakeNWConnection *spuriousConnection =
      [[GNCSpuriousWakeupFakeNWConnection alloc] init];
  NSCondition *condition = [[NSCondition alloc] init];
  spuriousConnection.targetCondition = condition;
  GNCTestNWFrameworkSocket *socket =
      [[GNCTestNWFrameworkSocket alloc] initWithConnection:spuriousConnection];
  socket.testCondition = condition;

  NSString *testString = @"testData";
  NSData *testData = [testString dataUsingEncoding:NSUTF8StringEncoding];
  dispatch_data_t dispatchData =
      dispatch_data_create(testData.bytes, testData.length, dispatch_get_main_queue(), ^{});
  spuriousConnection.dataToReceive = dispatchData;

  NSError *error = nil;
  NSData *receivedData = [socket readMaxLength:testData.length error:&error];
  XCTAssertEqualObjects(receivedData, testData);
  XCTAssertNil(error);

  [featureFlagsMock stopMocking];
}

- (void)testCloseUnblocksConcurrentReadAndWrite_WhenFlagEnabled {
  id featureFlagsMock = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([featureFlagsMock closeNWFrameworkSocketSafelyEnabled]).andReturn(YES);

  GNCBlockingFakeNWConnection *blockingConnection = [[GNCBlockingFakeNWConnection alloc] init];
  GNCNWFrameworkSocket *socket =
      [[GNCNWFrameworkSocket alloc] initWithConnection:blockingConnection];

  XCTestExpectation *receiveStarted = [self expectationWithDescription:@"receive started"];
  XCTestExpectation *sendStarted = [self expectationWithDescription:@"send started"];
  XCTestExpectation *readFinished = [self expectationWithDescription:@"read finished"];
  XCTestExpectation *writeFinished = [self expectationWithDescription:@"write finished"];
  blockingConnection.receiveStartedExpectation = receiveStarted;
  blockingConnection.sendStartedExpectation = sendStarted;

  __block NSData *readData = nil;
  __block NSError *readError = nil;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    NSError *error = nil;
    readData = [socket readMaxLength:10 error:&error];
    readError = error;
    [readFinished fulfill];
  });

  __block BOOL writeResult = YES;
  __block NSError *writeError = nil;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    NSError *error = nil;
    writeResult = [socket write:[@"payload" dataUsingEncoding:NSUTF8StringEncoding] error:&error];
    writeError = error;
    [writeFinished fulfill];
  });

  [self waitForExpectations:@[ receiveStarted, sendStarted ] timeout:1.0];
  [socket close];

  XCTWaiterResult waitResult =
      [XCTWaiter waitForExpectations:@[ readFinished, writeFinished ] timeout:1.0];
  [blockingConnection completePendingHandlers];
  [featureFlagsMock stopMocking];

  XCTAssertEqual(waitResult, XCTWaiterResultCompleted);
  XCTAssertTrue(blockingConnection.cancelCalled);
  XCTAssertNil(readData);
  XCTAssertNotNil(readError);
  XCTAssertEqualObjects(readError.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(readError.code, GNCNWFrameworkErrorNotConnected);
  XCTAssertFalse(writeResult);
  XCTAssertNotNil(writeError);
  XCTAssertEqualObjects(writeError.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(writeError.code, GNCNWFrameworkErrorNotConnected);
}

- (void)testCloseDoesNotUnblockInFlightRead_WhenFlagDisabled {
  id featureFlagsMock = OCMClassMock([GNCFeatureFlags class]);
  OCMStub([featureFlagsMock closeNWFrameworkSocketSafelyEnabled]).andReturn(NO);

  GNCBlockingFakeNWConnection *blockingConnection = [[GNCBlockingFakeNWConnection alloc] init];
  GNCNWFrameworkSocket *socket =
      [[GNCNWFrameworkSocket alloc] initWithConnection:blockingConnection];

  XCTestExpectation *receiveStarted = [self expectationWithDescription:@"receive started"];
  XCTestExpectation *readFinished = [self expectationWithDescription:@"read finished"];
  dispatch_semaphore_t readDrained = dispatch_semaphore_create(0);
  blockingConnection.receiveStartedExpectation = receiveStarted;

  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    NSError *error = nil;
    (void)[socket readMaxLength:10 error:&error];
    [readFinished fulfill];
    dispatch_semaphore_signal(readDrained);
  });

  [self waitForExpectations:@[ receiveStarted ] timeout:1.0];
  [socket close];

  XCTWaiterResult waitResult = [XCTWaiter waitForExpectations:@[ readFinished ] timeout:0.3];
  [blockingConnection completePendingHandlers];
  intptr_t drainResult =
      dispatch_semaphore_wait(readDrained, dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC));
  [featureFlagsMock stopMocking];

  XCTAssertEqual(waitResult, XCTWaiterResultTimedOut);
  XCTAssertEqual(drainResult, 0);
  XCTAssertTrue(blockingConnection.cancelCalled);
}

@end

NS_ASSUME_NONNULL_END
