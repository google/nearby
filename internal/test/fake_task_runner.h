// Copyright 2022 Google LLC
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
#include <future>  //NOLINT
#include <memory>
#include <thread>  //NOLINT
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "internal/platform/task_runner.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_timer.h"

namespace nearby {

class FakeTaskRunner : public TaskRunner {
 public:
  enum class Mode { kNoPending, kPending };

  FakeTaskRunner(FakeClock* clock, uint32_t count)
      : clock_(clock), count_(count) {}
  ~FakeTaskRunner() override = default;

  bool PostTask(absl::AnyInvocable<void()> task) override;

  // No matter the mode is pending or not, always put the task in timer control.
  // Caller can move forward time to trigger it.
  bool PostDelayedTask(absl::Duration delay,
                       absl::AnyInvocable<void()> task) override;

  // Mocked methods.
  void SetMode(Mode mode) { mode_ = mode; }
  Mode GetMode() const { return mode_; }

  void RunNextTask();
  void RunAllPendingTasks();

  const std::vector<absl::AnyInvocable<void()>>& GetPendingTasks() const;
  const absl::flat_hash_map<uint32_t, std::unique_ptr<Timer>>&
  GetPendingDelayedTask();

  int GetConcurrentCount() const { return count_; }

  // In some testcases, we needs to make sure all running tasks completion
  // before go to next task. This method can be used for the purpose.
  static bool WaitForRunningTasksWithTimeout(absl::Duration timeout);

 private:
  uint32_t GenerateId();
  void CleanThreads() ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void run(absl::AnyInvocable<void()> task) ABSL_LOCKS_EXCLUDED(mutex_);

  Mode mode_ = Mode::kNoPending;
  std::atomic_uint current_id_ = 0;
  FakeClock* clock_ = nullptr;
  uint32_t count_;
  std::vector<absl::AnyInvocable<void()>> pending_tasks_;
  std::vector<uint32_t> completed_delayed_tasks_;
  absl::flat_hash_map<uint32_t, std::unique_ptr<Timer>> pending_delayed_tasks_;
  absl::Mutex mutex_;
  std::vector<std::future<void>> threads_ ABSL_GUARDED_BY(mutex_);

  static std::atomic_uint running_thread_count_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_TASK_RUNNER_H_
