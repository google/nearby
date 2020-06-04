// Copyright 2020 Google LLC
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

#include "platform_v2/public/scheduled_executor.h"

#include <atomic>
#include <functional>

#include "platform_v2/base/exception.h"
#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {

TEST(ScheduledExecutorTest, ConsructorDestructorWorks) {
  ScheduledExecutor executor;
}

TEST(ScheduledExecutorTest, CanExecute) {
  absl::Mutex mutex;
  absl::CondVar cond;
  std::atomic_bool done = false;
  ScheduledExecutor executor;
  executor.Execute([&done, &cond]() {
    done = true;
    cond.SignalAll();
  });
  {
    absl::MutexLock lock(&mutex);
    if (!done) {
      cond.WaitWithTimeout(&mutex, absl::Seconds(1));
    }
  }
  EXPECT_TRUE(done);
}

TEST(ScheduledExecutorTest, CanSchedule) {
  ScheduledExecutor executor;
  std::atomic_int value = 0;
  absl::Mutex mutex;
  absl::CondVar cond;
  // schedule job due in 100 ms.
  executor.Schedule(
      [&value, &cond]() {
        EXPECT_EQ(value, 1);
        value = 5;
        cond.Signal();
      },
      absl::Milliseconds(100));
  // schedule job due in 10 ms; must fire before the first one.
  executor.Schedule(
      [&value]() {
        EXPECT_EQ(value, 0);
        value = 1;
      },
      absl::Milliseconds(10));
  {
    // wait for the final job to unblock us.
    absl::MutexLock lock(&mutex);
    cond.WaitWithTimeout(&mutex, absl::Milliseconds(1000));
  }
  EXPECT_EQ(value, 5);
}

TEST(ScheduledExecutorTest, CanCancel) {
  ScheduledExecutor executor;
  std::atomic_int value = 0;
  Cancelable cancelable =
      executor.Schedule([&value]() { value += 1; }, absl::Milliseconds(10));
  EXPECT_EQ(value, 0);
  EXPECT_TRUE(cancelable.Cancel());
  absl::SleepFor(absl::Milliseconds(500));
  EXPECT_EQ(value, 0);
}

TEST(ScheduledExecutorTest, FailToCancel) {
  absl::Mutex mutex;
  absl::CondVar cond;
  ScheduledExecutor executor;
  std::atomic_int value = 0;
  // Schedule job in 10ms, which will we will attempt to cancel later.
  Cancelable cancelable =
      executor.Schedule([&value]() { value += 1; }, absl::Milliseconds(10));
  // schedule another job to test results of the first one, in 50ms from now.
  executor.Schedule(
      [&cancelable, &cond]() {
        EXPECT_FALSE(cancelable.Cancel());
        // Wake up main thread.
        cond.Signal();
      },
      absl::Milliseconds(50));
  {
    absl::MutexLock lock(&mutex);
    cond.Wait(&mutex);
  }
  EXPECT_EQ(value, 1);
}

}  // namespace nearby
}  // namespace location
