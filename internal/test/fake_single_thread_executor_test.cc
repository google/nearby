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

#include "internal/test/fake_single_thread_executor.h"

#include "gtest/gtest.h"

namespace nearby {
namespace {

TEST(FakeSingleThreadExecutorTest, ExecuteDefersUntilRunAllExecutables) {
  FakeSingleThreadExecutor executor;
  int count = 0;
  bool on_executor_in_task = false;

  EXPECT_FALSE(executor.IsRunningOnExecutor());
  EXPECT_FALSE(executor.Execute("empty", nullptr));
  EXPECT_TRUE(executor.Execute("first", [&]() {
    on_executor_in_task = executor.IsRunningOnExecutor();
    count += 1;
  }));
  EXPECT_TRUE(executor.Execute("second", [&count]() { count *= 10; }));
  EXPECT_EQ(count, 0);
  EXPECT_FALSE(executor.IsRunningOnExecutor());

  executor.RunAllExecutables();
  EXPECT_EQ(count, 10);
  EXPECT_TRUE(on_executor_in_task);
  EXPECT_FALSE(executor.IsRunningOnExecutor());
}

TEST(FakeSingleThreadExecutorTest, ExecuteImmediatelyRunsNonEmptyTasks) {
  FakeSingleThreadExecutor executor;
  executor.SetRunExecutablesImmediately(true);
  int count = 0;

  EXPECT_FALSE(executor.Execute("empty", nullptr));
  EXPECT_TRUE(executor.Execute("immediate", [&count]() { count = 42; }));
  EXPECT_EQ(count, 42);
}

}  // namespace
}  // namespace nearby
