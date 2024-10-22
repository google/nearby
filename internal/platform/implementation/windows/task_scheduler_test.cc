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

#include "internal/platform/implementation/windows/task_scheduler.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace nearby::windows {

namespace {
constexpr absl::Duration kTaskDuration = absl::Milliseconds(500);

TEST(TaskScheduler, ScheduleOneTask) {
  TaskScheduler task_scheduler;
  int counter = 0;
  absl::Notification notification;
  auto task = [&counter, &notification]() {
    counter++;
    notification.Notify();
  };
  auto cancelable = task_scheduler.Schedule(task, absl::Milliseconds(100));
  EXPECT_NE(cancelable, nullptr);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kTaskDuration));
  EXPECT_EQ(counter, 1);
}

TEST(TaskScheduler, ScheduleOneEarlierTaskAfterOneTask) {
  TaskScheduler task_scheduler;
  std::vector<int> result;
  absl::Notification notification1;
  absl::Notification notification2;
  auto task1 = [&result, &notification1]() {
    result.push_back(1);
    notification1.Notify();
  };

  auto task2 = [&result, &notification2]() {
    result.push_back(2);
    notification2.Notify();
  };
  task_scheduler.Schedule(task1, absl::Milliseconds(100));
  task_scheduler.Schedule(task2, absl::Milliseconds(50));
  EXPECT_TRUE(notification1.WaitForNotificationWithTimeout(kTaskDuration));
  EXPECT_TRUE(notification2.WaitForNotificationWithTimeout(kTaskDuration));
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result.at(0), 2);
  EXPECT_EQ(result.at(1), 1);
}

TEST(TaskScheduler, CancelScheduledTask) {
  TaskScheduler task_scheduler;
  int counter = 0;
  absl::Notification notification;
  auto task = [&counter, &notification]() {
    counter++;
    notification.Notify();
  };
  auto cancelable = task_scheduler.Schedule(task, absl::Milliseconds(200));
  cancelable->Cancel();
  EXPECT_FALSE(notification.WaitForNotificationWithTimeout(kTaskDuration));
  EXPECT_EQ(counter, 0);
}

TEST(TaskScheduler, ShundownShouldWaitForScheduledTaskToFinish) {
  TaskScheduler task_scheduler;
  int counter = 0;
  absl::Notification notification;
  auto task = [&counter, &notification]() {
    absl::SleepFor(kTaskDuration);
    counter++;
    notification.Notify();
  };
  auto cancelable = task_scheduler.Schedule(task, absl::Milliseconds(100));
  absl::SleepFor(absl::Milliseconds(200));
  task_scheduler.Shutdown();
  EXPECT_EQ(counter, 1);
}

TEST(TaskScheduler, CancelCancleledScheduledTask) {
  TaskScheduler task_scheduler;
  int counter = 0;
  absl::Notification notification;
  auto task = [&counter, &notification]() {
    absl::SleepFor(kTaskDuration);
    counter++;
    notification.Notify();
  };
  auto cancelable = task_scheduler.Schedule(task, absl::Milliseconds(200));
  EXPECT_TRUE(cancelable->Cancel());
  EXPECT_FALSE(cancelable->Cancel());
  task_scheduler.Shutdown();
  EXPECT_EQ(counter, 0);
}

}  // namespace
}  // namespace nearby::windows
