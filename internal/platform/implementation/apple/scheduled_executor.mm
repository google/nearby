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

#include <atomic>
#include <memory>
#include <utility>

#include "absl/time/time.h"
#include "internal/platform/runnable.h"

// Defines the state of a scheduled task. This enum is at global scope
// to be accessible by the global Objective-C++ classes below.
enum class GNCScheduledTaskState {
  kScheduled,
  kRunning,
  kDone,
  kCanceled,
};

// An Objective-C object to hold the shared state between the cancelable handle
// and the scheduled block. Its lifetime is managed by ARC.
@interface GNCScheduledTask : NSObject {
 @public
  // The user-provided runnable to execute.
  nearby::Runnable _runnable;
  // The atomic state machine for the task's lifecycle.
  std::atomic<GNCScheduledTaskState> _state;
}
- (instancetype)initWithRunnable:(nearby::Runnable &&)runnable;
@end

@implementation GNCScheduledTask
- (instancetype)initWithRunnable:(nearby::Runnable &&)runnable {
  if ((self = [super init])) {
    _runnable = std::move(runnable);
    _state = GNCScheduledTaskState::kScheduled;
  }
  return self;
}
@end

// An implementation of api::Cancelable that controls a GNCScheduledTask.
// This is an Objective-C++ class to allow for a __strong ivar, which lets ARC
// manage the lifetime of the GNCScheduledTask object.
class ExecutorCancelable : public nearby::api::Cancelable {
 public:
  explicit ExecutorCancelable(GNCScheduledTask *task) : task_(task) {}
  ~ExecutorCancelable() override = default;
  ExecutorCancelable(const ExecutorCancelable &) = delete;
  ExecutorCancelable &operator=(const ExecutorCancelable &) = delete;

  bool Cancel() override {
    // If the task has already been canceled, do nothing and return true.
    if (task_->_state.load() == GNCScheduledTaskState::kCanceled) {
      return true;
    }
    GNCScheduledTaskState expected = GNCScheduledTaskState::kScheduled;
    // Atomically transition from Scheduled to Canceled. This will only
    // succeed if the task has not already started running.
    return task_->_state.compare_exchange_strong(expected, GNCScheduledTaskState::kCanceled);
  }

 private:
  // ARC-managed strong reference to the shared task state.
  __strong GNCScheduledTask *task_;
};

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

ScheduledExecutor::ScheduledExecutor() { impl_ = [GNCOperationQueueImpl implWithMaxConcurrency:1]; }

ScheduledExecutor::ScheduledExecutor(int max_concurrency) {
  impl_ = [GNCOperationQueueImpl implWithMaxConcurrency:max_concurrency];
}

ScheduledExecutor::~ScheduledExecutor() {
  Shutdown();
  impl_ = nil;
}

void ScheduledExecutor::Shutdown() { Shutdown(kExecutorShutdownDefaultTimeout); }

std::shared_ptr<api::Cancelable> ScheduledExecutor::Schedule(Runnable &&runnable,
                                                             absl::Duration duration) {
  if (impl_.shuttingDown) return std::shared_ptr<api::Cancelable>(nullptr);

  // Create the shared task object. ARC will manage its lifetime.
  GNCScheduledTask *task = [[GNCScheduledTask alloc] initWithRunnable:std::move(runnable)];

  // Create the cancelable handle that the caller will own.
  auto cancelable = std::make_shared<ExecutorCancelable>(task);

  __weak __typeof__(impl_) weakImpl = impl_;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, absl::ToInt64Milliseconds(duration) * NSEC_PER_MSEC),
      dispatch_get_global_queue(DISPATCH_TARGET_QUEUE_DEFAULT, 0),
      // The block captures the task object, and ARC will keep it alive.
      [weakImpl, task]() {
        __strong __typeof__(weakImpl) strongImpl = weakImpl;
        if (!strongImpl || strongImpl.shuttingDown) {
          return;
        }
        [strongImpl.queue addOperationWithBlock:^{
          if (strongImpl.shuttingDown) {
            return;
          }
          GNCScheduledTaskState expected = GNCScheduledTaskState::kScheduled;
          if (task->_state.compare_exchange_strong(expected, GNCScheduledTaskState::kRunning)) {
            task->_runnable();
            task->_state.store(GNCScheduledTaskState::kDone);
          }
        }];
      });

  return cancelable;
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
