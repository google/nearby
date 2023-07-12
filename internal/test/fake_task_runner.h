// Copyright 2022-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_TASK_RUNNER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_TASK_RUNNER_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/time/time.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/task_runner.h"
#include "internal/platform/timer.h"
#include "internal/test/fake_clock.h"

namespace nearby {

class FakeTaskRunner : public TaskRunner {
 public:
  FakeTaskRunner(FakeClock* clock, uint32_t count)
      : clock_(clock),
        count_(count),
        task_executor_(std::make_unique<MultiThreadExecutor>(count)) {}
  ~FakeTaskRunner() override ABSL_LOCKS_EXCLUDED(mutex_);

  bool PostTask(absl::AnyInvocable<void()> task) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // No matter the mode is pending or not, always put the task in timer control.
  // Caller can move forward time to trigger it.
  bool PostDelayedTask(absl::Duration delay,
                       absl::AnyInvocable<void()> task) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Wait for all thread completed.
  void Sync();

  // In some test cases, we only need to wait for a timeout .
  bool SyncWithTimeout(absl::Duration timeout);

  // In some test cases, we need to make sure all running tasks completion
  // before go to next task. This method can be used for the purpose.
  static bool WaitForRunningTasksWithTimeout(absl::Duration timeout);

 private:
  mutable absl::Mutex mutex_;
  FakeClock* clock_ = nullptr;
  uint32_t count_ = 0;
  std::unique_ptr<MultiThreadExecutor> task_executor_ ABSL_GUARDED_BY(mutex_) =
      nullptr;

  // Tracks delayed tasks.
  std::vector<std::unique_ptr<Timer>> timers_ ABSL_GUARDED_BY(mutex_);

  static std::atomic_uint total_running_thread_count_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_TASK_RUNNER_H_
