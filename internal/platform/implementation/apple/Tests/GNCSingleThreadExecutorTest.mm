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

#include <atomic>
#include <memory>
#include <utility>

#include "absl/time/time.h"
#include "internal/platform/implementation/executor.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/single_thread_executor.h"
#include "internal/platform/runnable.h"

using ::nearby::Runnable;
using ::nearby::api::ImplementationPlatform;
using ::nearby::api::SingleThreadExecutor;

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
  XCTestExpectation *expectation = [self expectationWithDescription:@"finished"];
  executor->Execute([self, expectation]() {
    self.counter++;
    [expectation fulfill];
  });
  executor->Shutdown();
  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  XCTAssertEqual(self.counter, 1);
}

// Tests that DelayedExecute executes after the specified delay and treats zero/negative delays as
// immediate Execute submissions.
- (void)testDelayedExecute {
  std::unique_ptr<SingleThreadExecutor> executor([self executor]);
  XCTestExpectation *zeroExpectation = [self expectationWithDescription:@"zero"];
  XCTestExpectation *negativeExpectation = [self expectationWithDescription:@"negative"];
  XCTestExpectation *expectation = [self expectationWithDescription:@"delayed"];

  XCTAssertTrue(executor->DelayedExecute(absl::ZeroDuration(), [self, zeroExpectation]() {
    self.counter++;
    [zeroExpectation fulfill];
  }));
  XCTAssertTrue(executor->DelayedExecute(-absl::Seconds(1), [self, negativeExpectation]() {
    self.counter++;
    [negativeExpectation fulfill];
  }));
  XCTAssertTrue(executor->DelayedExecute(absl::Milliseconds(20), [self, expectation]() {
    self.counter++;
    [expectation fulfill];
  }));

  [self waitForExpectations:@[ zeroExpectation, negativeExpectation, expectation ] timeout:1.0];
  XCTAssertEqual(self.counter, 3);
}

// Tests that DelayedExecute cancels pending delayed tasks on Shutdown and rejects post-Shutdown
// calls.
- (void)testDelayedExecuteShutdown {
  std::unique_ptr<SingleThreadExecutor> executor([self executor]);
  XCTAssertFalse(executor->DelayedExecute(absl::Milliseconds(10), nullptr));
  XCTAssertTrue(executor->DelayedExecute(absl::Seconds(10), [self]() { self.counter++; }));

  executor->Shutdown();

  XCTAssertFalse(executor->DelayedExecute(absl::Milliseconds(10), [self]() { self.counter++; }));
  XCTAssertEqual(self.counter, 0);
}

// Tests IsRunningOnExecutor returns true only on the queue currently executing a task.
- (void)testIsRunningOnExecutor {
  std::unique_ptr<SingleThreadExecutor> executor([self executor]);
  std::unique_ptr<SingleThreadExecutor> otherExecutor([self executor]);
  SingleThreadExecutor *executorPtr = executor.get();
  SingleThreadExecutor *otherExecutorPtr = otherExecutor.get();
  XCTAssertFalse(executor->IsRunningOnExecutor());

  XCTestExpectation *executeExpectation = [self expectationWithDescription:@"execute"];
  std::atomic<bool> onExecutorInExecute = false;
  std::atomic<bool> onOtherInExecute = true;
  XCTAssertTrue(executor->Execute(
      [executorPtr, otherExecutorPtr, &onExecutorInExecute, &onOtherInExecute, executeExpectation]() {
        onExecutorInExecute.store(executorPtr->IsRunningOnExecutor(), std::memory_order_relaxed);
        onOtherInExecute.store(otherExecutorPtr->IsRunningOnExecutor(), std::memory_order_relaxed);
        [executeExpectation fulfill];
      }));
  [self waitForExpectations:@[ executeExpectation ] timeout:1.0];
  XCTAssertTrue(onExecutorInExecute.load(std::memory_order_relaxed));
  XCTAssertFalse(onOtherInExecute.load(std::memory_order_relaxed));

  XCTestExpectation *delayedExpectation = [self expectationWithDescription:@"delayed"];
  std::atomic<bool> onExecutorInDelayed = false;
  std::atomic<bool> onOtherInDelayed = true;
  XCTAssertTrue(executor->DelayedExecute(
      absl::Milliseconds(10),
      [executorPtr, otherExecutorPtr, &onExecutorInDelayed, &onOtherInDelayed, delayedExpectation]() {
        onExecutorInDelayed.store(executorPtr->IsRunningOnExecutor(), std::memory_order_relaxed);
        onOtherInDelayed.store(otherExecutorPtr->IsRunningOnExecutor(), std::memory_order_relaxed);
        [delayedExpectation fulfill];
      }));
  [self waitForExpectations:@[ delayedExpectation ] timeout:1.0];
  XCTAssertTrue(onExecutorInDelayed.load(std::memory_order_relaxed));
  XCTAssertFalse(onOtherInDelayed.load(std::memory_order_relaxed));

  executor->Shutdown();
  otherExecutor->Shutdown();
  XCTAssertFalse(executor->IsRunningOnExecutor());
}

@end
