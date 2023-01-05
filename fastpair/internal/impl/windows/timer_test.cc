// Copyright 2022 Google LLC
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

#include <chrono>      // NOLINT
#include <functional>  // NOLINT
#include <memory>
#include <thread>  // NOLINT

#include "gtest/gtest.h"
#include "fastpair/internal/api/fast_pair_platform.h"

namespace nearby {
namespace windows {
namespace {

TEST(Timer, TestCreateTimer) {
  int count = 0;
  std::function<void()> callback = [&count]() { ++count; };

  std::unique_ptr<api::Timer> timer =
      api::ImplementationFastPairPlatform::CreateTimer();

  ASSERT_TRUE(timer != nullptr);
  EXPECT_FALSE(timer->Create(-100, 0, callback));
  EXPECT_TRUE(timer->Stop());
}

// This test case cannot run on Google3
TEST(Timer, DISABLED_TestRepeatTimer) {
  int count = 0;
  std::function<void()> callback = [&count]() { count++; };

  std::unique_ptr<api::Timer> timer =
      api::ImplementationFastPairPlatform::CreateTimer();

  ASSERT_TRUE(timer != nullptr);
  EXPECT_TRUE(timer->Create(300, 300, callback));
  std::this_thread::sleep_for(std::chrono::seconds(1));
  EXPECT_TRUE(timer->Stop());
  EXPECT_EQ(count, 3);
}

TEST(Timer, DISABLED_TestFireNow) {
  int count = 0;
  std::function<void()> callback = [&count]() { ++count; };
  auto timer = api::ImplementationFastPairPlatform::CreateTimer();
  EXPECT_TRUE(timer != nullptr);
  EXPECT_TRUE(timer->Create(3000, 3000, callback));
  EXPECT_TRUE(timer->FireNow());
  EXPECT_TRUE(timer->Stop());
  EXPECT_EQ(count, 1);
}

}  // namespace
}  // namespace windows
}  // namespace nearby
