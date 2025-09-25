// Copyright 2021 Google LLC
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

#include "internal/platform/timer_impl.h"

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"

namespace nearby {
namespace {

TEST(TimerImpl, TestCreateTimer) {
  TimerImpl timer;

  EXPECT_TRUE(timer.Start(-100, 0, []() {}));
  timer.Stop();
  EXPECT_TRUE(timer.Start(0, -100, []() {}));
  timer.Stop();
  EXPECT_TRUE(timer.Start(100, 100, []() {}));
  timer.Stop();
}

TEST(TimerImpl, TestRunningStatus) {
  TimerImpl timer;

  EXPECT_TRUE(timer.Start(100, 100, []() {}));
  EXPECT_TRUE(timer.IsRunning());
  timer.Stop();
  EXPECT_FALSE(timer.IsRunning());
}

TEST(TimerImpl, TestStartRunningTimer) {
  TimerImpl timer;

  EXPECT_TRUE(timer.Start(100, 100, []() {}));
  EXPECT_FALSE(timer.Start(100, 100, []() {}));
  timer.Stop();
}

TEST(TimerImpl, TestStopAfterFire) {
  TimerImpl timer;
  absl::Notification notification;
  EXPECT_TRUE(timer.Start(0, 0, [&notification]() {
    notification.Notify();
  }));
  notification.WaitForNotificationWithTimeout(absl::Seconds(1));

  timer.Stop();
}

TEST(TimerImpl, TestRestartAfterFire) {
  TimerImpl timer;
  absl::Notification notification;
  EXPECT_TRUE(timer.Start(0, 0, [&notification]() {
    notification.Notify();
  }));
  notification.WaitForNotificationWithTimeout(absl::Seconds(1));
  timer.Stop();
  absl::Notification notification2;

  EXPECT_TRUE(timer.Start(100, 100, [&notification2]() {
    notification2.Notify();
  }));
  notification2.WaitForNotificationWithTimeout(absl::Seconds(1));
  timer.Stop();
}

}  // namespace
}  // namespace nearby
