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

#include "internal/platform/implementation/shared/multi_thread_executor.h"

#include <atomic>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/shared/single_thread_executor.h"
#include "internal/platform/runnable.h"

namespace nearby::shared {
namespace {

using ::testing::ElementsAre;

constexpr absl::Duration kWaitTimeout = absl::Seconds(5);

bool WaitUntilInShutdown(const MultiThreadExecutor& executor,
                         absl::Duration timeout) {
  absl::Time deadline = absl::Now() + timeout;
  while (!executor.InShutdown()) {
    if (absl::Now() >= deadline) {
      return false;
    }
    absl::SleepFor(absl::Milliseconds(1));
  }
  return true;
}

TEST(MultiThreadExecutorTest, InitialStateIsNotInShutdown) {
  MultiThreadExecutor multi_executor(2);
  SingleThreadExecutor single_executor;

  EXPECT_FALSE(multi_executor.InShutdown());
  EXPECT_FALSE(single_executor.InShutdown());
}

TEST(MultiThreadExecutorTest, ExecuteAndScheduleAfterRunTasks) {
  absl::Notification executed;
  absl::Notification scheduled;
  MultiThreadExecutor executor(2);

  EXPECT_TRUE(executor.Execute([&executed]() { executed.Notify(); }));
  EXPECT_TRUE(executor.ScheduleAfter(absl::Milliseconds(10),
                                     [&scheduled]() { scheduled.Notify(); }));

  EXPECT_TRUE(executed.WaitForNotificationWithTimeout(kWaitTimeout));
  EXPECT_TRUE(scheduled.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST(MultiThreadExecutorTest, ExecuteAndScheduleAfterIgnoreNullRunnable) {
  absl::Notification done;
  MultiThreadExecutor executor(2);

  EXPECT_FALSE(executor.Execute(Runnable()));
  EXPECT_FALSE(executor.ScheduleAfter(absl::ZeroDuration(), Runnable()));
  EXPECT_TRUE(executor.Execute([&done]() { done.Notify(); }));

  EXPECT_TRUE(done.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST(MultiThreadExecutorTest, ShutdownBlocksUntilInFlightTaskCompletes) {
  absl::Notification task_started;
  absl::Notification allow_finish;
  absl::Notification shutdown_returned;
  std::atomic<bool> task_finished = false;
  MultiThreadExecutor executor(2);
  SingleThreadExecutor shutdown_runner;

  executor.Execute([&]() {
    task_started.Notify();
    allow_finish.WaitForNotificationWithTimeout(kWaitTimeout);
    task_finished.store(true, std::memory_order_release);
  });
  ASSERT_TRUE(task_started.WaitForNotificationWithTimeout(kWaitTimeout));

  shutdown_runner.Execute([&]() {
    executor.Shutdown();
    shutdown_returned.Notify();
  });
  ASSERT_TRUE(WaitUntilInShutdown(executor, kWaitTimeout));
  bool returned_while_in_flight = shutdown_returned.HasBeenNotified();
  allow_finish.Notify();

  EXPECT_FALSE(returned_while_in_flight);
  EXPECT_TRUE(shutdown_returned.WaitForNotificationWithTimeout(kWaitTimeout));
  EXPECT_TRUE(task_finished.load(std::memory_order_acquire));
  EXPECT_TRUE(executor.InShutdown());
}

TEST(MultiThreadExecutorTest,
     ReentrantSubmissionsDuringShutdownDoNotDeadlock) {
  absl::Notification task_started;
  absl::Notification shutdown_entered;
  std::atomic<bool> reentrant_ran = false;
  MultiThreadExecutor executor(1);
  SingleThreadExecutor shutdown_runner;

  executor.Execute([&]() {
    task_started.Notify();
    shutdown_entered.WaitForNotificationWithTimeout(kWaitTimeout);
    executor.Execute(
        [&]() { reentrant_ran.store(true, std::memory_order_relaxed); });
    executor.ScheduleAfter(absl::ZeroDuration(), [&]() {
      reentrant_ran.store(true, std::memory_order_relaxed);
    });
  });
  ASSERT_TRUE(task_started.WaitForNotificationWithTimeout(kWaitTimeout));

  shutdown_runner.Execute([&]() { executor.Shutdown(); });
  ASSERT_TRUE(WaitUntilInShutdown(executor, kWaitTimeout));

  shutdown_entered.Notify();
  shutdown_runner.Shutdown();
  executor.Shutdown();

  EXPECT_FALSE(reentrant_ran.load(std::memory_order_relaxed));
  EXPECT_TRUE(executor.InShutdown());
}

TEST(MultiThreadExecutorTest, SubmissionsAfterShutdownDoNotRun) {
  std::atomic<bool> ran = false;
  MultiThreadExecutor executor(2);
  executor.Shutdown();

  EXPECT_FALSE(
      executor.Execute([&]() { ran.store(true, std::memory_order_relaxed); }));
  EXPECT_FALSE(executor.ScheduleAfter(
      absl::ZeroDuration(),
      [&]() { ran.store(true, std::memory_order_relaxed); }));
  EXPECT_FALSE(executor.Execute(Runnable()));
  EXPECT_FALSE(executor.ScheduleAfter(absl::ZeroDuration(), Runnable()));

  EXPECT_FALSE(ran.load(std::memory_order_relaxed));
}

TEST(SingleThreadExecutorTest, ExecutesTasksSequentiallyAndShutsDown) {
  std::vector<int> executed_order;
  absl::Notification all_done;
  SingleThreadExecutor executor;

  executor.Execute([&]() { executed_order.push_back(0); });
  executor.Execute([&]() { executed_order.push_back(1); });
  executor.Execute([&]() {
    executed_order.push_back(2);
    all_done.Notify();
  });
  ASSERT_TRUE(all_done.WaitForNotificationWithTimeout(kWaitTimeout));
  executor.Shutdown();

  EXPECT_TRUE(executor.InShutdown());
  EXPECT_THAT(executed_order, ElementsAre(0, 1, 2));
}

}  // namespace
}  // namespace nearby::shared
