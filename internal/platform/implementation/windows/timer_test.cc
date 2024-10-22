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

#include "internal/platform/implementation/timer.h"

#include <chrono>  // NOLINT
#include <memory>
#include <thread>  // NOLINT

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/platform.h"

namespace nearby {
namespace windows {
namespace {

class TimerTest : public ::testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        platform::config_package_nearby::nearby_platform_feature::
            kEnableTaskScheduler,
        GetParam());
  }

  void TearDown() override {
    NearbyFlags::GetInstance().ResetOverridedValues();
  }
};

TEST_P(TimerTest, TestCreateTimer) {
  int count = 0;

  std::unique_ptr<nearby::api::Timer> timer =
      nearby::api::ImplementationPlatform::CreateTimer();

  ASSERT_TRUE(timer != nullptr);
  EXPECT_FALSE(timer->Create(-100, 0, [&]() { ++count; }));
  EXPECT_TRUE(timer->Stop());
}

// This test case cannot run on Google3
TEST_P(TimerTest, TestRepeatTimer) {
  CountDownLatch latch(3);
  int count = 0;
  std::unique_ptr<nearby::api::Timer> timer =
      nearby::api::ImplementationPlatform::CreateTimer();

  ASSERT_TRUE(timer != nullptr);
  EXPECT_TRUE(timer->Create(300, 300, [&]() {
    ++count;
    latch.CountDown();
  }));

  EXPECT_TRUE(latch.Await(absl::Seconds(2)));
  EXPECT_EQ(count, 3);
  EXPECT_TRUE(timer->Stop());
}

TEST_P(TimerTest, TestFireNow) {
  int count = 0;
  absl::Notification notification;

  auto timer = nearby::api::ImplementationPlatform::CreateTimer();

  EXPECT_TRUE(timer != nullptr);
  EXPECT_TRUE(timer->Create(3000, 3000, [&count, &notification]() {
    ++count;
    notification.Notify();
  }));
  EXPECT_TRUE(timer->FireNow());
  EXPECT_TRUE(timer->Stop());
  EXPECT_TRUE(
      notification.WaitForNotificationWithTimeout(absl::Milliseconds(1000)));
  EXPECT_EQ(count, 1);
}

INSTANTIATE_TEST_SUITE_P(TimerTaskSchedulerFlagTest, TimerTest,
                         testing::Bool());

}  // namespace
}  // namespace windows
}  // namespace nearby
