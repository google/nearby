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

#include "internal/platform/cancellable_task.h"

#include <atomic>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace {

TEST(CancellableTaskTest, CanRunTaskOnce) {
  std::atomic<bool> executed{false};
  CancellableTask task([&executed]() { executed = true; });

  task();
  EXPECT_TRUE(executed.load());

  // Second execution on non-repeated task should not run again.
  executed = false;
  task();
  EXPECT_FALSE(executed.load());
}

TEST(CancellableTaskTest, CancelBeforeRunPreventsExecution) {
  std::atomic<bool> executed{false};
  CancellableTask task([&executed]() { executed = true; });

  task.CancelAndWaitIfStarted();
  task();
  EXPECT_FALSE(executed.load());
}

TEST(CancellableTaskTest, MultipleCancelsAreSafe) {
  std::atomic<bool> executed{false};
  CancellableTask task([&executed]() { executed = true; });

  task.CancelAndWaitIfStarted();
  task.CancelAndWaitIfStarted();
  task();
  EXPECT_FALSE(executed.load());
}

TEST(CancellableTaskTest, CancelAndWaitWaitsForRunningTaskToFinish) {
  SingleThreadExecutor task_executor;
  std::atomic<bool> task_finished{false};
  absl::Notification task_started;
  absl::Notification allow_task_finish;

  CancellableTask task([&]() {
    task_started.Notify();
    allow_task_finish.WaitForNotification();
    task_finished = true;
  });

  // Start executing the task on background thread.
  task_executor.Execute([&task]() { task(); });

  // Wait until the task has actively started running.
  ASSERT_TRUE(task_started.WaitForNotificationWithTimeout(absl::Seconds(3)));

  // Run CancelAndWaitIfStarted on a background executor.
  SingleThreadExecutor cancel_executor;
  absl::Notification cancel_started;
  absl::Notification cancel_done;

  cancel_executor.Execute([&]() {
    cancel_started.Notify();
    task.CancelAndWaitIfStarted();
    cancel_done.Notify();
  });

  ASSERT_TRUE(cancel_started.WaitForNotificationWithTimeout(absl::Seconds(3)));
  // Sleep briefly to ensure CancelAndWaitIfStarted has entered wait state.
  SystemClock::Sleep(absl::Milliseconds(100));

  // Verify that CancelAndWaitIfStarted is blocked and hasn't finished yet.
  EXPECT_FALSE(cancel_done.HasBeenNotified());
  EXPECT_FALSE(task_finished.load());

  // Allow the task to finish, which should unblock CancelAndWaitIfStarted.
  allow_task_finish.Notify();

  EXPECT_TRUE(cancel_done.WaitForNotificationWithTimeout(absl::Seconds(3)));
  EXPECT_TRUE(task_finished.load());
}

TEST(CancellableTaskTest, RepeatedTaskCanRunMultipleTimes) {
  std::atomic<int> run_count{0};
  CancellableTask task([&run_count]() { ++run_count; },
                       /*is_repeated=*/true);

  task();
  EXPECT_EQ(run_count.load(), 1);

  task();
  EXPECT_EQ(run_count.load(), 2);

  task();
  EXPECT_EQ(run_count.load(), 3);

  // After cancel, it should not run again.
  task.CancelAndWaitIfStarted();
  task();
  EXPECT_EQ(run_count.load(), 3);
}

TEST(CancellableTaskTest, DestructorWaitsForRunningTaskToFinish) {
  SingleThreadExecutor executor;
  std::atomic<bool> task_finished{false};
  absl::Notification task_started;
  absl::Notification allow_task_finish;

  {
    CancellableTask task([&]() {
      task_started.Notify();
      allow_task_finish.WaitForNotification();
      task_finished = true;
    });

    // Start executing the task on background thread.
    executor.Execute([&task]() { task(); });

    // Wait until the task has actively started running.
    ASSERT_TRUE(task_started.WaitForNotificationWithTimeout(absl::Seconds(3)));

    // Asynchronously allow task to finish after a short delay.
    SingleThreadExecutor unblock_executor;
    unblock_executor.Execute([&]() {
      SystemClock::Sleep(absl::Milliseconds(100));
      allow_task_finish.Notify();
    });

    // Destructor of task runs here at end of scope and should block until
    // the running task finishes.
  }

  EXPECT_TRUE(task_finished.load());
}

}  // namespace
}  // namespace nearby
