// Copyright 2021-2023 Google LLC
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

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/platform.h"

namespace nearby {
namespace windows {
namespace {

TEST(Timer, TestCreateTimer) {
  int count = 0;

  std::unique_ptr<nearby::api::Timer> timer =
      nearby::api::ImplementationPlatform::CreateTimer();

  ASSERT_TRUE(timer != nullptr);
  EXPECT_FALSE(timer->Create(-100, 0, [&]() { ++count; }));
  EXPECT_TRUE(timer->Stop());
}

// This test case cannot run on Google3
TEST(Timer, DISABLED_TestRepeatTimer) {
  int count = 0;

  std::unique_ptr<nearby::api::Timer> timer =
      nearby::api::ImplementationPlatform::CreateTimer();

  ASSERT_TRUE(timer != nullptr);
  EXPECT_TRUE(timer->Create(300, 300, [&]() { ++count; }));
  absl::SleepFor(absl::Seconds(1));
  EXPECT_TRUE(timer->Stop());
  EXPECT_EQ(count, 3);
}

TEST(Timer, DISABLED_TestFireNow) {
  int count = 0;

  auto timer = nearby::api::ImplementationPlatform::CreateTimer();

  EXPECT_TRUE(timer != nullptr);
  EXPECT_TRUE(timer->Create(3000, 3000, [&]() {
    absl::SleepFor(absl::Milliseconds(1000));
    ++count;
  }));
  EXPECT_TRUE(timer->FireNow());
  absl::SleepFor(absl::Milliseconds(100));
  EXPECT_TRUE(timer->Stop());
  EXPECT_EQ(count, 1);
}

TEST(Timer, DISABLED_TestWaitForRunningCallback) {
  int count = 0;

  auto timer = nearby::api::ImplementationPlatform::CreateTimer();

  EXPECT_TRUE(timer != nullptr);
  EXPECT_TRUE(timer->Create(1000, 0, [&]() {
    absl::SleepFor(absl::Milliseconds(3000));
    ++count;
  }));
  absl::SleepFor(absl::Milliseconds(1050));
  EXPECT_TRUE(timer->Stop());
  EXPECT_EQ(count, 1);
}

}  // namespace
}  // namespace windows
}  // namespace nearby
