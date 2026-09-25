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

#include "internal/platform/cancelable_alarm.h"

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/atomic_reference.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/scheduled_executor.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace {

TEST(CancelableAlarmTest, CanCreateDefault) { CancelableAlarm alarm; }

TEST(CancelableAlarmTest, CancelDefaultFails) {
  CancelableAlarm alarm;
  EXPECT_FALSE(alarm.Cancel());
}

TEST(CancelableAlarmTest, CanCreateAndFireAlarm) {
  ScheduledExecutor alarm_executor;
  AtomicBoolean done{false};
  CancelableAlarm alarm(
      "test_alarm", [&done]() { done.Set(true); }, absl::Milliseconds(100),
      &alarm_executor);
  SystemClock::Sleep(absl::Milliseconds(1000));
  EXPECT_TRUE(done.Get());
}

TEST(CancelableAlarmTest, CanCreateAndCancelAlarm) {
  ScheduledExecutor alarm_executor;
  AtomicBoolean done{false};
  CancelableAlarm alarm(
      "test_alarm", [&done]() { done.Set(true); }, absl::Milliseconds(100),
      &alarm_executor);
  EXPECT_TRUE(alarm.Cancel());
  SystemClock::Sleep(absl::Milliseconds(1000));
  EXPECT_FALSE(done.Get());
}

TEST(CancelableAlarmTest, CancelExpiredAlarmFails) {
  ScheduledExecutor alarm_executor;
  AtomicBoolean done{false};
  CancelableAlarm alarm(
      "test_alarm", [&done]() { done.Set(true); }, absl::Milliseconds(100),
      &alarm_executor);
  SystemClock::Sleep(absl::Milliseconds(1000));
  EXPECT_TRUE(done.Get());
  EXPECT_FALSE(alarm.Cancel());
}

TEST(CancelableAlarmTest, CanCreateRecurringAlarm) {
  ScheduledExecutor alarm_executor;
  AtomicReference<int> count(0);
  CancelableAlarm alarm(
      "test_alarm", [&count]() { count.Set(count.Get() + 1); },
      absl::Milliseconds(1000), &alarm_executor, /*is_recurring=*/true);
  // Wait for 2 rounds (>1000ms * 2) and expect the `count` = 2.
  SystemClock::Sleep(absl::Milliseconds(2800));
  alarm.Cancel();
  EXPECT_EQ(count.Get(), 2);
}

TEST(CancelableAlarmTest, DestructorCancelsAlarm) {
  ScheduledExecutor alarm_executor;
  AtomicBoolean executed{false};
  {
    CancelableAlarm alarm(
        "destructor_test", [&executed]() { executed.Set(true); },
        absl::Milliseconds(100), &alarm_executor);
  }
  SystemClock::Sleep(absl::Milliseconds(500));
  EXPECT_FALSE(executed.Get());
}

TEST(CancelableAlarmTest, IsValidReflectsCancelledState) {
  ScheduledExecutor alarm_executor;
  CancelableAlarm alarm(
      "is_valid_test", []() {}, absl::Seconds(10), &alarm_executor);
  EXPECT_TRUE(alarm.IsValid());
  EXPECT_TRUE(alarm.Cancel());
  EXPECT_FALSE(alarm.IsValid());
}

TEST(CancelableAlarmTest, CancelDuringExecutionDoesNotRescheduleOrDeadlock) {
  ScheduledExecutor alarm_executor;
  SingleThreadExecutor finish_executor;
  AtomicReference<int> run_count(0);
  absl::Notification callback_started;
  absl::Notification allow_callback_finish;

  CancelableAlarm alarm(
      "recurring_cancel_test",
      [&]() {
        run_count.Set(run_count.Get() + 1);
        if (!callback_started.HasBeenNotified()) {
          callback_started.Notify();
        }
        allow_callback_finish.WaitForNotification();
      },
      absl::Milliseconds(50), &alarm_executor, /*is_recurring=*/true);

  // Wait until the callback starts running on the executor thread.
  ASSERT_TRUE(
      callback_started.WaitForNotificationWithTimeout(absl::Seconds(3)));

  // Asynchronously allow the callback to finish so Cancel() can unblock.
  finish_executor.Execute([&]() {
    SystemClock::Sleep(absl::Milliseconds(100));
    allow_callback_finish.Notify();
  });

  // Cancel while the callback is currently executing in the background.
  // Because CancelAndWaitIfStarted() waits for running tasks, Cancel() will
  // wait for allow_callback_finish, then complete without deadlocking with
  // Schedule().
  // Cancel() returns false because the current invocation was already
  // dispatched, but it marks the alarm cancelled so it won't reschedule.
  alarm.Cancel();
  EXPECT_FALSE(alarm.IsValid());

  // Wait a while to ensure it was NOT rescheduled by Schedule().
  SystemClock::Sleep(absl::Milliseconds(300));
  EXPECT_EQ(run_count.Get(), 1);
}

}  // namespace
}  // namespace nearby
