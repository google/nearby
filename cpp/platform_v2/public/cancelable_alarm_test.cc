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

#include "platform_v2/public/cancelable_alarm.h"

#include "platform_v2/public/atomic_boolean.h"
#include "platform_v2/public/scheduled_executor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"

namespace location {
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

}  // namespace
}  // namespace nearby
}  // namespace location
