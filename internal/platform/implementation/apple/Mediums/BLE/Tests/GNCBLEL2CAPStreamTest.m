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

/// Tests that initializing a second stream with already-configured @c NSStream instances rejects
/// them without stealing or closing the active stream's delegates.
- (void)testInitWithAlreadyConfiguredStreamsDoesNotDisruptExistingStream {
  // GIVEN
  NSData *dummyData = [@"dummyData" dataUsingEncoding:NSASCIIStringEncoding];
  _fakeInputOutputStream =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:dummyData.length * 2];

  _stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];

  // WHEN
  __weak GNCBLEL2CAPStream *weakDuplicateStream = nil;
  @autoreleasepool {
    GNCBLEL2CAPStream *duplicateStream = [[GNCBLEL2CAPStream alloc]
        initWithClosedBlock:^{
        }
                inputStream:_fakeInputOutputStream.inputStream
               outputStream:_fakeInputOutputStream.outputStream];
    weakDuplicateStream = duplicateStream;
    [duplicateStream tearDown];
    duplicateStream = nil;
  }

  XCTestExpectation *completionExpectation =
      [self expectationWithDescription:@"Primary stream still sends data"];
  [_stream sendData:dummyData
      completionBlock:^(BOOL result) {
        XCTAssertTrue(result);
        [completionExpectation fulfill];
      }];

  // THEN
  [self waitForExpectations:@[ completionExpectation ] timeout:1.0];
  NSData *sentData = [_fakeInputOutputStream dataSentToWatchWithMaxBytes:dummyData.length];
  XCTAssertEqualObjects(sentData, dummyData);
  XCTAssertNil(weakDuplicateStream);
}

/// Tests concurrent teardown and deallocation when a duplicate stream is rejected.
- (void)testConcurrentTearDownAndDeallocWithDuplicateStreamsDoesNotCrash {
  for (int i = 0; i < 50; ++i) {
    @autoreleasepool {
      GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
          [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:64];
      GNCBLEL2CAPStream *firstStream = [[GNCBLEL2CAPStream alloc]
          initWithClosedBlock:^{
          }
                  inputStream:fakeStreams.inputStream
                 outputStream:fakeStreams.outputStream];
      GNCBLEL2CAPStream *secondStream = [[GNCBLEL2CAPStream alloc]
          initWithClosedBlock:^{
          }
                  inputStream:fakeStreams.inputStream
                 outputStream:fakeStreams.outputStream];
      [firstStream tearDown];
      [secondStream tearDown];
      firstStream = nil;
      secondStream = nil;
      [fakeStreams tearDown];
    }
  }

  XCTestExpectation *drainExpectation =
      [self expectationWithDescription:@"Async teardown blocks drained"];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [drainExpectation fulfill];
                 });
  [self waitForExpectations:@[ drainExpectation ] timeout:1.0];
}

/// Tests deallocating an active stream without calling @c tearDown while stream events are firing.
- (void)testDeallocWithoutTearDownWhileReceivingDataDoesNotCrash {
  NSData *dummyData = [@"dummyData" dataUsingEncoding:NSASCIIStringEncoding];
  for (int i = 0; i < 50; ++i) {
    @autoreleasepool {
      GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
          [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:dummyData.length * 2];
      GNCBLEL2CAPStream *stream = [[GNCBLEL2CAPStream alloc]
          initWithClosedBlock:^{
          }
                  inputStream:fakeStreams.inputStream
                 outputStream:fakeStreams.outputStream];
      [fakeStreams writeFromDevice:dummyData];
      stream = nil;
      [fakeStreams tearDown];
    }
  }

  XCTestExpectation *drainExpectation =
      [self expectationWithDescription:@"Async deallocation blocks drained"];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [drainExpectation fulfill];
                 });
  [self waitForExpectations:@[ drainExpectation ] timeout:1.0];
}

/// Tests passing @c nil input or output streams does not crash and fails subsequent writes.
- (void)testInitWithNilStreamsDoesNotCrash {
  NSData *dummyData = [@"dummyData" dataUsingEncoding:NSASCIIStringEncoding];
  _fakeInputOutputStream = [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:64];
  NSInputStream *nilInputStream = nil;
  NSOutputStream *nilOutputStream = nil;

  GNCBLEL2CAPStream *nilInput = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:nilInputStream
             outputStream:_fakeInputOutputStream.outputStream];
  GNCBLEL2CAPStream *nilOutput = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:nilOutputStream];
  GNCBLEL2CAPStream *bothNil = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:nilInputStream
             outputStream:nilOutputStream];

  XCTestExpectation *completionExpectation =
      [self expectationWithDescription:@"Writes on nil-configured streams fail immediately"];
  completionExpectation.expectedFulfillmentCount = 3;

  [nilInput sendData:dummyData
      completionBlock:^(BOOL result) {
        XCTAssertFalse(result);
        [completionExpectation fulfill];
      }];
  [nilOutput sendData:dummyData
      completionBlock:^(BOOL result) {
        XCTAssertFalse(result);
        [completionExpectation fulfill];
      }];
  [bothNil sendData:dummyData
      completionBlock:^(BOOL result) {
        XCTAssertFalse(result);
        [completionExpectation fulfill];
      }];

  [self waitForExpectations:@[ completionExpectation ] timeout:1.0];
  XCTAssertNil(_fakeInputOutputStream.inputStream.delegate);
  XCTAssertNil(_fakeInputOutputStream.outputStream.delegate);
  [nilInput tearDown];
  [nilOutput tearDown];
  [bothNil tearDown];
}

/// Tests that re-using an already-opened or already-closed @c NSStream pair is rejected.
- (void)testInitWithAlreadyOpenedOrClosedStreamsIsRejected {
  NSData *dummyData = [@"dummyData" dataUsingEncoding:NSASCIIStringEncoding];

  // 1. Already-opened streams with nil delegates should be rejected.
  NSInputStream *openedInput = nil;
  NSOutputStream *openedPeerOutput = nil;
  [NSStream getBoundStreamsWithBufferSize:64
                              inputStream:&openedInput
                             outputStream:&openedPeerOutput];
  NSInputStream *openedPeerInput = nil;
  NSOutputStream *openedOutput = nil;
  [NSStream getBoundStreamsWithBufferSize:64
                              inputStream:&openedPeerInput
                             outputStream:&openedOutput];
  [openedInput open];
  [openedOutput open];

  GNCBLEL2CAPStream *openedRejectedStream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:openedInput
             outputStream:openedOutput];
  XCTAssertNil(openedInput.delegate);
  XCTAssertNil(openedOutput.delegate);

  XCTestExpectation *openedFailExpectation =
      [self expectationWithDescription:@"Write on opened-rejected stream fails"];
  [openedRejectedStream sendData:dummyData
                 completionBlock:^(BOOL result) {
                   XCTAssertFalse(result);
                   [openedFailExpectation fulfill];
                 }];

  [openedInput close];
  [openedPeerOutput close];
  [openedPeerInput close];
  [openedOutput close];

  // 2. Closed streams from a torn-down GNCBLEL2CAPStream (with delegates cleared) are rejected.
  _fakeInputOutputStream =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:dummyData.length - 1];
  _stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];

  XCTestExpectation *firstTearDownExpectation =
      [self expectationWithDescription:@"First stream tearDown finished closing streams"];
  [_stream sendData:dummyData
      completionBlock:^(BOOL result) {
        XCTAssertFalse(result);
        [firstTearDownExpectation fulfill];
      }];
  [_stream tearDown];
  [self waitForExpectations:@[ openedFailExpectation, firstTearDownExpectation ] timeout:1.0];

  XCTAssertNil(_fakeInputOutputStream.inputStream.delegate);
  XCTAssertNil(_fakeInputOutputStream.outputStream.delegate);
  XCTAssertEqual(_fakeInputOutputStream.inputStream.streamStatus, NSStreamStatusClosed);
  XCTAssertEqual(_fakeInputOutputStream.outputStream.streamStatus, NSStreamStatusClosed);

  GNCBLEL2CAPStream *reusedClosedStream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];
  XCTAssertNil(_fakeInputOutputStream.inputStream.delegate);
  XCTAssertNil(_fakeInputOutputStream.outputStream.delegate);

  XCTestExpectation *closedFailExpectation =
      [self expectationWithDescription:@"Write on closed-rejected stream fails"];
  [reusedClosedStream sendData:dummyData
               completionBlock:^(BOOL result) {
                 XCTAssertFalse(result);
                 [closedFailExpectation fulfill];
               }];
  [self waitForExpectations:@[ closedFailExpectation ] timeout:1.0];
}

/// Tests calling @c sendData:completionBlock: after @c tearDown or stream rejection immediately
/// invokes @c completionBlock(NO).
- (void)testSendDataAfterTearDownOrStreamRejectionFailsImmediately {
  NSData *dummyData = [@"dummyData" dataUsingEncoding:NSASCIIStringEncoding];
  _fakeInputOutputStream =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:dummyData.length * 2];

  _stream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];

  GNCBLEL2CAPStream *rejectedStream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
      }
              inputStream:_fakeInputOutputStream.inputStream
             outputStream:_fakeInputOutputStream.outputStream];

  __block BOOL rejectedCallbackInvoked = NO;
  [rejectedStream sendData:dummyData
           completionBlock:^(BOOL result) {
             XCTAssertFalse(result);
             rejectedCallbackInvoked = YES;
           }];
  XCTAssertTrue(rejectedCallbackInvoked);

  [_stream tearDown];

  __block BOOL tornDownCallbackInvoked = NO;
  [_stream sendData:dummyData
      completionBlock:^(BOOL result) {
        XCTAssertFalse(result);
        tornDownCallbackInvoked = YES;
      }];
  XCTAssertTrue(tornDownCallbackInvoked);
}

/// Tests deallocating a stream with pending queued writes without calling @c tearDown invokes
/// @c completionBlock(NO) on all pending writes.
- (void)testDeallocWithoutTearDownFailsPendingWrites {
  NSData *dummyData = [@"dummyData" dataUsingEncoding:NSASCIIStringEncoding];
  _fakeInputOutputStream =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:dummyData.length - 1];

  XCTestExpectation *pendingWritesFailedExpectation =
      [self expectationWithDescription:@"All pending writes fail on dealloc"];
  pendingWritesFailedExpectation.expectedFulfillmentCount = 2;

  @autoreleasepool {
    GNCBLEL2CAPStream *stream = [[GNCBLEL2CAPStream alloc]
        initWithClosedBlock:^{
        }
                inputStream:_fakeInputOutputStream.inputStream
               outputStream:_fakeInputOutputStream.outputStream];

    [stream sendData:dummyData
        completionBlock:^(BOOL result) {
          XCTAssertFalse(result);
          [pendingWritesFailedExpectation fulfill];
        }];
    [stream sendData:dummyData
        completionBlock:^(BOOL result) {
          XCTAssertFalse(result);
          [pendingWritesFailedExpectation fulfill];
        }];
    stream = nil;
  }

  [self waitForExpectations:@[ pendingWritesFailedExpectation ] timeout:1.0];

  XCTestExpectation *drainExpectation =
      [self expectationWithDescription:@"Async deallocation block drained"];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [drainExpectation fulfill];
                 });
  [self waitForExpectations:@[ drainExpectation ] timeout:1.0];
}

@end
