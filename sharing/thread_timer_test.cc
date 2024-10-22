// Copyright 2024 Google LLC
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

#include "sharing/thread_timer.h"

#include <memory>
#include <utility>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_task_runner.h"

namespace nearby::sharing {
namespace {

class ThreadTimerTest : public ::testing::Test {
 public:
  ThreadTimerTest() : task_runner_(&fake_clock_, 1) {}

  void Sync(absl::Duration timeout) {
    absl::Notification notification;
    task_runner_.PostTask([&notification]() { notification.Notify(); });
    notification.WaitForNotificationWithTimeout(timeout);
  }

 protected:
  FakeClock fake_clock_;
  FakeTaskRunner task_runner_;
};

TEST_F(ThreadTimerTest, TimerFires) {
  bool fired = false;
  ThreadTimer timer(task_runner_, "test", absl::Milliseconds(100),
                    [&fired]() { fired = true; });
  EXPECT_TRUE(timer.IsRunning());
  fake_clock_.FastForward(absl::Milliseconds(100));
  Sync(absl::Milliseconds(100));
  EXPECT_FALSE(timer.IsRunning());
  EXPECT_TRUE(fired);
}

TEST_F(ThreadTimerTest, CancelBeforeFires) {
  bool fired = false;
  ThreadTimer timer(task_runner_, "test", absl::Milliseconds(100),
                    [&fired]() { fired = true; });
  EXPECT_TRUE(timer.IsRunning());
  timer.Cancel();
  EXPECT_FALSE(timer.IsRunning());
  fake_clock_.FastForward(absl::Milliseconds(100));
  Sync(absl::Milliseconds(100));
  EXPECT_FALSE(fired);
}

TEST_F(ThreadTimerTest, DeleteBeforeFires) {
  bool fired = false;
  auto timer = std::make_unique<ThreadTimer>(task_runner_, "test",
                                             absl::Milliseconds(100),
                                             [&fired]() { fired = true; });
  timer.reset();
  fake_clock_.FastForward(absl::Milliseconds(100));
  Sync(absl::Milliseconds(100));
  EXPECT_FALSE(fired);
}

TEST_F(ThreadTimerTest, CancelInCallback) {
  bool fired = false;
  ThreadTimer timer(task_runner_, "test", absl::Milliseconds(100),
                    [&fired, &timer]() {
                      fired = true;
                      timer.Cancel();
                    });
  fake_clock_.FastForward(absl::Milliseconds(100));
  Sync(absl::Milliseconds(100));
  EXPECT_TRUE(fired);
}

TEST_F(ThreadTimerTest, DeleteInCallback) {
  bool fired = false;
  std::unique_ptr<ThreadTimer> timer;
  timer = std::make_unique<ThreadTimer>(
      task_runner_, "test", absl::Milliseconds(100),
      [&fired, timer = std::move(timer)]() mutable {
        fired = true;
        timer.reset();
      });
  fake_clock_.FastForward(absl::Milliseconds(100));
  Sync(absl::Milliseconds(100));
  EXPECT_TRUE(fired);
}

}  // namespace
}  // namespace nearby::sharing
