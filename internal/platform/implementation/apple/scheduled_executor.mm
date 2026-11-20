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

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#import "internal/platform/implementation/apple/Log/GNCLogger.h"
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

/** Thread-safe wrapper around `NSOperationQueue` coordinating submission with shutdown. */
@interface GNCOperationQueueImpl : NSObject
/** Creates an instance with the given `maxConcurrency`. */
+ (instancetype)implWithMaxConcurrency:(int)maxConcurrency;
/** Enqueues `block` unless shutdown has started; returns `YES` if queued. */
- (BOOL)submitWithBlock:(dispatch_block_t)block;
/** Returns `YES` if shutdown has started. */
- (BOOL)isShuttingDown;
/** Marks the queue as shutting down; returns `YES` on the first call only. */
- (BOOL)beginShutdown;
/** Blocks until all queued operations finish without holding the internal mutex. */
- (void)waitUntilAllOperationsAreFinished;
@end

@implementation GNCOperationQueueImpl {
  NSOperationQueue *_queue;
  absl::Mutex _mutex;
  BOOL _shuttingDown ABSL_GUARDED_BY(_mutex);
}

+ (instancetype)implWithMaxConcurrency:(int)maxConcurrency {
  GNCOperationQueueImpl *impl = [[GNCOperationQueueImpl alloc] init];
  if (impl) {
    impl->_queue = [[NSOperationQueue alloc] init];
    impl->_queue.maxConcurrentOperationCount = maxConcurrency;
  }
  return impl;
}

- (BOOL)submitWithBlock:(dispatch_block_t)block {
  absl::MutexLock lock(_mutex);
  if (_shuttingDown) {
    return NO;
  }
  [_queue addOperationWithBlock:block];
  return YES;
}

- (BOOL)isShuttingDown {
  absl::MutexLock lock(_mutex);
  return _shuttingDown;
}

- (BOOL)beginShutdown {
  absl::MutexLock lock(_mutex);
  if (_shuttingDown) {
    return NO;
  }
  _shuttingDown = YES;
  return YES;
}

- (void)waitUntilAllOperationsAreFinished {
  [_queue waitUntilAllOperationsAreFinished];
}

@end

namespace nearby {
namespace apple {
namespace {

#if defined(__APPLE__) && !defined(GITHUB_BUILD) && defined(__cpp_exceptions)
static_assert(false, "Nearby Apple platform implementation must be compiled with "
                     "-fno-exceptions");
#endif

void ExecuteRunnable(Runnable &runnable) {
  @try {
#if defined(__cpp_exceptions)
    try {
      runnable();
    } catch (const std::exception &e) {
      GNCLoggerError(@"Runnable threw C++ exception: %s", e.what());
    } catch (...) {
      GNCLoggerError(@"Runnable threw unknown C++ exception");
    }
#else
    runnable();
#endif
  } @catch (NSException *e) {
    GNCLoggerError(@"Runnable threw ObjC exception: %@: %@", e.name, e.reason);
  }
}

}  // namespace

ScheduledExecutor::ScheduledExecutor() { impl_ = [GNCOperationQueueImpl implWithMaxConcurrency:1]; }

ScheduledExecutor::ScheduledExecutor(int max_concurrency) {
  impl_ = [GNCOperationQueueImpl implWithMaxConcurrency:max_concurrency];
}

ScheduledExecutor::~ScheduledExecutor() {
  Shutdown();
  impl_ = nil;
}

std::shared_ptr<api::Cancelable> ScheduledExecutor::Schedule(Runnable &&runnable,
                                                             absl::Duration duration) {
  if ([impl_ isShuttingDown]) return std::shared_ptr<api::Cancelable>(nullptr);

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
        if (!strongImpl) {
          return;
        }
        [strongImpl submitWithBlock:^{
          if ([strongImpl isShuttingDown]) {
            return;
          }
          GNCScheduledTaskState expected = GNCScheduledTaskState::kScheduled;
          if (task->_state.compare_exchange_strong(expected, GNCScheduledTaskState::kRunning)) {
            ExecuteRunnable(task->_runnable);
            task->_state.store(GNCScheduledTaskState::kDone);
          }
        }];
      });

  return cancelable;
}

bool ScheduledExecutor::Execute(Runnable &&runnable) {
  if (!runnable) {
    return false;
  }
  // Submit the runnable to the queue if not shutting down.
  __block Runnable local_runnable = std::move(runnable);
  return [impl_ submitWithBlock:^{
    ExecuteRunnable(local_runnable);
  }];
}

void ScheduledExecutor::Shutdown() {
  // Prevent new/delayed operations from being queued/executed.
  if (![impl_ beginShutdown]) {
    return;
  }

  // Block until all currently executing operations finish.
  dispatch_group_t group = dispatch_group_create();
  dispatch_group_async(group, dispatch_get_global_queue(DISPATCH_TARGET_QUEUE_DEFAULT, 0), ^{
    [impl_ waitUntilAllOperationsAreFinished];
  });
  dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
}

}  // namespace apple
}  // namespace nearby
