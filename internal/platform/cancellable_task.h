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

#ifndef PLATFORM_PUBLIC_CANCELLABLE_TASK_H_
#define PLATFORM_PUBLIC_CANCELLABLE_TASK_H_

#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/runnable.h"

namespace nearby {

/**
 * Runnable wrapper that allows one to wait for the task
 * to complete if it is already running.
 */
class CancellableTask {
 public:
  explicit CancellableTask(Runnable&& runnable)
      : CancellableTask(std::move(runnable), /*is_repeated=*/false) {}

  explicit CancellableTask(Runnable&& runnable, bool is_repeated_)
      : is_repeated_{is_repeated_}, runnable_{std::move(runnable)} {}

  ~CancellableTask() { CancelAndWaitIfStarted(); }

  /**
   * Try to cancel the task and wait until completion if the task is already
   * running.
   */
  void CancelAndWaitIfStarted() {
    absl::MutexLock lock(mutex_);
    is_cancelled_ = true;
    while (is_running_) {
      cond_.Wait(&mutex_);
    }
  }

  void operator()() {
    {
      absl::MutexLock lock(mutex_);
      if (is_cancelled_ || is_running_) return;
      if (!is_repeated_ && was_run_) return;
      was_run_ = true;
      is_running_ = true;
    }
    runnable_();
    {
      absl::MutexLock lock(mutex_);
      is_running_ = false;
      cond_.SignalAll();
    }
  }

 private:
  const bool is_repeated_;
  absl::Mutex mutex_;
  absl::CondVar cond_;
  bool is_cancelled_ ABSL_GUARDED_BY(mutex_){false};
  bool was_run_ ABSL_GUARDED_BY(mutex_){false};
  bool is_running_ ABSL_GUARDED_BY(mutex_){false};
  Runnable runnable_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_CANCELLABLE_TASK_H_
