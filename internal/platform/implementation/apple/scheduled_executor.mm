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

#import "internal/platform/implementation/apple/scheduled_executor.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <utility>

#include "absl/time/time.h"
#import "internal/platform/implementation/apple/atomic_boolean.h"
#include "internal/platform/runnable.h"

// This wraps the C++ Runnable in an Obj-C object for memory management. It is retained by the
// dispatch block below, and deleted when the block is released.
@interface GNCRunnableWrapper : NSObject {
 @public
  nearby::Runnable _runnable;
  std::unique_ptr<nearby::apple::AtomicBoolean> _canceled;
}
@end

@implementation GNCRunnableWrapper

+ (instancetype)wrapperWithRunnable:(nearby::Runnable)runnable {
  GNCRunnableWrapper *wrapper = [[GNCRunnableWrapper alloc] init];
  wrapper->_runnable = std::move(runnable);
  wrapper->_canceled = std::make_unique<nearby::apple::AtomicBoolean>(false);
  return wrapper;
}

@end

@implementation GNCOperationQueueImpl

+ (instancetype)implWithMaxConcurrency:(int)maxConcurrency {
  GNCOperationQueueImpl *impl = [[GNCOperationQueueImpl alloc] init];
  impl.queue = [[NSOperationQueue alloc] init];
  impl.queue.maxConcurrentOperationCount = maxConcurrency;
  return impl;
}

@end

namespace nearby {
namespace apple {

static const std::int64_t kExecutorShutdownDefaultTimeout = 500;  // 0.5 seconds

// This Cancelable references a Runnable and a cancel method that sets its canceled boolean to true.
class CancelableForRunnable : public api::Cancelable {
 public:
  explicit CancelableForRunnable(GNCRunnableWrapper *runnable) : runnable_(runnable) {}
  CancelableForRunnable() = default;
  ~CancelableForRunnable() override = default;
  CancelableForRunnable(const CancelableForRunnable &) = delete;
  CancelableForRunnable &operator=(const CancelableForRunnable &) = delete;

  // api::Cancelable:
  bool Cancel() override {
    runnable_->_canceled->Set(true);
    return true;
  }

 private:
  GNCRunnableWrapper *runnable_;
};

ScheduledExecutor::ScheduledExecutor() { impl_ = [GNCOperationQueueImpl implWithMaxConcurrency:1]; }

ScheduledExecutor::ScheduledExecutor(int max_concurrency) {
  impl_ = [GNCOperationQueueImpl implWithMaxConcurrency:max_concurrency];
}

ScheduledExecutor::~ScheduledExecutor() { impl_ = nil; }

void ScheduledExecutor::Shutdown() { Shutdown(kExecutorShutdownDefaultTimeout); }

std::shared_ptr<api::Cancelable> ScheduledExecutor::Schedule(Runnable &&runnable,
                                                             absl::Duration duration) {
  if (impl_.shuttingDown) return std::shared_ptr<api::Cancelable>(nullptr);

  // Wrap the runnable in an Obj-C object so it can be referenced by the delayed block.
  GNCRunnableWrapper *wrapper = [GNCRunnableWrapper wrapperWithRunnable:std::move(runnable)];
  CancelableForRunnable *cancelable = new CancelableForRunnable(wrapper);
  GNCOperationQueueImpl *impl = impl_;  // don't capture |this|
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, absl::ToInt64Milliseconds(duration) * NSEC_PER_MSEC),
      dispatch_get_global_queue(DISPATCH_TARGET_QUEUE_DEFAULT, 0), ^{
        [impl.queue addOperationWithBlock:^{
          // Execute the runnable only if the executor is not shutting down, and the runnable isn't
          // canceled.
          // Warning: This block should reference only Obj-C objects, and never C++ objects.
          if (!impl.shuttingDown && !wrapper->_canceled->Get()) {
            wrapper->_runnable();
          }
        }];
      });

  return std::shared_ptr<api::Cancelable>(cancelable);
}

void ScheduledExecutor::Execute(Runnable &&runnable) { DoSubmit(std::move(runnable)); }

bool ScheduledExecutor::DoSubmit(Runnable &&runnable) {
  if (impl_.shuttingDown) {
    return false;
  }

  // Submit the runnable to the queue.
  __block Runnable local_runnable = std::move(runnable);
  [impl_.queue addOperationWithBlock:^{
    local_runnable();
  }];
  return true;
}

void ScheduledExecutor::Shutdown(std::int64_t timeout_millis) {
  // Prevent new/delayed operations from being queued/executed.
  impl_.shuttingDown = YES;

  // Block until either (a) all currently executing operations finish, or (b) the timeout expires.
  dispatch_group_t group = dispatch_group_create();
  dispatch_group_async(group, dispatch_get_global_queue(DISPATCH_TARGET_QUEUE_DEFAULT, 0), ^{
    [impl_.queue waitUntilAllOperationsAreFinished];
  });
  dispatch_group_wait(group, dispatch_time(DISPATCH_TIME_NOW, timeout_millis * NSEC_PER_MSEC));
}

}  // namespace apple
}  // namespace nearby
