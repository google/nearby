// Copyright 2020 Google LLC
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

#include "internal/platform/implementation/apple/condition_variable.h"

#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "internal/platform/implementation/apple/mutex.h"
#include "thread/fiber/fiber.h"

namespace nearby {
namespace apple {
namespace {

TEST(ConditionVariableTest, CanCreate) {
  Mutex mutex{};
  ConditionVariable cond{&mutex};
}

TEST(ConditionVariableTest, CanWakeupWaiter) {
  Mutex mutex{};
  ConditionVariable cond{&mutex};
  bool done = false;
  bool waiting = false;
  {
    thread::Fiber f([&cond, &mutex, &done, &waiting] {
      mutex.Lock();
      waiting = true;
      cond.Wait();
      waiting = false;
      done = true;
      mutex.Unlock();
    });
    while (true) {
      {
        mutex.Lock();
        if (waiting) {
          mutex.Unlock();
          break;
        }
        mutex.Unlock();
      }
      absl::SleepFor(absl::Milliseconds(100));
    }
    {
      mutex.Lock();
      cond.Notify();
      EXPECT_FALSE(done);
      mutex.Unlock();
    }
    f.Join();
  }
  EXPECT_TRUE(done);
}

TEST(ConditionVariableTest, WaitTerminatesOnTimeoutWithoutNotify) {
  Mutex mutex{};
  ConditionVariable cond{&mutex};
  mutex.Lock();

  const absl::Duration kWaitTime = absl::Milliseconds(100);
  absl::Time start = absl::Now();
  cond.Wait(kWaitTime);
  absl::Duration duration = absl::Now() - start;
  EXPECT_GE(duration, kWaitTime);
  mutex.Unlock();
}

}  // namespace
}  // namespace apple
}  // namespace nearby
