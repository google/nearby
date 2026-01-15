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

#include <memory>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/implementation/platform.h"

namespace nearby {
namespace linux {
namespace {

TEST(TimerTest, TestCreateTimer) {
  int count = 0;

  std::unique_ptr<nearby::api::Timer> timer =
      nearby::api::ImplementationPlatform::CreateTimer();

  ASSERT_TRUE(timer != nullptr);
  EXPECT_FALSE(timer->Create(-100, 0, [&]() { ++count; }));
  EXPECT_TRUE(timer->Stop());
}

// This test case cannot run on Google3
TEST(TimerTest, TestRepeatTimer) {
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

}  // namespace
}  // namespace linux
}  // namespace nearby
