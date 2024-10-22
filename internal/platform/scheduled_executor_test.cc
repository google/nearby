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

#include "internal/platform/scheduled_executor.h"

#include <atomic>

#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/cancelable.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/medium_environment.h"
#include "internal/test/fake_clock.h"

namespace nearby {

class ScheduledExecutorTest : public ::testing::Test {
 public:
  void SetUp() override {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        platform::config_package_nearby::nearby_platform_feature::
            kEnableTaskScheduler,
        true);
  }

  void TearDown() override {
    NearbyFlags::GetInstance().ResetOverridedValues();
  }
};

// kShortDelay must be significant enough to guarantee that OS under heavy load
// should be able to execute the non-blocking test paths within this time.
absl::Duration kShortDelay = absl::Milliseconds(100);

// kLongDelay must be long enough to make sure that under OS under heavy load
// will let kShortDelay fire and jobs scheduled before the kLongDelay fires.
absl::Duration kLongDelay = 10 * kShortDelay;

TEST_F(ScheduledExecutorTest, ConsructorDestructorWorks) {
  ScheduledExecutor executor;
}

TEST_F(ScheduledExecutorTest, CanExecute) {
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
      cond.WaitWithTimeout(&mutex, kLongDelay);
    }
  }
  EXPECT_TRUE(done);
}

TEST_F(ScheduledExecutorTest, CanSchedule) {
  ScheduledExecutor executor;
  std::atomic_int value = 0;
  absl::Mutex mutex;
  absl::CondVar cond;
  // schedule job due in kLongDelay.
  executor.Schedule(
      [&value, &cond]() {
        EXPECT_EQ(value, 1);
        value = 5;
        cond.Signal();
      },
      kLongDelay);
  // schedule job due in kShortDelay; must fire before the first one.
  executor.Schedule(
      [&value]() {
        EXPECT_EQ(value, 0);
        value = 1;
      },
      kShortDelay);
  {
    // wait for the final job to unblock us; wait longer than kLongDelay.
    absl::MutexLock lock(&mutex);
    cond.WaitWithTimeout(&mutex, 2 * kLongDelay);
  }
  EXPECT_EQ(value, 5);
}

TEST_F(ScheduledExecutorTest, CanCancel) {
  ScheduledExecutor executor;
  std::atomic_int value = 0;
  Cancelable cancelable =
      executor.Schedule([&value]() { value += 1; }, kShortDelay);
  EXPECT_EQ(value, 0);
  EXPECT_TRUE(cancelable.Cancel());
  absl::SleepFor(kLongDelay);
  EXPECT_EQ(value, 0);
}

TEST_F(ScheduledExecutorTest, CanCancelTwice) {
  ScheduledExecutor executor;
  std::atomic_int value = 0;
  Cancelable cancelable =
      executor.Schedule([&value]() { value += 1; }, kShortDelay);
  EXPECT_EQ(value, 0);

  cancelable.Cancel();
  cancelable.Cancel();

  absl::SleepFor(kLongDelay);
  EXPECT_EQ(value, 0);
}

TEST_F(ScheduledExecutorTest, FailToCancel) {
  absl::Mutex mutex;
  absl::CondVar cond;
  ScheduledExecutor executor;
  std::atomic_int value = 0;
  // Schedule job in kShortDelay, which will we will attempt to cancel later.
  Cancelable cancelable =
      executor.Schedule([&value]() { value += 1; }, kShortDelay);
  // schedule another job to test results of the first one, in kLongDelay.
  executor.Schedule(
      [&cancelable, &cond]() {
        EXPECT_FALSE(cancelable.Cancel());
        // Wake up main thread.
        cond.Signal();
      },
      kLongDelay);
  {
    absl::MutexLock lock(&mutex);
    cond.Wait(&mutex);
  }
  EXPECT_EQ(value, 1);
}

TEST_F(ScheduledExecutorTest,
       CancelWhileRunning_TaskCompletesBeforeCancelReturns) {
  CountDownLatch start_latch(1);
  ScheduledExecutor executor;
  std::atomic_int value = 0;
  // A task that takes a little bit of time to complete
  Cancelable cancelable = executor.Schedule(
      [&start_latch, &value]() {
        start_latch.CountDown();
        absl::SleepFor(kLongDelay);
        value += 1;
      },
      absl::ZeroDuration());

  start_latch.Await();
  cancelable.Cancel();

  EXPECT_EQ(value, 1);
}

TEST_F(ScheduledExecutorTest,
       CancelTwiceWhileRunning_TaskCompletesBeforeCancelReturns) {
  CountDownLatch start_latch(1);
  ScheduledExecutor executor;
  std::atomic_int value = 0;
  // A task that takes a little bit of time to complete
  Cancelable cancelable = executor.Schedule(
      [&start_latch, &value]() {
        start_latch.CountDown();
        absl::SleepFor(kLongDelay);
        value += 1;
      },
      absl::ZeroDuration());

  start_latch.Await();

  cancelable.Cancel();
  cancelable.Cancel();

  EXPECT_EQ(value, 1);
}

TEST_F(ScheduledExecutorTest, ShutdownWaitsForRunningTasks) {
  ScheduledExecutor executor;
  std::atomic_int value = 0;
  executor.Execute([&]() {
    absl::SleepFor(kLongDelay);
    value += 1;
  });

  executor.Shutdown();

  EXPECT_EQ(value, 1);
}

TEST_F(ScheduledExecutorTest, ExecuteAfterShutdownFails) {
  ScheduledExecutor executor;

  executor.Shutdown();
  executor.Execute([&]() { FAIL() << "Task should not run"; });
}

TEST_F(ScheduledExecutorTest, ExecuteDuringShutdownFails) {
  CountDownLatch latch(1);
  ScheduledExecutor executor;

  executor.Execute([&]() {
    latch.CountDown();
    absl::SleepFor(kLongDelay);
    executor.Execute([&]() { FAIL() << "Task should not run"; });
  });
  latch.Await();
  executor.Shutdown();
}

TEST_F(ScheduledExecutorTest, SimulatedClockCanSchedule) {
  MediumEnvironment::Instance().Start({.use_simulated_clock = true});
  FakeClock* fake_clock =
      MediumEnvironment::Instance().GetSimulatedClock().value();
  ScheduledExecutor executor;
  std::atomic_int value = 0;
  CountDownLatch first_task_latch(1);
  CountDownLatch second_task_latch(1);
  // schedule job due in kLongDelay.
  executor.Schedule(
      [&]() {
        EXPECT_EQ(value, 1);
        value = 5;
        first_task_latch.CountDown();
      },
      kLongDelay);
  // schedule job due in kShortDelay; must fire before the first one.
  executor.Schedule(
      [&]() {
        EXPECT_EQ(value, 0);
        value = 1;
        second_task_latch.CountDown();
      },
      kShortDelay);
  EXPECT_EQ(value, 0);
  fake_clock->FastForward(kShortDelay - absl::Milliseconds(1));
  EXPECT_EQ(value, 0);
  fake_clock->FastForward(absl::Milliseconds(1));
  second_task_latch.Await();
  EXPECT_EQ(value, 1);
  fake_clock->FastForward(kLongDelay - kShortDelay);
  first_task_latch.Await();
  EXPECT_EQ(value, 5);
  // Very long sleep to make sure that the sleep is truly simulated.
  fake_clock->FastForward(absl::Minutes(30));
  MediumEnvironment::Instance().Stop();
}

TEST_F(ScheduledExecutorTest,
       DestroyExecutorWithSimulatedClockIgnoresPendingTasks) {
  MediumEnvironment::Instance().Start({.use_simulated_clock = true});
  FakeClock* fake_clock =
      MediumEnvironment::Instance().GetSimulatedClock().value();
  {
    ScheduledExecutor executor;
    executor.Schedule(
        [&]() {
          // This task should never be executed.
          EXPECT_TRUE(false);
        },
        kShortDelay);
  }
  fake_clock->FastForward(absl::Minutes(30));
  MediumEnvironment::Instance().Stop();
}

struct ScheduledThreadCheckTestClass {
  ScheduledExecutor executor;
  int value ABSL_GUARDED_BY(executor) = 0;

  void incValue() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor) { value++; }
  int getValue() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor) { return value; }
};

TEST_F(ScheduledExecutorTest, ThreadCheck_Execute) {
  ScheduledThreadCheckTestClass test_class;
  absl::Notification notification;

  test_class.executor.Execute(
      [&test_class, &notification]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(test_class.executor) {
            test_class.incValue();
            notification.Notify();
          });
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(absl::Seconds(2)));
}

TEST_F(ScheduledExecutorTest, ThreadCheck_Schedule) {
  ScheduledThreadCheckTestClass test_class;
  absl::Notification notification;

  test_class.executor.Schedule(
      [&test_class, &notification]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(test_class.executor) {
            test_class.incValue();
            notification.Notify();
          },
      absl::ZeroDuration());
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(absl::Seconds(2)));
}

}  // namespace nearby
