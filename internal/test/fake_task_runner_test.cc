// Copyright 2022-2023 Google LLC
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

#include <list>

#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/test/fake_clock.h"

namespace nearby {
namespace {

TEST(FakeTaskRunner, PostTask) {
  FakeTaskRunner::ResetPendingTasksCount();
  FakeClock clock;
  int count = 0;
  FakeTaskRunner task_runner{&clock, 1};
  task_runner.PostTask([&count] { ++count; });
  ASSERT_TRUE(
      FakeTaskRunner::WaitForRunningTasksWithTimeout(absl::Milliseconds(100)));
  EXPECT_EQ(count, 1);
}

TEST(FakeTaskRunner, PostDelayedTask) {
  FakeTaskRunner::ResetPendingTasksCount();
  FakeClock clock;
  int count = 0;
  FakeTaskRunner task_runner{&clock, 1};
  task_runner.PostDelayedTask(absl::Seconds(10), [&count] { ++count; });
  EXPECT_EQ(count, 0);
  clock.FastForward(absl::Seconds(10));
  ASSERT_TRUE(
      FakeTaskRunner::WaitForRunningTasksWithTimeout(absl::Milliseconds(100)));
  EXPECT_EQ(count, 1);
}

TEST(FakeTaskRunner, PostTasksRunInSequence) {
  std::list<int> result;
  absl::Mutex mutex;
  FakeClock clock;
  FakeTaskRunner task_runner{&clock, 1};
  for (int i = 0; i < 100; ++i) {
    task_runner.PostTask([&, i]() {
      absl::MutexLock lock(&mutex);
      absl::SleepFor(absl::Milliseconds(40));
      result.push_back(i);
    });
  }
  task_runner.Sync();
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(result.front(), i);
    result.pop_front();
  }
}

TEST(FakeTaskRunner, PostDelayedTaskInDelayedTask) {
  FakeTaskRunner::ResetPendingTasksCount();
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
