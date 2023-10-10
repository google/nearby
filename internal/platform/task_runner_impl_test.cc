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
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "internal/platform/count_down_latch.h"

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

INSTANTIATE_TEST_SUITE_P(ParameterizedTaskRunnerImplTest, TaskRunnerImplTest,
                         ::testing::ValuesIn(kNumThreads));

}  // namespace
}  // namespace nearby
