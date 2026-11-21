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

#ifndef PLATFORM_API_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_API_SINGLE_THREAD_EXECUTOR_H_

#include "absl/time/time.h"
#include "internal/platform/implementation/executor.h"
#include "internal/platform/runnable.h"

namespace nearby::api {

// An Executor that executes submitted and delayed tasks sequentially.
class SingleThreadExecutor : public Executor {
 public:
  ~SingleThreadExecutor() override = default;

  // Schedules `runnable` to execute after `delay` (clamped to `[0, INT_MAX]`
  // milliseconds). Returns `true` if `runnable` was scheduled; returns `false`
  // if `runnable` is empty, the executor has been shut down, or scheduling
  // fails. Pending delayed tasks that have not yet started are cancelled on
  // `Shutdown()`.
  virtual bool DelayedExecute(absl::Duration delay, Runnable&& runnable) = 0;

  // Returns `true` if the calling thread is currently executing a task on this
  // executor, and `false` otherwise.
  virtual bool IsRunningOnExecutor() const = 0;
};

}  // namespace nearby::api

#endif  // PLATFORM_API_SINGLE_THREAD_EXECUTOR_H_
