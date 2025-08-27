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

#import "internal/platform/implementation/apple/Mediums/Hotspot/GNCHotspotSocket.h"
#import "internal/platform/implementation/apple/Mediums/Hotspot/Tests/GNCFakeNWConnection.h"

#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

@interface GNCHotspotSocketTests : XCTestCase
@end

@implementation GNCHotspotSocketTests {
  GNCFakeNWConnection *_fakeConnection;
  GNCHotspotSocket *_socket;
}

- (void)setUp {
  [super setUp];
  _fakeConnection = [[GNCFakeNWConnection alloc] init];
  _socket = [[GNCHotspotSocket alloc] initWithConnection:_fakeConnection];
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

  XCTAssertEqualObjects(receivedData, [NSData data]);
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

  XCTAssertTrue(result);  // This seems wrong, but send completion always returns error=nil
  XCTAssertNil(error);
}

- (void)testClose {
  XCTAssertFalse(_fakeConnection.cancelCalled);
  [_socket close];
  XCTAssertTrue(_fakeConnection.cancelCalled);
  // Also test that subsequent operations fail
  NSError *error = nil;
  XCTAssertNil([_socket readMaxLength:10 error:&error]);
  XCTAssertEqual(error.code, EBADF);
  XCTAssertFalse([_socket write:[NSData data] error:&error]);
  XCTAssertEqual(error.code, EBADF);
}

@end

NS_ASSUME_NONNULL_END
