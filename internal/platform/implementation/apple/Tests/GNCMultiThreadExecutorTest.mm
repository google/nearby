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
using MultiThreadExecutor = ::nearby::api::SubmittableExecutor;

@interface GNCMultiThreadExecutorTest : XCTestCase
@property(atomic) int counter;
@property(atomic) int otherCounter;
@end

@implementation GNCMultiThreadExecutorTest

// Creates a MultiThreadExecutor.
- (std::unique_ptr<MultiThreadExecutor>)executor {
  std::unique_ptr<MultiThreadExecutor> executor =
      ImplementationPlatform::CreateMultiThreadExecutor(4);
  XCTAssert(executor != nullptr);
  return executor;
}

// Tests that the executor executes runnables as expected.
- (void)testExecute {
  std::unique_ptr<MultiThreadExecutor> executor([self executor]);

  // Execute two runnables that increment the counter.
  const int kIncrements = 13;
  for (int i = 0; i < kIncrements; i++) {
    executor->Execute([self]() { self.counter++; });
    [NSThread sleepForTimeInterval:0.01];
  }

  // Check that the counter has the expected value after giving the runnables time to run.
  XCTAssertLessThanOrEqual(abs(self.counter - kIncrements), 0);
}

// Tests that the executor submits runnables as expected.
- (void)testSubmit {
  std::unique_ptr<MultiThreadExecutor> executor([self executor]);

  // Submit two runnables that increment the counter.
  const int kIncrements = 13;
  for (int i = 0; i < kIncrements; i++) {
    executor->DoSubmit([self]() { self.counter++; });
    [NSThread sleepForTimeInterval:0.01];
  }

  // Check that the counter has the expected value after giving the runnables time to run.
  XCTAssertLessThanOrEqual(abs(self.counter - kIncrements), 0);
}

// Tests that fails to submit when the executor is shut down.
- (void)testFailtoSubmitAfterShutdown {
  std::unique_ptr<MultiThreadExecutor> executor([self executor]);

  executor->Shutdown();

  XCTAssertFalse(executor->DoSubmit([self]() { self.counter++; }));

  // Check that the counter has the expected value after giving the runnables time to run.
  [NSThread sleepForTimeInterval:0.01];
  XCTAssertEqual(self.counter, 0);
}

// Tests that when the executor is shut down, it waits for pending operations to finish, and no new
// operations will be executed.
- (void)testShutdownToAllowExistingTaskComplete {
  std::unique_ptr<MultiThreadExecutor> executor([self executor]);

  dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_TARGET_QUEUE_DEFAULT, 0);
  XCTestExpectation *expectation = [self expectationWithDescription:@"finished"];

  const int kRunnableCount = 1000;
  for (int i = 0; i < kRunnableCount; i++) {
    executor->Execute([self]() { self.counter++; });
  }

  executor->Shutdown();

  executor->Execute([self]() { self.otherCounter++; });

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)), queue, ^{
    XCTAssertLessThanOrEqual(self.counter, kRunnableCount);
    XCTAssertEqual(self.otherCounter, 0);
    [expectation fulfill];
  });

  [self waitForExpectationsWithTimeout:0.5 handler:nil];
}

@end
