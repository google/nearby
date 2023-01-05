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

#include "internal/platform/condition_variable.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/system_clock.h"

namespace nearby {
namespace {

TEST(ConditionVariableTest, CanCreate) {
  Mutex mutex;
  ConditionVariable cond{&mutex};
}

TEST(ConditionVariableTest, CanWakeupWaiter) {
  Mutex mutex;
  ConditionVariable cond{&mutex};
  bool done = false;
  bool waiting = false;
  NEARBY_LOG(INFO, "At start; done=%d", done);
  {
    SingleThreadExecutor executor;
    executor.Execute([&cond, &mutex, &done, &waiting]() {
      MutexLock lock(&mutex);
      NEARBY_LOG(INFO, "Before cond.Wait(); done=%d", done);
      waiting = true;
      cond.Wait();
      waiting = false;
      done = true;
      NEARBY_LOG(INFO, "After cond.Wait(); done=%d", done);
    });
    while (true) {
      {
        MutexLock lock(&mutex);
        if (waiting) break;
      }
      SystemClock::Sleep(absl::Milliseconds(100));
    }
    {
      MutexLock lock(&mutex);
      cond.Notify();
      EXPECT_FALSE(done);
    }
  }
  NEARBY_LOG(INFO, "After executor shutdown: done=%d", done);
  EXPECT_TRUE(done);
}

TEST(ConditionVariableTest, WaitTerminatesOnTimeoutWithoutNotify) {
  Mutex mutex;
  ConditionVariable cond{&mutex};
  MutexLock lock(&mutex);

  const absl::Duration kWaitTime = absl::Milliseconds(100);
  absl::Time start = SystemClock::ElapsedRealtime();
  cond.Wait(kWaitTime);
  absl::Duration duration = SystemClock::ElapsedRealtime() - start;
  EXPECT_GE(duration, kWaitTime);
}

}  // namespace
}  // namespace nearby
