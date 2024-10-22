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

#include <cstdint>

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/system_clock.h"

namespace nearby {
namespace {
constexpr absl::Duration kWaitTime = absl::Milliseconds(500);

TEST(ConditionVariableTest, CanCreate) {
  Mutex mutex;
  ConditionVariable cond{&mutex};
}

TEST(ConditionVariableTest, CanWakeupWaiter) {
  Mutex mutex;
  ConditionVariable cond{&mutex};
  bool done = false;
  bool waiting = false;
  NEARBY_LOGS(INFO) << "At start; done=" << done;
  {
    SingleThreadExecutor executor;
    executor.Execute([&cond, &mutex, &done, &waiting]() {
      MutexLock lock(&mutex);
      NEARBY_LOGS(INFO) << "Before cond.Wait(); done=" << done;
      waiting = true;
      cond.Wait();
      waiting = false;
      done = true;
      NEARBY_LOGS(INFO) << "After cond.Wait(); done=" << done;
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
  NEARBY_LOGS(INFO) << "After executor shutdown: done=" << done;
  EXPECT_TRUE(done);
}

TEST(ConditionVariableTest, WaitTerminatesOnTimeoutWithoutNotify) {
  Mutex mutex;
  ConditionVariable cond{&mutex};
  MutexLock lock(&mutex);

  absl::Time start = SystemClock::ElapsedRealtime();
  cond.Wait(kWaitTime);
  int64_t bias = absl::ToInt64Milliseconds(SystemClock::ElapsedRealtime() -
                                           start - kWaitTime);

  // Windows cannot guarantee the exact time of the timeout.
  EXPECT_GE(bias, -100);
}

}  // namespace
}  // namespace nearby
