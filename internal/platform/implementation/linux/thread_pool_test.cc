// Copyright 2022 Google LLC
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

#include <vector>

#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "internal/platform/implementation/linux/thread_pool.h"

namespace nearby {
namespace linux {
namespace {

constexpr int kTaskCount = 10;

TEST(ThreadPool, TasksInSingleThreadRunInSequence) {
  absl::BlockingCounter blocking_counter(kTaskCount);
  auto pool = ThreadPool::Create(1);
  std::vector<int> completed_tasks;
  std::vector<int> expected_tasks;

  for (int i = 0; i < kTaskCount; ++i) {
    expected_tasks.push_back(i);
    pool->Run([&, i]() {
      absl::SleepFor(absl::Milliseconds(200));
      completed_tasks.push_back(i);
      blocking_counter.DecrementCount();
    });
  }

  blocking_counter.Wait();
  EXPECT_EQ(completed_tasks, expected_tasks);
  pool->ShutDown();
}

TEST(ThreadPool, TasksInMultipleThreadsRunInParallel) {
  absl::BlockingCounter blocking_counter(kTaskCount);
  absl::Time start_time = absl::Now();

  auto pool = ThreadPool::Create(2);

  for (int i = 0; i < kTaskCount; ++i) {
    pool->Run([&]() {
      absl::SleepFor(absl::Milliseconds(200));
      blocking_counter.DecrementCount();
    });
  }

  blocking_counter.Wait();
  EXPECT_TRUE(absl::Now() - start_time < absl::Milliseconds(1500));
  pool->ShutDown();
}

}  // namespace
}  // namespace linux
}  // namespace nearby
