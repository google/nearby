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

#if defined(__APPLE__) && !defined(__clang_analyzer__) && __has_include(<XCTest/XCTest.h>)
#import <XCTest/XCTest.h>

#include <memory>
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

// Verifies that the executor runs scheduled tasks in order after their specified delays.
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

// Verifies that scheduling or executing a task after Shutdown synchronously returns a null
// cancelable and does not run the task.
- (void)testFailtoScheduleAfterShutdown {
  std::unique_ptr<ScheduledExecutor> executor([self executor]);

  executor->Shutdown();

  auto cancelable = executor->Schedule([self]() { self.counter++; }, absl::Milliseconds(100));
  XCTAssertTrue(cancelable == nullptr);
  executor->Execute([self]() { self.counter++; });
  XCTAssertEqual(self.counter, 0);
}

// Verifies that Cancel returns false after a scheduled task has already executed.
- (void)testCancelFailsIfTaskAlreadyExecuted {
  std::unique_ptr<ScheduledExecutor> executor([self executor]);
  XCTestExpectation *executed = [self expectationWithDescription:@"task executed"];

  auto cancelable = executor->Schedule(
      [self, executed]() {
        self.counter++;
        [executed fulfill];
      },
      absl::ZeroDuration());
  XCTAssertTrue(cancelable != nullptr);

  [self waitForExpectationsWithTimeout:2.0 handler:nil];

  XCTAssertFalse(cancelable->Cancel());
  XCTAssertEqual(self.counter, 1);
}

// Verifies that calling Shutdown before a scheduled task's delay expires prevents the task from
// executing.
- (void)testShutdownToFailExistingTask {
  std::unique_ptr<ScheduledExecutor> executor([self executor]);
  XCTestExpectation *notExecuted = [self expectationWithDescription:@"task should not run"];
  notExecuted.inverted = YES;

  const NSTimeInterval delay = 0.1;
  executor->Schedule(
      [self, notExecuted]() {
        self.counter++;
        [notExecuted fulfill];
      },
      absl::Seconds(delay));

  executor->Shutdown();

  [self waitForExpectationsWithTimeout:delay * 1.5 handler:nil];
  XCTAssertEqual(self.counter, 0);
}

// Verifies that canceling a scheduled runnable before its delay expires prevents it from
// executing.
- (void)testCancelable {
  std::unique_ptr<ScheduledExecutor> executor([self executor]);
  XCTestExpectation *notExecuted =
      [self expectationWithDescription:@"canceled task should not run"];
  notExecuted.inverted = YES;

  const NSTimeInterval delay = 0.1;
  auto cancelable = executor->Schedule(
      [self, notExecuted]() {
        self.counter++;
        [notExecuted fulfill];
      },
      absl::Seconds(delay));
  XCTAssert(cancelable.get() != nullptr);

  XCTAssertTrue(cancelable->Cancel());

  [self waitForExpectationsWithTimeout:delay * 1.5 handler:nil];
  XCTAssertEqual(self.counter, 0);
}

// Verifies that an Objective-C NSException thrown inside a runnable is caught by
// ScheduledExecutor when compiled with -fno-exceptions and subsequent tasks still execute.
- (void)testScheduleRecoversFromObjCException {
  std::unique_ptr<ScheduledExecutor> executor([self executor]);
  XCTestExpectation *expectation = [self expectationWithDescription:@"recovered after exception"];

  executor->Execute([]() {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:@"Simulated ObjC exception in scheduled task"
                                 userInfo:nil];
  });

  executor->Execute([self, expectation]() {
    self.counter++;
    [expectation fulfill];
  });

  [self waitForExpectationsWithTimeout:2.0 handler:nil];
  XCTAssertEqual(self.counter, 1);
}

@end
#endif  // defined(__APPLE__) && !defined(__clang_analyzer__) &&
        // __has_include(<XCTest/XCTest.h>)
