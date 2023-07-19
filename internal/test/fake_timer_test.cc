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

#include "internal/test/fake_timer.h"

#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/logging.h"
#include "internal/test/fake_task_runner.h"

namespace nearby {
namespace {

TEST(FakeTimer, TestOneTimeTimer) {
  FakeClock clock;
  FakeTimer timer(&clock);
  int count = 0;
  auto callback = [&count]() { ++count; };
  timer.Start(100, 0, callback);
  EXPECT_TRUE(timer.IsRunning());
  clock.FastForward(absl::Milliseconds(1000));
  EXPECT_EQ(count, 1);
  EXPECT_TRUE(timer.Stop());
  EXPECT_FALSE(timer.IsRunning());
}

TEST(FakeTimer, TestRepeatTimer) {
  FakeClock clock;
  FakeTimer timer(&clock);
  int count = 0;
  auto callback = [&count]() { ++count; };
  timer.Start(100, 100, callback);
  EXPECT_TRUE(timer.IsRunning());
  clock.FastForward(absl::Milliseconds(1000));
  EXPECT_EQ(count, 10);
  EXPECT_TRUE(timer.Stop());
  EXPECT_FALSE(timer.IsRunning());
}

TEST(FakeTimer, TestInvalidInput) {
  FakeClock clock;
  FakeTimer timer(&clock);
  int count = 0;
  auto callback = [&count]() { ++count; };
  timer.Start(-100, 100, callback);
  EXPECT_FALSE(timer.IsRunning());
  EXPECT_TRUE(timer.Stop());
}

TEST(FakeTimer, TestStopTimerBeforeClockUpdate) {
  FakeClock clock;
  FakeTimer timer(&clock);
  int count = 0;
  auto callback = [&count]() { ++count; };
  timer.Start(100, 100, callback);
  EXPECT_TRUE(timer.IsRunning());
  EXPECT_TRUE(timer.Stop());
  EXPECT_FALSE(timer.IsRunning());
  clock.FastForward(absl::Milliseconds(1000));
  EXPECT_EQ(count, 0);
}

TEST(FakeTimer, TestUpdateMultipleTimesClockForOnetimeTimer) {
  FakeClock clock;
  FakeTimer timer(&clock);
  int count = 0;
  auto callback = [&count]() { ++count; };
  timer.Start(100, 0, callback);
  EXPECT_TRUE(timer.IsRunning());
  clock.FastForward(absl::Nanoseconds(50));
  EXPECT_EQ(count, 0);
  clock.FastForward(absl::Milliseconds(1000));
  EXPECT_EQ(count, 1);
  clock.FastForward(absl::Milliseconds(1000));
  EXPECT_EQ(count, 1);
  EXPECT_TRUE(timer.Stop());
  EXPECT_FALSE(timer.IsRunning());
}

TEST(FakeTimer, TestInstantRunTimer) {
  FakeClock clock;
  FakeTimer timer(&clock);
  int count = 0;
  auto callback = [&count]() { ++count; };
  timer.Start(0, 100, callback);
  EXPECT_TRUE(timer.IsRunning());
  clock.FastForward(absl::Milliseconds(1000));
  EXPECT_EQ(count, 11);
  EXPECT_TRUE(timer.Stop());
  EXPECT_FALSE(timer.IsRunning());
}

TEST(FakeTimer, TestTimerDestructor) {
  FakeClock clock;
  {
    FakeTimer timer(&clock);
    int count = 0;
    auto callback = [&count]() { ++count; };
    timer.Start(100, 0, callback);
    EXPECT_EQ(clock.GetObserversCount(), 1);
    EXPECT_TRUE(timer.IsRunning());
    EXPECT_EQ(count, 0);
    EXPECT_TRUE(timer.Stop());
    EXPECT_FALSE(timer.IsRunning());
  }
  EXPECT_EQ(clock.GetObserversCount(), 0);
}

TEST(FakeTimer, TestTimerFireNow) {
  FakeClock clock;
  int count = 0;
  FakeTimer timer(&clock);
  auto callback = [&count]() { ++count; };
  timer.Start(200, 100, callback);
  timer.FireNow();
  timer.Stop();
  EXPECT_EQ(count, 1);
}

TEST(FakeTimer, CloseTimerInTimerProc) {
  int count = 0;
  FakeClock clock;
  FakeTimer timer1(&clock);
  FakeTimer timer2(&clock);
  timer1.Start(100, 0, [&]() { ++count; });
  timer2.Start(50, 0, [&]() {
    ++count;
    timer1.Stop();
  });

  clock.FastForward(absl::Milliseconds(50));
  EXPECT_EQ(count, 1);
  clock.FastForward(absl::Milliseconds(50));
  EXPECT_EQ(count, 1);
}

TEST(FakeTimer, StartTimerInTimerProc) {
  int count = 0;
  FakeClock clock;
  FakeTimer timer1(&clock);
  timer1.Start(1000, 0, [&]() {
    ++count;
    timer1.Stop();
    timer1.Start(1000, 0, [&]() { ++count; });
  });
  clock.FastForward(absl::Milliseconds(1000));
  EXPECT_EQ(count, 1);
  clock.FastForward(absl::Milliseconds(1000));
  EXPECT_EQ(count, 2);
}

TEST(FakeTimer, WaitForRunningTask) {
  bool finish = false;
  FakeClock clock;
  FakeTimer timer1(&clock);
  FakeTaskRunner task_runner{&clock, 1};

  timer1.Start(50, 0, [&]() {
    absl::SleepFor(absl::Milliseconds(2000));
    finish = true;
  });

  task_runner.PostTask([&]() {
    absl::SleepFor(absl::Milliseconds(10));
    timer1.Stop();
    EXPECT_TRUE(finish);
  });

  clock.FastForward(absl::Milliseconds(50));
}

}  // namespace
}  // namespace nearby
