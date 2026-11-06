// Copyright 2026 Google LLC
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

#import "internal/platform/implementation/apple/single_thread_executor.h"

#import <Foundation/Foundation.h>
#include <pthread.h>

#if defined(__cpp_exceptions)
#include <exception>
#endif
#include <memory>
#include <utility>

#include "absl/synchronization/mutex.h"
#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace apple {
namespace {

#if defined(__APPLE__) && !defined(GITHUB_BUILD) && defined(__cpp_exceptions)
static_assert(false, "Nearby Apple platform implementation must be compiled with "
                     "-fno-exceptions");
#endif

void ExecuteRunnable(Runnable& runnable) {
  @try {
#if defined(__cpp_exceptions)
    try {
      runnable();
    } catch (const std::exception& e) {
      GNCLoggerError(@"Runnable threw C++ exception: %s", e.what());
    } catch (...) {
      GNCLoggerError(@"Runnable threw unknown C++ exception");
    }
#else
    runnable();
#endif
  } @catch (NSException* e) {
    GNCLoggerError(@"Runnable threw ObjC exception: %@: %@", e.name, e.reason);
  }
}

}  // namespace

SingleThreadExecutor::SingleThreadExecutor()
    : state_(std::make_shared<WorkerState>()) {
  auto* thread_state = new std::shared_ptr<WorkerState>(state_);
  int rc = pthread_create(&thread_, /*attr=*/nullptr,
                          &SingleThreadExecutor::ThreadMain, thread_state);
  if (rc == 0) {
    thread_started_ = true;
  } else {
    delete thread_state;
    GNCLoggerError(@"Failed to create SingleThreadExecutor worker thread: %d",
                   rc);
    absl::MutexLock lock(state_->mutex);
    state_->shutting_down = true;
  }
}

SingleThreadExecutor::~SingleThreadExecutor() {
  Shutdown();
  if (thread_started_) {
    absl::MutexLock join_lock(join_mutex_);
    if (!thread_joined_) {
      pthread_detach(thread_);
      thread_joined_ = true;
    }
  }
}

void SingleThreadExecutor::Execute(Runnable&& runnable) {
  DoSubmit(std::move(runnable));
}

bool SingleThreadExecutor::DoSubmit(Runnable&& runnable) {
  absl::MutexLock lock(state_->mutex);
  if (state_->shutting_down) {
    return false;
  }
  state_->queue.push_back(std::move(runnable));
  return true;
}

void SingleThreadExecutor::Shutdown() {
  {
    absl::MutexLock lock(state_->mutex);
    state_->shutting_down = true;
  }
  if (!thread_started_ || pthread_equal(pthread_self(), thread_)) {
    return;
  }
  absl::MutexLock join_lock(join_mutex_);
  if (!thread_joined_) {
    pthread_join(thread_, /*value_ptr=*/nullptr);
    thread_joined_ = true;
  }
}

// static
void* SingleThreadExecutor::ThreadMain(void* arg) {
  pthread_setname_np("com.google.nearby.SingleThreadExecutor");
  std::unique_ptr<std::shared_ptr<WorkerState>> state_holder(
      static_cast<std::shared_ptr<WorkerState>*>(arg));
  RunWorkerLoop(**state_holder);
  return nullptr;
}

// static
void SingleThreadExecutor::RunWorkerLoop(WorkerState& state) {
  while (true) {
    Runnable runnable;
    {
      auto has_work_or_shutting_down =
          [&state]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(state.mutex) {
            return !state.queue.empty() || state.shutting_down;
          };
      absl::MutexLock lock(state.mutex,
                           absl::Condition(&has_work_or_shutting_down));
      if (state.queue.empty()) {
        return;
      }
      runnable = std::move(state.queue.front());
      state.queue.pop_front();
    }
    if (runnable) {
      @autoreleasepool {
        ExecuteRunnable(runnable);
        runnable = nullptr;
      }
    }
  }
}

}  // namespace apple
}  // namespace nearby
