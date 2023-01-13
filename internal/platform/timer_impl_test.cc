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

namespace nearby {
namespace {

TEST(TimerImpl, TestCreateTimer) {
  TimerImpl timer;

  EXPECT_FALSE(timer.Start(-100, 0, nullptr));
  EXPECT_TRUE(timer.Start(100, 100, []() {}));
  EXPECT_TRUE(timer.Stop());
}

TEST(TimerImpl, TestRunningStatus) {
  TimerImpl timer;

  EXPECT_TRUE(timer.Start(100, 100, []() {}));
  EXPECT_TRUE(timer.IsRunning());
  EXPECT_TRUE(timer.Stop());
  EXPECT_FALSE(timer.IsRunning());
}

TEST(TimerImpl, TestStartRunningTimer) {
  TimerImpl timer;

  EXPECT_TRUE(timer.Start(100, 100, []() {}));
  EXPECT_FALSE(timer.Start(100, 100, []() {}));
  EXPECT_TRUE(timer.Stop());
}

TEST(TimerImpl, TestFireNow) {
  TimerImpl timer;
  int count = 0;

  EXPECT_TRUE(timer.Start(100, 100, [&count]() { ++count; }));
  EXPECT_TRUE(timer.FireNow());
  EXPECT_TRUE(timer.Stop());
  EXPECT_EQ(count, 1);
}

}  // namespace
}  // namespace nearby
