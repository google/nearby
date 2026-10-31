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

#include "internal/platform/implementation/windows/timer.h"

#include <memory>

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/shared/count_down_latch.h"
#include "internal/platform/implementation/timer.h"

namespace nearby {
namespace windows {
namespace {

TEST(TimerTest, TestCreateTimer) {
  int count = 0;

  std::unique_ptr<nearby::api::Timer> timer = std::make_unique<Timer>();

  ASSERT_TRUE(timer != nullptr);
  EXPECT_FALSE(timer->Create(-100, 0, [&]() { ++count; }));
  EXPECT_TRUE(timer->Stop());
}

// This test case cannot run on Google3
TEST(TimerTest, TestRepeatTimer) {
  shared::CountDownLatch latch(3);
  int count = 0;
  std::unique_ptr<nearby::api::Timer> timer = std::make_unique<Timer>();

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
}  // namespace windows
}  // namespace nearby
