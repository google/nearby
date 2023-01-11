// Copyright 2020 Google LLC
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

#include <utility>

#include "absl/time/time.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/implementation/executor.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/scheduled_executor.h"
#include "internal/platform/runnable.h"

using ::nearby::Runnable;
using ::nearby::api::ImplementationPlatform;
using ::nearby::api::ScheduledExecutor;

@interface GNCScheduledExecutorTest : XCTestCase
@property(atomic) int counter;
@end

@implementation GNCScheduledExecutorTest

// Creates a ScheduledExecutor.
- (std::unique_ptr<ScheduledExecutor>)executor {
  std::unique_ptr<ScheduledExecutor> executor = ImplementationPlatform::CreateScheduledExecutor();
  XCTAssert(executor != nullptr);
  return executor;
}

// Tests that the executor schedules operation at the specified time.
- (void)testScheduling {
  std::unique_ptr<ScheduledExecutor> executor([self executor]);

  XCTestExpectation *expectation = [self expectationWithDescription:@"finished"];

  void (^checkCounter)(int, NSTimeInterval, dispatch_block_t) =
      ^(int expectedCount, NSTimeInterval delay, dispatch_block_t finalBlock) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
                         XCTAssertEqual(self.counter, expectedCount);
                         finalBlock();
                       });
      };

  // Schedule two runnables that increment the counter, at 0.4 and 0.8 seconds.
  executor->Schedule([self]() { self.counter++; }, absl::Seconds(0.4));
  executor->Schedule([self]() { self.counter++; }, absl::Seconds(0.8));

  // Check that the counter contains the expected values at 0.2, 0.6, and 1.0 seconds.
  checkCounter(0, 0.2,
               ^{
               });
  checkCounter(1, 0.6,
               ^{
               });
  checkCounter(2, 1.0, ^{
    [expectation fulfill];
  });

  [self waitForExpectationsWithTimeout:1.2 handler:nil];
}

// Tests that fails to schedule when the executor is shut down.
- (void)testFailtoScheduleAfterShutdown {
  std::unique_ptr<ScheduledExecutor> executor([self executor]);

  executor->Shutdown();

  dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_TARGET_QUEUE_DEFAULT, 0);
  XCTestExpectation *expectation = [self expectationWithDescription:@"finished"];

  const NSTimeInterval delay = 0.1;
  executor->Schedule([self]() { self.counter++; }, absl::Milliseconds(delay));

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * 2 * NSEC_PER_SEC)), queue, ^{
    XCTAssertEqual(self.counter, 0);
    [expectation fulfill];
  });

  [self waitForExpectationsWithTimeout:delay * 5 handler:nil];
}

// Tests that fails to cancel when the executor is shut down.
- (void)testFailToCancelAfterShutdown {
  std::unique_ptr<ScheduledExecutor> executor([self executor]);

  executor->Shutdown();

  auto cancelable = executor->Schedule([self]() { self.counter++; }, absl::Seconds(0.1));

  XCTAssert(cancelable.get() == nullptr);
}

// Tests that shutting down an existing task fails to complete.
- (void)testShutdownToFailExistingTask {
  std::unique_ptr<ScheduledExecutor> executor([self executor]);

  dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_TARGET_QUEUE_DEFAULT, 0);
  XCTestExpectation *expectation = [self expectationWithDescription:@"finished"];

  const NSTimeInterval delay = 0.1;
  executor->Schedule([self]() { self.counter++; }, absl::Milliseconds(delay));

  executor->Shutdown();

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * 2 * NSEC_PER_SEC)), queue, ^{
    XCTAssertEqual(self.counter, 0);
    [expectation fulfill];
  });

  [self waitForExpectationsWithTimeout:delay * 5 handler:nil];
}

// Tests that a canceled runnable doesn't run.
- (void)testCancelable {
  std::unique_ptr<ScheduledExecutor> executor([self executor]);

  // This runnable should run but not increment the counter because it sleeps for longer than the
  // cancel below.
  const NSTimeInterval delay = 0.1;
  auto cancelable = executor->Schedule([self]() { self.counter++; }, absl::Seconds(delay));
  XCTAssert(cancelable.get() != nullptr);

  // Cancel the runnable.
  cancelable->Cancel();

  // Wait for the time interval to pass and verify that it didn't run.
  [NSThread sleepForTimeInterval:delay * 1.1];
  XCTAssertEqual(self.counter, 0);
}

@end
