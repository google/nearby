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

#include "internal/test/fake_task_runner.h"

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "internal/test/fake_clock.h"

namespace nearby {
namespace {

TEST(FakeTaskRunner, PostTask) {
  FakeClock clock;
  int count = 0;
  FakeTaskRunner task_runner{&clock, 1};
  task_runner.PostTask([&count] { ++count; });
  ASSERT_TRUE(
      FakeTaskRunner::WaitForRunningTasksWithTimeout(absl::Milliseconds(100)));
  EXPECT_EQ(task_runner.GetPendingTasks().size(), 0);
  EXPECT_EQ(count, 1);
}

TEST(FakeTaskRunner, PostDelayedTask) {
  FakeClock clock;
  int count = 0;
  FakeTaskRunner task_runner{&clock, 1};
  task_runner.PostDelayedTask(absl::Seconds(10), [&count] { ++count; });
  EXPECT_EQ(task_runner.GetPendingTasks().size(), 0);
  EXPECT_EQ(task_runner.GetPendingDelayedTask().size(), 1);
  EXPECT_EQ(count, 0);
  clock.FastForward(absl::Seconds(10));
  ASSERT_TRUE(
      FakeTaskRunner::WaitForRunningTasksWithTimeout(absl::Milliseconds(100)));
  EXPECT_EQ(count, 1);
  EXPECT_EQ(task_runner.GetPendingDelayedTask().size(), 0);
}

TEST(FakeTaskRunner, PostTasksInPendingMode) {
  FakeClock clock;
  FakeTaskRunner task_runner{&clock, 1};
  task_runner.SetMode(FakeTaskRunner::Mode::kPending);
  EXPECT_EQ(task_runner.GetConcurrentCount(), 1);
  task_runner.PostTask([]() {});
  task_runner.PostTask([]() {});
  EXPECT_EQ(task_runner.GetPendingTasks().size(), 2);
  task_runner.RunNextTask();
  EXPECT_EQ(task_runner.GetPendingTasks().size(), 1);
  task_runner.RunNextTask();
  EXPECT_EQ(task_runner.GetPendingTasks().size(), 0);
}

TEST(FakeTaskRunner, RunAllPostedTasksInPendingMode) {
  FakeClock clock;
  FakeTaskRunner task_runner{&clock, 1};
  task_runner.SetMode(FakeTaskRunner::Mode::kPending);
  task_runner.PostTask([]() {});
  task_runner.PostTask([]() {});
  EXPECT_EQ(task_runner.GetPendingTasks().size(), 2);
  task_runner.RunAllPendingTasks();
  ASSERT_TRUE(
      FakeTaskRunner::WaitForRunningTasksWithTimeout(absl::Milliseconds(100)));
  EXPECT_EQ(task_runner.GetPendingTasks().size(), 0);
}

TEST(FakeTaskRunner, PostDelayedTaskInPendingMode) {
  FakeClock clock;
  bool called = false;
  FakeTaskRunner task_runner{&clock, 1};
  task_runner.SetMode(FakeTaskRunner::Mode::kPending);
  task_runner.PostDelayedTask(absl::Seconds(1), [&called]() { called = true; });
  EXPECT_EQ(task_runner.GetPendingDelayedTask().size(), 1);
  clock.FastForward(absl::Seconds(1));
  EXPECT_EQ(task_runner.GetPendingDelayedTask().size(), 0);
  EXPECT_EQ(task_runner.GetPendingTasks().size(), 1);
  task_runner.RunAllPendingTasks();
  ASSERT_TRUE(
      FakeTaskRunner::WaitForRunningTasksWithTimeout(absl::Milliseconds(100)));
  EXPECT_EQ(task_runner.GetPendingTasks().size(), 0);
  EXPECT_TRUE(called);
}

TEST(FakeTaskRunner, PostDelayedTaskInDelayedTask) {
  FakeClock clock;
  int called_count = 0;
  FakeTaskRunner task_runner{&clock, 1};
  task_runner.PostDelayedTask(
      absl::Seconds(1), [&called_count, &task_runner]() {
        ++called_count;
        task_runner.PostDelayedTask(absl::Seconds(1),
                                    [&called_count]() { ++called_count; });
      });
  clock.FastForward(absl::Seconds(1));
  ASSERT_TRUE(
      FakeTaskRunner::WaitForRunningTasksWithTimeout(absl::Milliseconds(100)));
  EXPECT_EQ(called_count, 1);
  clock.FastForward(absl::Seconds(1));
  ASSERT_TRUE(
      FakeTaskRunner::WaitForRunningTasksWithTimeout(absl::Milliseconds(100)));
  EXPECT_EQ(called_count, 2);
}

}  // namespace
}  // namespace nearby
