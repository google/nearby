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

#include <memory>

#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/scheduled_executor.h"

namespace nearby {
namespace {

TEST(CancelableAlarmTest, CanCreateDefault) { CancelableAlarm alarm; }

TEST(CancelableAlarmTest, CancelDefaultFails) {
  CancelableAlarm alarm;
  alarm.Cancel();
  EXPECT_FALSE(alarm.IsRunning());
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
  alarm.Cancel();
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
  alarm.Cancel();
  EXPECT_TRUE(done.Get());
}

TEST(CancelableAlarmTest, CancelInCallbackWithoutDeadlock) {
  ScheduledExecutor alarm_executor;
  AtomicBoolean done{false};
  std::unique_ptr<CancelableAlarm> alarm;
  alarm = std::make_unique<CancelableAlarm>(
      "test_alarm",
      [&done, &alarm]() {
        done.Set(true);
        alarm->Cancel();
      },
      absl::Milliseconds(100), &alarm_executor);
  SystemClock::Sleep(absl::Milliseconds(1000));
  EXPECT_TRUE(done.Get());
}

TEST(CancelableAlarmTest, CancelWithMutex) {
  ScheduledExecutor alarm_executor;
  AtomicBoolean done{false};
  std::unique_ptr<CancelableAlarm> alarm;
  absl::Mutex mutex;
  {
    absl::MutexLock lock(&mutex);
    alarm = std::make_unique<CancelableAlarm>(
        "test_alarm",
        [&done, &mutex]() {
          absl::MutexLock lock(&mutex);
          done.Set(true);
        },
        absl::Milliseconds(300), &alarm_executor);
  }

  SystemClock::Sleep(absl::Milliseconds(100));
  {
    absl::MutexLock lock(&mutex);
    SystemClock::Sleep(absl::Milliseconds(500));
    alarm->Cancel();
  }
  SystemClock::Sleep(absl::Milliseconds(200));
  EXPECT_TRUE(done.Get());
}

}  // namespace
}  // namespace nearby
