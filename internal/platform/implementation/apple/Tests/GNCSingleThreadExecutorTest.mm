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
#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/runnable.h"

using ::nearby::Runnable;
using ::nearby::api::ImplementationPlatform;
using SingleThreadExecutor = ::nearby::api::SubmittableExecutor;

@interface GNCSingleThreadExecutorTest : XCTestCase
@property(atomic) int counter;
@end

@implementation GNCSingleThreadExecutorTest

// Creates a SingleThreadExecutor.
- (std::unique_ptr<SingleThreadExecutor>)executor {
  std::unique_ptr<SingleThreadExecutor> executor =
      ImplementationPlatform::CreateSingleThreadExecutor();
  XCTAssert(executor != nullptr);
  return executor;
}

// Tests that the executor executes runnables as expected.
- (void)testRunnables {
  std::unique_ptr<SingleThreadExecutor> executor([self executor]);

  // Schedule two runnables that increment the counter.
  executor->Execute([self]() { self.counter++; });
  executor->Execute([self]() { self.counter++; });

  // Check that the counter has the expected value after a moment.
  [NSThread sleepForTimeInterval:0.01];
  XCTAssertEqual(self.counter, 2);
}

// Tests that fails to execute when the executor is shut down.
- (void)testShutdownBeforeInvokeTask {
  std::unique_ptr<SingleThreadExecutor> executor([self executor]);

  executor->Shutdown();

  dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_TARGET_QUEUE_DEFAULT, 0);
  XCTestExpectation *expectation = [self expectationWithDescription:@"finished"];

  executor->Execute([self]() { self.counter++; });

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)), queue, ^{
    XCTAssertEqual(self.counter, 0);
    [expectation fulfill];
  });

  [self waitForExpectationsWithTimeout:0.5 handler:nil];
}

// Tests that shutting down an existing task allows to complete.
- (void)testShutdownToAllowExistingTaskComplete {
  std::unique_ptr<SingleThreadExecutor> executor([self executor]);

  dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_TARGET_QUEUE_DEFAULT, 0);
  XCTestExpectation *expectation = [self expectationWithDescription:@"finished"];

  executor->Execute([self]() { self.counter++; });

  executor->Shutdown();

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)), queue, ^{
    XCTAssertEqual(self.counter, 1);
    [expectation fulfill];
  });

  [self waitForExpectationsWithTimeout:0.5 handler:nil];
}

@end
