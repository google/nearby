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

#include "internal/test/test_single_thread_executor.h"

#include <atomic>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/test/fake_clock.h"

namespace nearby {
namespace {

using ::testing::ElementsAre;

TEST(TestSingleThreadExecutorTest, Execute) {
  FakeClock clock;
  TestSingleThreadExecutor executor(&clock);
  std::atomic<int> count = 0;

  executor.Execute([&count] { count.fetch_add(1, std::memory_order_seq_cst); });
  executor.Sync();

  EXPECT_EQ(count.load(std::memory_order_seq_cst), 1);
}

TEST(TestSingleThreadExecutorTest, DelayedExecuteUsesFakeClock) {
  FakeClock clock;
  TestSingleThreadExecutor executor(&clock);
  std::atomic<int> count = 0;

  EXPECT_TRUE(executor.DelayedExecute(absl::Seconds(10), [&count] {
    count.fetch_add(1, std::memory_order_seq_cst);
  }));
  executor.Sync();
  EXPECT_EQ(count.load(std::memory_order_seq_cst), 0);

  clock.FastForward(absl::Seconds(10));
  EXPECT_TRUE(executor.SyncWithTimeout(absl::Seconds(1)));
  EXPECT_EQ(count.load(std::memory_order_seq_cst), 1);
}

TEST(TestSingleThreadExecutorTest,
     ZeroAndNegativeDelayedExecuteRunWithoutAdvancingClock) {
  FakeClock clock;
  TestSingleThreadExecutor executor(&clock);
  std::vector<int> results;

  EXPECT_FALSE(executor.DelayedExecute(absl::ZeroDuration(), nullptr));
  EXPECT_TRUE(executor.Execute([&results] { results.push_back(1); }));
  EXPECT_TRUE(executor.DelayedExecute(absl::ZeroDuration(),
                                      [&results] { results.push_back(2); }));
  EXPECT_TRUE(executor.DelayedExecute(-absl::Seconds(5),
                                      [&results] { results.push_back(3); }));
  executor.Sync();

  EXPECT_THAT(results, ElementsAre(1, 2, 3));
}

TEST(TestSingleThreadExecutorTest, TasksRunInSequence) {
  FakeClock clock;
  TestSingleThreadExecutor executor(&clock);
  std::vector<int> results;

  executor.Execute([&results] {
    absl::SleepFor(absl::Milliseconds(20));
    results.push_back(1);
  });
  executor.Execute([&results] { results.push_back(2); });
  executor.Sync();

  ASSERT_EQ(results.size(), 2);
  EXPECT_EQ(results[0], 1);
  EXPECT_EQ(results[1], 2);
}

TEST(TestSingleThreadExecutorTest, DelayedExecuteInsideDelayedExecute) {
  FakeClock clock;
  TestSingleThreadExecutor executor(&clock);
  std::atomic<int> count = 0;

  EXPECT_TRUE(
      executor.DelayedExecute(absl::Seconds(2), [&count, &executor] {
        count.fetch_add(1, std::memory_order_seq_cst);
        executor.DelayedExecute(absl::Seconds(2), [&count] {
          count.fetch_add(1, std::memory_order_seq_cst);
        });
      }));
  clock.FastForward(absl::Seconds(2));
  executor.Sync();
  EXPECT_EQ(count.load(std::memory_order_seq_cst), 1);

  clock.FastForward(absl::Seconds(2));
  executor.Sync();
  EXPECT_EQ(count.load(std::memory_order_seq_cst), 2);
}

TEST(TestSingleThreadExecutorTest, SyncWithTimeoutExpiresSafely) {
  absl::Notification task_started;
  absl::Notification unblock_task;
  FakeClock clock;
  TestSingleThreadExecutor executor(&clock);

  executor.Execute([&task_started, &unblock_task] {
    task_started.Notify();
    unblock_task.WaitForNotification();
  });
  task_started.WaitForNotification();

  EXPECT_FALSE(executor.SyncWithTimeout(absl::Milliseconds(10)));

  unblock_task.Notify();
  executor.Sync();
}

TEST(TestSingleThreadExecutorTest, IsRunningOnExecutorTrueInsideTasks) {
  FakeClock clock;
  TestSingleThreadExecutor executor(&clock);
  TestSingleThreadExecutor other_executor(&clock);
  std::atomic<bool> on_executor_in_execute = false;
  std::atomic<bool> on_other_in_execute = true;
  std::atomic<bool> on_executor_in_delayed = false;
  std::atomic<bool> on_other_in_delayed = true;

  EXPECT_FALSE(executor.IsRunningOnExecutor());

  EXPECT_TRUE(executor.Execute([&] {
    on_executor_in_execute.store(executor.IsRunningOnExecutor(),
                                 std::memory_order_relaxed);
    on_other_in_execute.store(other_executor.IsRunningOnExecutor(),
                              std::memory_order_relaxed);
  }));
  executor.Sync();
  EXPECT_TRUE(on_executor_in_execute.load(std::memory_order_relaxed));
  EXPECT_FALSE(on_other_in_execute.load(std::memory_order_relaxed));
  EXPECT_FALSE(executor.IsRunningOnExecutor());

  EXPECT_TRUE(executor.DelayedExecute(absl::Seconds(5), [&] {
    on_executor_in_delayed.store(executor.IsRunningOnExecutor(),
                                 std::memory_order_relaxed);
    on_other_in_delayed.store(other_executor.IsRunningOnExecutor(),
                              std::memory_order_relaxed);
  }));
  clock.FastForward(absl::Seconds(5));
  executor.Sync();
  EXPECT_TRUE(on_executor_in_delayed.load(std::memory_order_relaxed));
  EXPECT_FALSE(on_other_in_delayed.load(std::memory_order_relaxed));
  EXPECT_FALSE(executor.IsRunningOnExecutor());
}

}  // namespace
}  // namespace nearby
