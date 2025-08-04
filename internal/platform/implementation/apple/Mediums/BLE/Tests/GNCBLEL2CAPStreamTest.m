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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPStream.h"

#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPFakeInputOutputStream.h"

@interface GNCBLEL2CAPStreamTest : XCTestCase
@end

// TODO: edwinwu - Add tests for the stream received delegate.
@implementation GNCBLEL2CAPStreamTest {
  GNCBLEL2CAPFakeInputOutputStream* _fakeInputOutputStream;
  GNCBLEL2CAPStream* _stream;
}

- (void)tearDown {
  [_stream tearDown];
  [_fakeInputOutputStream tearDown];

  _stream = nil;
  [super tearDown];
}

#pragma mark Tests

/// Tests data larger that buffer size is sent to watch despite chunking.
- (void)testSendDataWithSmallerStreamBuffer {
  // GIVEN
  NSData* dummyData = [@"dummyData" dataUsingEncoding:NSASCIIStringEncoding];
  _fakeInputOutputStream =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:dummyData.length - 1];

  _stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];

  // WHEN
  [_stream sendData:dummyData
      completionBlock:^(BOOL result){
      }];

  // THEN
  NSData* sentData = [_fakeInputOutputStream dataSentToWatchWithMaxBytes:dummyData.length];
  XCTAssertEqualObjects(sentData, dummyData);
}

- (void)testSendDataCompletionIsCalled {
  // GIVEN
  NSData* dummyData = [@"dummyData" dataUsingEncoding:NSASCIIStringEncoding];
  _fakeInputOutputStream =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:dummyData.length * 2];

  _stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];

  // WHEN
  XCTestExpectation* completionExpectation =
      [self expectationWithDescription:@"Completion is called"];
  [_stream sendData:dummyData
      completionBlock:^(BOOL result) {
        XCTAssert(result);
        [completionExpectation fulfill];
      }];

  // THEN
  [self waitForExpectations:@[ completionExpectation ] timeout:1.0];
}

- (void)testSendDataCompletionIsNotCalledIfDataIsNotCompletelyWritten {
  // GIVEN
  NSData* dummyData = [@"dummyData" dataUsingEncoding:NSASCIIStringEncoding];
  // Small buffer size so that the data cannot be completely written.
  _fakeInputOutputStream =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:dummyData.length - 1];

  _stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];

  // WHEN
  XCTestExpectation* noCompletionExpectation =
      [self expectationWithDescription:@"Completion is not called"];
  noCompletionExpectation.inverted = YES;
  [_stream sendData:dummyData
      completionBlock:^(BOOL result) {
        [noCompletionExpectation fulfill];
      }];

  // THEN
  [self waitForExpectations:@[ noCompletionExpectation ] timeout:1.0];
}

- (void)testSendDataCompletionIsCalledAfterDataIsCompletelyWritten {
  // GIVEN
  NSData* dummyData = [@"dummyData" dataUsingEncoding:NSASCIIStringEncoding];
  // Small buffer size so that the data is not completely written until the watch starts reading.
  _fakeInputOutputStream =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:dummyData.length - 1];

  _stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];

  // WHEN
  XCTestExpectation* completionExpectation =
      [self expectationWithDescription:@"Completion is called"];
  [_stream sendData:dummyData
      completionBlock:^(BOOL result) {
        XCTAssert(result);
        [completionExpectation fulfill];
      }];
  // Reads the written data on the buffer so that the stream send finish writing the full packet.
  [_fakeInputOutputStream dataSentToWatchWithMaxBytes:dummyData.length];

  // THEN
  [self waitForExpectations:@[ completionExpectation ] timeout:1.0];
}

- (void)testSendDataCompletionIsCalledWhenStreamIsTornDown {
  // GIVEN
  NSData* dummyData = [@"dummyData" dataUsingEncoding:NSASCIIStringEncoding];
  // Small buffer size so that the data is not completely written until the watch starts reading.
  _fakeInputOutputStream =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:dummyData.length - 1];

  _stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];

  XCTestExpectation* completionExpectation =
      [self expectationWithDescription:@"Completion is called"];

  // WHEN
  [_stream sendData:dummyData
      completionBlock:^(BOOL result) {
        XCTAssertFalse(result);
        [completionExpectation fulfill];
      }];
  [_stream tearDown];

  // THEN
  [self waitForExpectations:@[ completionExpectation ] timeout:1.0];
}

/// Tests receiving data is not possible after invoking the tear down method.
- (void)testDataCannotBeReceivedAfterInvokingTearDown {
  // GIVEN
  NSData* dummyData1 = [@"dummyData1" dataUsingEncoding:NSASCIIStringEncoding];

  _fakeInputOutputStream =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:dummyData1.length];

  XCTestExpectation* expectation = [self expectationWithDescription:@"Received data from watch."];
  expectation.inverted = YES;

  _stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];

  // WHEN
  [_stream tearDown];

  [_fakeInputOutputStream writeFromDevice:dummyData1];

  // THEN
  [self waitForExpectations:@[ expectation ] timeout:1.0];
}

/// Tests closing the remote end disconnects the L2CAP stream.
- (void)testClosedBlockIsCalledWhenStreamIsClosed {
  // GIVEN
  // Hard-coded from @c GNCBLEL2CAPStream.
  NSUInteger kMaxAllowedQueuedDataBytes = 20480;
  _fakeInputOutputStream =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:kMaxAllowedQueuedDataBytes];

  XCTestExpectation* disconnectedExpectation =
      [self expectationWithDescription:@"State disconnected"];

  _stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
        [disconnectedExpectation fulfill];
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];

  // WHEN
  [_fakeInputOutputStream tearDown];

  // THEN
  [self waitForExpectations:@[ disconnectedExpectation ] timeout:1.0];
}

@end
