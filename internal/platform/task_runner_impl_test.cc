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

#include "internal/platform/task_runner_impl.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/implementation/executor.h"
#include "internal/platform/task_runner.h"

namespace nearby {
namespace {

constexpr uint32_t kNumThreads[] = {1, 10};

class TaskRunnerImplTest : public ::testing::TestWithParam<uint32_t> {};

TEST_P(TaskRunnerImplTest, PostTask) {
  TaskRunnerImpl task_runner{GetParam()};
  absl::Notification notification;
  bool called = false;

  task_runner.PostTask([&called, &notification]() {
    called = true;
    notification.Notify();
  });
  notification.WaitForNotificationWithTimeout(absl::Milliseconds(100));
  EXPECT_TRUE(called);
}

TEST_F(TaskRunnerImplTest, PostSequenceTasks) {
  TaskRunnerImpl task_runner{1};
  std::vector<std::string> completed_tasks;
  absl::Notification notification;

  // Run the first task
  task_runner.PostTask([&completed_tasks, &notification]() {
    completed_tasks.push_back("task1");
    if (completed_tasks.size() == 2) {
      absl::SleepFor(absl::Milliseconds(100));
      notification.Notify();
    }
  });

  // Run the second task
  task_runner.PostTask([&completed_tasks, &notification]() {
    completed_tasks.push_back("task2");
    if (completed_tasks.size() == 2) {
      notification.Notify();
    }
  });

  notification.WaitForNotificationWithTimeout(absl::Milliseconds(200));
  ASSERT_EQ(completed_tasks.size(), 2u);
  EXPECT_EQ(completed_tasks[0], "task1");
  EXPECT_EQ(completed_tasks[1], "task2");
}

TEST_P(TaskRunnerImplTest, PostDelayedTask) {
  TaskRunnerImpl task_runner{GetParam()};
  std::atomic_bool first_task_started = false;
  CountDownLatch latch(2);

  // Run the first task
  task_runner.PostDelayedTask(absl::Milliseconds(50), [&]() {
    first_task_started = true;
    latch.CountDown();
  });

  // Run the second task
  task_runner.PostTask([&]() {
    EXPECT_FALSE(first_task_started);
    latch.CountDown();
  });

  latch.Await();
}

TEST_P(TaskRunnerImplTest, PostTwoDelayedTasks) {
  TaskRunnerImpl task_runner{GetParam()};
  std::atomic_bool first_task_started = false;
  CountDownLatch latch(2);

  // Run the first task
  task_runner.PostDelayedTask(absl::Milliseconds(500), [&]() {
    first_task_started = true;
    latch.CountDown();
  });

  // Run the second task
  task_runner.PostDelayedTask(absl::Milliseconds(1), [&]() {
    EXPECT_FALSE(first_task_started);
    latch.CountDown();
  });

  latch.Await();
}

TEST_P(TaskRunnerImplTest, PostMultipleTasks) {
  TaskRunnerImpl task_runner(GetParam());
  constexpr int kNumTasks = 10;
  CountDownLatch latch(kNumTasks);

  for (int i = 0; i < kNumTasks; i++) {
    task_runner.PostTask([&]() {
      absl::SleepFor(absl::Milliseconds(10));
      latch.CountDown();
    });
  }

  EXPECT_TRUE(latch.Await());
}

TEST_P(TaskRunnerImplTest, PostEmptyTask) {
  TaskRunnerImpl task_runner{GetParam()};
  EXPECT_TRUE(task_runner.PostTask(nullptr));
  EXPECT_TRUE(task_runner.PostDelayedTask(absl::Milliseconds(100), nullptr));
}

TEST_P(TaskRunnerImplTest, PostTaskAfterShutdown) {
  TaskRunnerImpl task_runner{GetParam()};
  task_runner.Shutdown();

  EXPECT_FALSE(task_runner.PostTask([]() {}));
}

TEST_P(TaskRunnerImplTest, PostDelayedTaskAfterShutdown) {
  TaskRunnerImpl task_runner{GetParam()};
  task_runner.Shutdown();

  EXPECT_FALSE(task_runner.PostDelayedTask(absl::Milliseconds(50), []() {}));
}

INSTANTIATE_TEST_SUITE_P(ParameterizedTaskRunnerImplTest, TaskRunnerImplTest,
                         ::testing::ValuesIn(kNumThreads));

TEST(SingleThreadTaskRunnerImplTest, PostTaskAndSequence) {
  SingleThreadTaskRunnerImpl task_runner;
  std::vector<std::string> completed_tasks;
  absl::Notification notification;

  EXPECT_TRUE(task_runner.PostTask(
      [&completed_tasks]() { completed_tasks.push_back("task1"); }));
  EXPECT_TRUE(task_runner.PostTask([&completed_tasks, &notification]() {
    completed_tasks.push_back("task2");
    notification.Notify();
  }));

  ASSERT_TRUE(notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
  ASSERT_EQ(completed_tasks.size(), 2u);
  EXPECT_EQ(completed_tasks[0], "task1");
  EXPECT_EQ(completed_tasks[1], "task2");
}

TEST(SingleThreadTaskRunnerImplTest, PostDelayedTasks) {
  SingleThreadTaskRunnerImpl task_runner;
  std::atomic_bool slow_task_started = false;
  CountDownLatch latch(2);

  EXPECT_TRUE(
      task_runner.PostDelayedTask(absl::Milliseconds(200), [&]() {
        slow_task_started = true;
        latch.CountDown();
      }));
  EXPECT_TRUE(task_runner.PostDelayedTask(absl::Milliseconds(1), [&]() {
    EXPECT_FALSE(slow_task_started);
    latch.CountDown();
  }));

  EXPECT_TRUE(latch.Await());
}

TEST(SingleThreadTaskRunnerImplTest, PostEmptyAndAfterShutdown) {
  SingleThreadTaskRunnerImpl task_runner;
  EXPECT_TRUE(task_runner.PostTask(nullptr));
  EXPECT_TRUE(task_runner.PostDelayedTask(absl::Milliseconds(50), nullptr));

  task_runner.Shutdown();
  EXPECT_FALSE(task_runner.PostTask([]() {}));
  EXPECT_FALSE(task_runner.PostDelayedTask(absl::Milliseconds(50), []() {}));
}

TEST(SingleThreadTaskRunnerImplTest, GetThreadIdMatchesWorkerThread) {
  SingleThreadTaskRunnerImpl task_runner;
  const SingleThreadTaskRunner& base_ref = task_runner;
  const ThreadId caller_tid = api::GetCurrentThreadId();
  const ThreadId runner_tid = base_ref.GetThreadId();

  EXPECT_NE(caller_tid, 0u);
  EXPECT_NE(runner_tid, 0u);
  EXPECT_NE(runner_tid, caller_tid);

  std::vector<ThreadId> observed_tids;
  absl::Notification delayed_done;

  ASSERT_TRUE(
      task_runner.PostTask([&observed_tids, &task_runner, runner_tid]() {
        EXPECT_EQ(api::GetCurrentThreadId(), task_runner.GetThreadId());
        EXPECT_EQ(api::GetCurrentThreadId(), runner_tid);
        observed_tids.push_back(api::GetCurrentThreadId());
      }));
  ASSERT_TRUE(task_runner.PostTask([&observed_tids]() {
    observed_tids.push_back(api::GetCurrentThreadId());
  }));
  ASSERT_TRUE(task_runner.PostDelayedTask(
      absl::Milliseconds(10), [&observed_tids, &delayed_done]() {
        observed_tids.push_back(api::GetCurrentThreadId());
        delayed_done.Notify();
      }));

  ASSERT_TRUE(delayed_done.WaitForNotificationWithTimeout(absl::Seconds(1)));
  ASSERT_EQ(observed_tids.size(), 3u);
  EXPECT_EQ(observed_tids[0], runner_tid);
  EXPECT_EQ(observed_tids[1], runner_tid);
  EXPECT_EQ(observed_tids[2], runner_tid);
}

TEST(SingleThreadTaskRunnerImplTest,
     DifferentTaskRunnersHaveDistinctThreadIds) {
  SingleThreadTaskRunnerImpl runner1;
  SingleThreadTaskRunnerImpl runner2;

  EXPECT_NE(runner1.GetThreadId(), 0u);
  EXPECT_NE(runner2.GetThreadId(), 0u);
  EXPECT_NE(runner1.GetThreadId(), runner2.GetThreadId());

  ThreadId tid1 = 0;
  ThreadId tid2 = 0;
  absl::Notification done1;
  absl::Notification done2;
  ASSERT_TRUE(runner1.PostTask([&tid1, &done1]() {
    tid1 = api::GetCurrentThreadId();
    done1.Notify();
  }));
  ASSERT_TRUE(runner2.PostTask([&tid2, &done2]() {
    tid2 = api::GetCurrentThreadId();
    done2.Notify();
  }));

  ASSERT_TRUE(done1.WaitForNotificationWithTimeout(absl::Seconds(1)));
  ASSERT_TRUE(done2.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_EQ(tid1, runner1.GetThreadId());
  EXPECT_EQ(tid2, runner2.GetThreadId());
}

TEST(SingleThreadTaskRunnerImplTest, GetThreadIdConsistentAfterShutdown) {
  SingleThreadTaskRunnerImpl task_runner;
  const ThreadId tid_before = task_runner.GetThreadId();
  EXPECT_NE(tid_before, 0u);

  task_runner.Shutdown();
  EXPECT_EQ(task_runner.GetThreadId(), tid_before);
}

}  // namespace
}  // namespace nearby
