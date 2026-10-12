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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkError.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWConnection.h"

NS_ASSUME_NONNULL_BEGIN

// Declare the private getter only for deterministic close-between-validation
// and-submission injection. This does not create a Network.framework connection.
@interface GNCNWFrameworkSocket (CloseSubmissionTest)
- (id<GNCNWConnection>)connection;
@end

@interface GNCCountingNWConnection : GNCFakeNWConnection
@property(nonatomic) NSUInteger receiveCalls;
@property(nonatomic) NSUInteger sendCalls;
@property(nonatomic) NSUInteger cancelCalls;
@end

@implementation GNCCountingNWConnection
- (void)cancel {
  self.cancelCalls += 1;
  [super cancel];
}
- (void)receiveMessageWithMinLength:(uint32_t)minimum
                          maxLength:(uint32_t)maximum
                    completionHandler:(void (^)(dispatch_data_t _Nullable,
                                              nw_content_context_t _Nullable, bool,
                                              nw_error_t _Nullable))handler {
  self.receiveCalls += 1;
  [super receiveMessageWithMinLength:minimum maxLength:maximum completionHandler:handler];
}
- (void)sendData:(dispatch_data_t)content
              context:(nw_content_context_t)context
           isComplete:(BOOL)complete
    completionHandler:(void (^)(nw_error_t _Nullable))handler {
  self.sendCalls += 1;
  [super sendData:content context:context isComplete:complete completionHandler:handler];
}
@end

@interface GNCCloseBeforeSubmissionSocket : GNCNWFrameworkSocket
@property(nonatomic) BOOL closedDuringValidation;
@end

@implementation GNCCloseBeforeSubmissionSocket
- (id<GNCNWConnection>)connection {
  id<GNCNWConnection> captured = [super connection];
  if (!self.closedDuringValidation) {
    self.closedDuringValidation = YES;
    [super close];
  }
  return captured;
}
@end

// Inline completion is an adapter seam: it deterministically completes before
// the waiting code runs. Actual Network.framework callbacks are asynchronous.
@interface GNCInlineNWConnection : GNCCountingNWConnection
@end
@implementation GNCInlineNWConnection
- (void)receiveMessageWithMinLength:(uint32_t)minimum
                          maxLength:(uint32_t)maximum
                    completionHandler:(void (^)(dispatch_data_t _Nullable,
                                              nw_content_context_t _Nullable, bool,
                                              nw_error_t _Nullable))handler {
  self.receiveCalls += 1;
  const char value = 'x';
  handler(dispatch_data_create(&value, 1, nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT), nil, YES, nil);
}
- (void)sendData:(dispatch_data_t)content
              context:(nw_content_context_t)context
           isComplete:(BOOL)complete
    completionHandler:(void (^)(nw_error_t _Nullable))handler {
  self.sendCalls += 1;
  handler(nil);
}
@end

@interface GNCHeldWriteNWConnection : GNCCountingNWConnection
@property(nonatomic, copy, nullable) void (^pendingWrite)(nw_error_t _Nullable);
@end
@implementation GNCHeldWriteNWConnection
- (void)sendData:(dispatch_data_t)content
              context:(nw_content_context_t)context
           isComplete:(BOOL)complete
    completionHandler:(void (^)(nw_error_t _Nullable))handler {
  self.sendCalls += 1;
  self.pendingWrite = handler;
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

- (void)testWriteBytes_Success {
  NSError *error = nil;
  NSString *testString = @"testData";
  NSData *testData = [testString dataUsingEncoding:NSUTF8StringEncoding];

  BOOL result = [_socket writeBytes:testData.bytes length:testData.length error:&error];

  XCTAssertTrue(result);
  XCTAssertNil(error);
}

- (void)testWriteBytes_Error {
  NSError *error = nil;
  NSString *testString = @"testData";
  NSData *testData = [testString dataUsingEncoding:NSUTF8StringEncoding];
  _fakeConnection.simulateSendFailure = YES;

  BOOL result = [_socket writeBytes:testData.bytes length:testData.length error:&error];

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
  XCTAssertFalse([_socket writeBytes:"test" length:4 error:&error]);
}

- (void)testReadMaxLength_ClosedBetweenValidationAndSubmission {
  GNCCountingNWConnection *peer = [[GNCCountingNWConnection alloc] init];
  GNCNWFrameworkSocket *socket = [[GNCCloseBeforeSubmissionSocket alloc] initWithConnection:peer];
  NSError *error = nil;
  XCTAssertNil([socket readMaxLength:1 error:&error]);
  XCTAssertNotNil(error);
  XCTAssertEqualObjects(error.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(error.code, GNCNWFrameworkErrorNotConnected);
  XCTAssertEqual(peer.receiveCalls, 0u);
  XCTAssertEqual(peer.cancelCalls, 1u);
}

- (void)testReadString_ClosedBetweenValidationAndSubmission {
  GNCCountingNWConnection *peer = [[GNCCountingNWConnection alloc] init];
  GNCNWFrameworkSocket *socket = [[GNCCloseBeforeSubmissionSocket alloc] initWithConnection:peer];
  NSError *error = nil;
  XCTAssertFalse([socket readStringWithMaxLength:1 error:&error].has_value());
  XCTAssertNotNil(error);
  XCTAssertEqualObjects(error.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(error.code, GNCNWFrameworkErrorNotConnected);
  XCTAssertEqual(peer.receiveCalls, 0u);
  XCTAssertEqual(peer.cancelCalls, 1u);
}

- (void)testWrite_ClosedBetweenValidationAndSubmission {
  GNCCountingNWConnection *peer = [[GNCCountingNWConnection alloc] init];
  GNCNWFrameworkSocket *socket = [[GNCCloseBeforeSubmissionSocket alloc] initWithConnection:peer];
  NSError *error = nil;
  XCTAssertFalse([socket write:[NSData dataWithBytes:"x" length:1] error:&error]);
  XCTAssertNotNil(error);
  XCTAssertEqualObjects(error.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(error.code, GNCNWFrameworkErrorNotConnected);
  XCTAssertEqual(peer.sendCalls, 0u);
  XCTAssertEqual(peer.cancelCalls, 1u);
}

- (void)testWriteBytes_ClosedBetweenValidationAndSubmission {
  GNCCountingNWConnection *peer = [[GNCCountingNWConnection alloc] init];
  GNCNWFrameworkSocket *socket = [[GNCCloseBeforeSubmissionSocket alloc] initWithConnection:peer];
  NSError *error = nil;
  XCTAssertFalse([socket writeBytes:"x" length:1 error:&error]);
  XCTAssertNotNil(error);
  XCTAssertEqualObjects(error.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(error.code, GNCNWFrameworkErrorNotConnected);
  XCTAssertEqual(peer.sendCalls, 0u);
  XCTAssertEqual(peer.cancelCalls, 1u);
}

- (void)testRepeatedCloseCancelsDetachedConnectionOnce {
  for (NSUInteger round = 0; round < 1000; ++round) {
    @autoreleasepool {
      GNCCountingNWConnection *peer = [[GNCCountingNWConnection alloc] init];
      GNCNWFrameworkSocket *socket = [[GNCNWFrameworkSocket alloc] initWithConnection:peer];
      [socket close];
      [socket close];
      XCTAssertEqual(peer.cancelCalls, 1u);
      NSError *error = nil;
      XCTAssertFalse([socket writeBytes:"x" length:1 error:&error]);
      XCTAssertNotNil(error);
      XCTAssertEqual(peer.sendCalls, 0u);
    }
  }
}

- (void)testReadMaxLength_CompletesBeforeWaiting {
  GNCInlineNWConnection *peer = [[GNCInlineNWConnection alloc] init];
  GNCNWFrameworkSocket *socket = [[GNCNWFrameworkSocket alloc] initWithConnection:peer];
  NSError *error = nil;
  XCTAssertEqualObjects([socket readMaxLength:1 error:&error], [@"x" dataUsingEncoding:NSUTF8StringEncoding]);
  XCTAssertNil(error);
  XCTAssertEqual(peer.receiveCalls, 1u);
}

- (void)testReadString_CompletesBeforeWaiting {
  GNCInlineNWConnection *peer = [[GNCInlineNWConnection alloc] init];
  GNCNWFrameworkSocket *socket = [[GNCNWFrameworkSocket alloc] initWithConnection:peer];
  NSError *error = nil;
  auto result = [socket readStringWithMaxLength:1 error:&error];
  XCTAssertTrue(result.has_value());
  if (result) XCTAssertTrue(result.value() == "x");
  XCTAssertNil(error);
}

- (void)testWrite_CompletesBeforeWaiting {
  GNCInlineNWConnection *peer = [[GNCInlineNWConnection alloc] init];
  GNCNWFrameworkSocket *socket = [[GNCNWFrameworkSocket alloc] initWithConnection:peer];
  NSError *error = nil;
  XCTAssertTrue([socket write:[@"x" dataUsingEncoding:NSUTF8StringEncoding] error:&error]);
  XCTAssertNil(error);
  XCTAssertEqual(peer.sendCalls, 1u);
}

- (void)testWriteBytes_CompletesBeforeWaiting {
  GNCInlineNWConnection *peer = [[GNCInlineNWConnection alloc] init];
  GNCNWFrameworkSocket *socket = [[GNCNWFrameworkSocket alloc] initWithConnection:peer];
  NSError *error = nil;
  XCTAssertTrue([socket writeBytes:"x" length:1 error:&error]);
  XCTAssertNil(error);
  XCTAssertEqual(peer.sendCalls, 1u);
}

- (void)assertWriteTimeoutWithRawBytes:(BOOL)raw {
  GNCHeldWriteNWConnection *peer = [[GNCHeldWriteNWConnection alloc] init];
  GNCNWFrameworkSocket *socket = [[GNCNWFrameworkSocket alloc] initWithConnection:peer];
  NSError *error = nil;
  NSTimeInterval start = NSProcessInfo.processInfo.systemUptime;
  BOOL result = raw ? [socket writeBytes:"x" length:1 error:&error]
                    : [socket write:[@"x" dataUsingEncoding:NSUTF8StringEncoding] error:&error];
  NSTimeInterval elapsed = NSProcessInfo.processInfo.systemUptime - start;
  XCTAssertFalse(result);
  XCTAssertEqualObjects(error.domain, GNCNWFrameworkErrorDomain);
  XCTAssertEqual(error.code, GNCNWFrameworkErrorTimedOut);
  XCTAssertGreaterThanOrEqual(elapsed, 4.9);
  XCTAssertLessThan(elapsed, 8.0);
  void (^late)(nw_error_t _Nullable) = peer.pendingWrite;
  peer.pendingWrite = nil;
  XCTAssertNotNil(late);
  if (late) late(nil);
  XCTAssertEqual(error.code, GNCNWFrameworkErrorTimedOut);
  XCTAssertEqual(peer.sendCalls, 1u);  // No replay after an uncertain timeout.
  [socket close];
}

- (void)testWrite_TimeoutAndLateCompletion { [self assertWriteTimeoutWithRawBytes:NO]; }
- (void)testWriteBytes_TimeoutAndLateCompletion { [self assertWriteTimeoutWithRawBytes:YES]; }

@end

NS_ASSUME_NONNULL_END
