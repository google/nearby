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
