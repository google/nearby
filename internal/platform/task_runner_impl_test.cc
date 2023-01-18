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

namespace nearby {
namespace {

TEST(TaskRunnerImpl, PostTask) {
  TaskRunnerImpl task_runner{1};
  absl::Notification notification;
  bool called = false;

  task_runner.PostTask([&called, &notification]() {
    called = true;
    notification.Notify();
  });
  notification.WaitForNotificationWithTimeout(absl::Milliseconds(100));
  EXPECT_TRUE(called);
}

TEST(TaskRunnerImpl, PostSequenceTasks) {
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

TEST(TaskRunnerImpl, DISABLED_PostDelayedTask) {
  TaskRunnerImpl task_runner{1};
  std::vector<std::string> completed_tasks;
  absl::Notification notification;

  // Run the first task
  task_runner.PostDelayedTask(absl::Milliseconds(50),
                              [&completed_tasks, &notification]() {
                                completed_tasks.push_back("task1");
                                if (completed_tasks.size() == 2) {
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
  EXPECT_EQ(completed_tasks[0], "task2");
  EXPECT_EQ(completed_tasks[1], "task1");
}

TEST(TaskRunnerImpl, DISABLED_PostTwoDelayedTask) {
  TaskRunnerImpl task_runner{1};
  std::vector<std::string> completed_tasks;
  absl::Notification notification;

  // Run the first task
  task_runner.PostDelayedTask(absl::Milliseconds(100),
                              [&completed_tasks, &notification]() {
                                completed_tasks.push_back("task1");
                                if (completed_tasks.size() == 2) {
                                  notification.Notify();
                                }
                              });

  // Run the second task
  task_runner.PostDelayedTask(absl::Milliseconds(50),
                              [&completed_tasks, &notification]() {
                                completed_tasks.push_back("task2");
                                if (completed_tasks.size() == 2) {
                                  notification.Notify();
                                }
                              });

  notification.WaitForNotificationWithTimeout(absl::Milliseconds(150));
  ASSERT_EQ(completed_tasks.size(), 2u);
  EXPECT_EQ(completed_tasks[0], "task2");
  EXPECT_EQ(completed_tasks[1], "task1");

  absl::Notification notification2;
  task_runner.PostDelayedTask(absl::Milliseconds(100),
                              [&completed_tasks, &notification2]() {
                                completed_tasks.push_back("task3");
                                notification2.Notify();
                              });
  notification2.WaitForNotificationWithTimeout(absl::Milliseconds(150));
  ASSERT_EQ(completed_tasks.size(), 3u);
  EXPECT_EQ(completed_tasks[2], "task3");
}

TEST(TaskRunnerImpl, PostTasksOnRunnerWithMultipleThreads) {
  TaskRunnerImpl task_runner{10};
  std::atomic_int count = 0;
  absl::Notification notification;

  for (int i = 0; i < 10; i++) {
    task_runner.PostTask([&count, &notification]() {
      absl::SleepFor(absl::Milliseconds(100));
      count++;
      if (count == 10) {
        notification.Notify();
      }
    });
  }

  notification.WaitForNotificationWithTimeout(absl::Milliseconds(190));
  EXPECT_EQ(count, 10);
}

TEST(TaskRunnerImpl, PostEmptyTask) {
  TaskRunnerImpl task_runner{1};
  EXPECT_TRUE(task_runner.PostTask(nullptr));
  EXPECT_TRUE(task_runner.PostDelayedTask(absl::Milliseconds(100), nullptr));
}

}  // namespace
}  // namespace nearby
