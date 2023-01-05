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

#include "internal/platform/feature_flags.h"
#include "internal/platform/runnable.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/future.h"

namespace nearby {

/**
 * Runnable wrapper that allows one to wait for the task
 * to complete if it is already running.
 */
class CancellableTask {
 public:
  explicit CancellableTask(Runnable&& runnable)
      : runnable_{std::move(runnable)} {}

  /**
   * Try to cancel the task and wait until completion if the task is already
   * running.
   */
  void CancelAndWaitIfStarted() {
    if (started_or_cancelled_.Set(true)) {
      if (FeatureFlags::GetInstance()
              .GetFlags()
              .cancel_waits_for_running_tasks) {
        // task could still be running, wait until finish
        finished_.Get();
      }
    } else {
      // mark as finished to support multiple calls to this method
      finished_.Set(true);
    }
  }

  void operator()() {
    if (started_or_cancelled_.Set(true)) return;
    runnable_();
    finished_.Set(true);
  }

 private:
  AtomicBoolean started_or_cancelled_;
  Future<bool> finished_;
  Runnable runnable_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_CANCELLABLE_TASK_H_
