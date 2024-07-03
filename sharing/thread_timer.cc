// Copyright 2024 Google LLC
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

#include "sharing/thread_timer.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/debugging/leak_check.h"
#include "absl/functional/any_invocable.h"
#include "absl/time/time.h"
#include "internal/platform/task_runner.h"
#include "sharing/internal/public/logging.h"

namespace nearby::sharing {

ThreadTimer::ThreadTimer(TaskRunner& task_runner, std::string name,
                         absl::Duration delay, absl::AnyInvocable<void()> task)
    : name_(std::move(name)) {
  auto run_cnt = std::make_unique<std::atomic<int8_t>>(0);
  // Do not capture any member variables in the lambda. as the object may be
  // deleted before the task is run.
  if (task_runner.PostDelayedTask(
          delay, [run_cnt = run_cnt.get(), task = std::move(task),
                  name = name_]() mutable {
            if (run_cnt->fetch_add(1) == 0) {
              // Timer has not been cancelled, run the task.
              NL_LOG(INFO) << "Timer " << name << " fired.";
              std::move(task)();
            } else {
              // Timer has been cancelled, need to delete the run_cnt.
              NL_VLOG(1) << "Timer " << name << " expired but was cancelled.";
              delete run_cnt;
            }
          })) {
    // During tests, long running timers may not expire leaving run_cnt_
    // undeleted.  Ignore leaks here to reduce noise.
    run_cnt_ = absl::IgnoreLeak(run_cnt.release());
  }
}

ThreadTimer::~ThreadTimer() { Cancel(); }

void ThreadTimer::Cancel() {
  if (run_cnt_ != nullptr) {
    NL_LOG(INFO) << "Timer " << name_ << " cancelled.";
    if (run_cnt_->fetch_add(1) > 0) {
      // Timer has already fired, delete the run_cnt.
      delete run_cnt_;
    }
    run_cnt_ = nullptr;
  }
}

bool ThreadTimer::IsRunning() {
  if (run_cnt_ == nullptr) {
    return false;
  }
  return run_cnt_->load() == 0;
}

}  // namespace nearby::sharing
