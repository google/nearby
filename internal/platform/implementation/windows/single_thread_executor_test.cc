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

#include "internal/platform/implementation/windows/single_thread_executor.h"

#include <atomic>
#include <vector>

#include "gtest/gtest.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"

namespace nearby::windows {
namespace {

constexpr absl::Duration kWaitTimeout = absl::Seconds(5);

TEST(WindowsSingleThreadExecutorTest, IsRunningOnExecutorMatchesActiveWorker) {
  SingleThreadExecutor executor;
  SingleThreadExecutor other_executor;
  absl::Notification execute_done;
  absl::Notification delayed_done;
  std::atomic<bool> on_executor_in_execute = false;
  std::atomic<bool> on_other_in_execute = true;
  std::atomic<bool> on_executor_in_delayed = false;
  std::atomic<bool> on_other_in_delayed = true;

  EXPECT_FALSE(executor.IsRunningOnExecutor());

  EXPECT_TRUE(executor.Execute([&]() {
    on_executor_in_execute.store(executor.IsRunningOnExecutor(),
                                 std::memory_order_relaxed);
    on_other_in_execute.store(other_executor.IsRunningOnExecutor(),
                              std::memory_order_relaxed);
    execute_done.Notify();
  }));
  ASSERT_TRUE(execute_done.WaitForNotificationWithTimeout(kWaitTimeout));
  EXPECT_TRUE(on_executor_in_execute.load(std::memory_order_relaxed));
  EXPECT_FALSE(on_other_in_execute.load(std::memory_order_relaxed));

  EXPECT_TRUE(executor.DelayedExecute(absl::Milliseconds(20), [&]() {
    on_executor_in_delayed.store(executor.IsRunningOnExecutor(),
                                 std::memory_order_relaxed);
    on_other_in_delayed.store(other_executor.IsRunningOnExecutor(),
                              std::memory_order_relaxed);
    delayed_done.Notify();
  }));
  ASSERT_TRUE(delayed_done.WaitForNotificationWithTimeout(kWaitTimeout));
  EXPECT_TRUE(on_executor_in_delayed.load(std::memory_order_relaxed));
  EXPECT_FALSE(on_other_in_delayed.load(std::memory_order_relaxed));

  executor.Shutdown();
  EXPECT_FALSE(executor.IsRunningOnExecutor());
}

TEST(WindowsSingleThreadExecutorTest,
     DelayedExecuteHandlesZeroNegativeEmptyAndShutdown) {
  SingleThreadExecutor executor;
  absl::BlockingCounter immediate_counter(2);
  std::atomic<bool> canceled_task_ran = false;

  EXPECT_FALSE(executor.DelayedExecute(absl::Milliseconds(50), nullptr));
  EXPECT_TRUE(executor.DelayedExecute(
      absl::ZeroDuration(), [&]() { immediate_counter.DecrementCount(); }));
  EXPECT_TRUE(executor.DelayedExecute(
      -absl::Seconds(1), [&]() { immediate_counter.DecrementCount(); }));
  immediate_counter.Wait();

  EXPECT_TRUE(executor.DelayedExecute(absl::Seconds(10), [&]() {
    canceled_task_ran.store(true, std::memory_order_relaxed);
  }));
  executor.Shutdown();

  EXPECT_FALSE(executor.DelayedExecute(absl::Milliseconds(10), []() {}));
  EXPECT_FALSE(executor.DelayedExecute(absl::ZeroDuration(), nullptr));
  EXPECT_FALSE(canceled_task_ran.load(std::memory_order_relaxed));
}

TEST(WindowsSingleThreadExecutorTest,
     ShutdownDrainsQueuedImmediateTasksAndRejectsPostShutdownExecute) {
  SingleThreadExecutor executor;
  absl::Notification first_started;
  absl::Notification release_first;
  std::vector<int> executed;

  EXPECT_FALSE(executor.Execute(nullptr));
  EXPECT_TRUE(executor.Execute([&]() {
    first_started.Notify();
    release_first.WaitForNotification();
    executed.push_back(1);
  }));
  ASSERT_TRUE(first_started.WaitForNotificationWithTimeout(kWaitTimeout));

  EXPECT_TRUE(executor.Execute([&]() { executed.push_back(2); }));
  EXPECT_TRUE(executor.Execute([&]() { executed.push_back(3); }));

  release_first.Notify();
  executor.Shutdown();

  EXPECT_EQ(executed, (std::vector<int>{1, 2, 3}));
  EXPECT_FALSE(executor.Execute([]() {}));
}

}  // namespace
}  // namespace nearby::windows
