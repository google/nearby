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

#include "internal/platform/scheduled_executor.h"

#include <atomic>

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "internal/platform/cancelable.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"

namespace nearby {

// kShortDelay must be significant enough to guarantee that OS under heavy load
// should be able to execute the non-blocking test paths within this time.
absl::Duration kShortDelay = absl::Milliseconds(200);

// kLongDelay must be long enough to make sure that under OS under heavy load
// will let kShortDelay fire and jobs scheduled before the kLongDelay fires.
absl::Duration kLongDelay = 10 * kShortDelay;

TEST(ScheduledExecutorTest, SimulatedClockCanSchedule) {
  MediumEnvironment::Instance().Start({.use_simulated_clock = true});
  ScheduledExecutor executor;
  std::atomic_int value = 0;
  CountDownLatch first_task_latch(1);
  CountDownLatch second_task_latch(1);
  // schedule job due in kLongDelay.
  executor.Schedule(
      [&]() {
        EXPECT_EQ(value, 1);
        value = 5;
        first_task_latch.CountDown();
      },
      kLongDelay);
  // schedule job due in kShortDelay; must fire before the first one.
  executor.Schedule(
      [&]() {
        EXPECT_EQ(value, 0);
        value = 1;
        second_task_latch.CountDown();
      },
      kShortDelay);
  EXPECT_EQ(value, 0);
  MediumEnvironment::Instance().FastForward(kShortDelay -
                                            absl::Milliseconds(1));
  EXPECT_EQ(value, 0);
  MediumEnvironment::Instance().FastForward(absl::Milliseconds(1));
  second_task_latch.Await();
  EXPECT_EQ(value, 1);
  MediumEnvironment::Instance().FastForward(kLongDelay - kShortDelay);
  first_task_latch.Await();
  EXPECT_EQ(value, 5);
  // Very long sleep to make sure that the sleep is truly simulated.
  MediumEnvironment::Instance().FastForward(absl::Minutes(30));
  MediumEnvironment::Instance().Stop();
}

TEST(ScheduledExecutorTest,
     DestroyExecutorWithSimulatedClockIgnoresPendingTasks) {
  MediumEnvironment::Instance().Start({.use_simulated_clock = true});
  {
    ScheduledExecutor executor;
    executor.Schedule(
        [&]() {
          // This task should never be executed.
          EXPECT_TRUE(false);
        },
        kShortDelay);
  }
  MediumEnvironment::Instance().FastForward(absl::Minutes(30));
  MediumEnvironment::Instance().Stop();
}

TEST(ScheduledExecutorTest, SimulatedClockCanScheduleRepeatedly) {
  MediumEnvironment::Instance().Start({.use_simulated_clock = true});
  ScheduledExecutor executor;
  std::atomic_int value = 0;
  std::atomic_int i = 0;
  CountDownLatch latch[] = {CountDownLatch(1), CountDownLatch(1)};

  Cancelable cancelable = executor.ScheduleRepeatedly(
      [&]() {
        value++;
        latch[i.fetch_add(1)].CountDown();
      },
      kShortDelay);

  EXPECT_EQ(value, 0);
  // Advance to just before the first execution.
  MediumEnvironment::Instance().FastForward(kShortDelay -
                                            absl::Milliseconds(1));
  EXPECT_EQ(value, 0);

  // Advance past the first execution.
  MediumEnvironment::Instance().FastForward(absl::Milliseconds(1));
  latch[0].Await(absl::Seconds(1));
  EXPECT_EQ(value, 1);

  // Advance to just before the second execution.
  MediumEnvironment::Instance().FastForward(kShortDelay -
                                            absl::Milliseconds(1));
  EXPECT_EQ(value, 1);

  // Advance past the second execution.
  MediumEnvironment::Instance().FastForward(absl::Milliseconds(1));
  latch[1].Await(absl::Seconds(1));
  EXPECT_EQ(value, 2);

  // Cancel the task.
  cancelable.Cancel();

  // Advance a long time and make sure it doesn't run again.
  MediumEnvironment::Instance().FastForward(kLongDelay * 5);
  EXPECT_EQ(value, 2);

  MediumEnvironment::Instance().Stop();
}

}  // namespace nearby
