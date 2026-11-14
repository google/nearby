// Copyright 2026 Google LLC
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

#include "internal/test/test_single_thread_executor.h"

#include <atomic>
#include <vector>

#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/test/fake_clock.h"

namespace nearby {
namespace {

TEST(TestSingleThreadExecutorTest, Execute) {
  FakeClock clock;
  TestSingleThreadExecutor executor(&clock);
  std::atomic<int> count = 0;

  executor.Execute([&count] { ++count; });
  executor.Sync();

  EXPECT_EQ(count, 1);
}

TEST(TestSingleThreadExecutorTest, DelayedExecuteUsesFakeClock) {
  FakeClock clock;
  TestSingleThreadExecutor executor(&clock);
  std::atomic<int> count = 0;

  EXPECT_TRUE(
      executor.DelayedExecute(absl::Seconds(10), [&count] { ++count; }));
  executor.Sync();
  EXPECT_EQ(count, 0);

  clock.FastForward(absl::Seconds(10));
  EXPECT_TRUE(executor.SyncWithTimeout(absl::Seconds(1)));
  EXPECT_EQ(count, 1);
}

TEST(TestSingleThreadExecutorTest, TasksRunInSequence) {
  FakeClock clock;
  TestSingleThreadExecutor executor(&clock);
  std::vector<int> results;

  executor.Execute([&results] {
    absl::SleepFor(absl::Milliseconds(20));
    results.push_back(1);
  });
  executor.Execute([&results] { results.push_back(2); });
  executor.Sync();

  ASSERT_EQ(results.size(), 2);
  EXPECT_EQ(results[0], 1);
  EXPECT_EQ(results[1], 2);
}

TEST(TestSingleThreadExecutorTest, DelayedExecuteInsideDelayedExecute) {
  FakeClock clock;
  TestSingleThreadExecutor executor(&clock);
  std::atomic<int> count = 0;

  EXPECT_TRUE(
      executor.DelayedExecute(absl::Seconds(2), [&count, &executor] {
        ++count;
        executor.DelayedExecute(absl::Seconds(2), [&count] { ++count; });
      }));
  clock.FastForward(absl::Seconds(2));
  executor.Sync();
  EXPECT_EQ(count, 1);

  clock.FastForward(absl::Seconds(2));
  executor.Sync();
  EXPECT_EQ(count, 2);
}

}  // namespace
}  // namespace nearby
