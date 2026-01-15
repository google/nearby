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

#include "internal/platform/implementation/linux/thread_pool.h"

#include <atomic>
#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace nearby {
namespace linux {
namespace {

constexpr int kTaskCount = 10;

TEST(ThreadPool, TasksInSingleThreadRunInSequence) {
  absl::BlockingCounter blocking_counter(kTaskCount);
  absl::Mutex mutex;
  auto pool = std::make_unique<ThreadPool>(1);
  std::vector<int> completed_tasks;
  std::vector<int> expected_tasks;

  for (int i = 0; i < kTaskCount; ++i) {
    expected_tasks.push_back(i);
    pool->Run([&, i]() {
      absl::MutexLock lock(&mutex);
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

  auto pool = std::make_unique<ThreadPool>(2);

  for (int i = 0; i < kTaskCount; ++i) {
    pool->Run([&]() {
      absl::SleepFor(absl::Milliseconds(100));
      blocking_counter.DecrementCount();
    });
  }

  blocking_counter.Wait();
  EXPECT_TRUE(absl::Now() - start_time < absl::Milliseconds(1000));
  pool->ShutDown();
}

  // Test is flaky
TEST(ThreadPool, ShutdownWaitsForRunningTasks) {
  auto pool = std::make_unique<ThreadPool>(1);
  std::atomic_int value = 0;
  std::atomic_bool task_started_ = false;
  pool->Run([&]() {
    task_started_ = true;
    absl::SleepFor(absl::Milliseconds(1000));
    value += 1;
  });

  while (!task_started_)
  {
    absl::SleepFor(absl::Milliseconds(100));
  }

  pool->ShutDown();
  EXPECT_EQ(value, 1);
}

TEST(ThreadPool, RunNullTaskReturnsFalse) {
  auto pool = std::make_unique<ThreadPool>(1);
  EXPECT_FALSE(pool->Run(nullptr));
  pool->ShutDown();
}

TEST(ThreadPool, RunTaskAfterShutdownReturnsFalse) {
  auto pool = std::make_unique<ThreadPool>(1);
  pool->ShutDown();
  std::atomic_int value = 0;
  EXPECT_FALSE(pool->Run([&]() { value += 1; }));
  EXPECT_EQ(value, 0);
}

TEST(ThreadPool, MultipleTasksExecute) {
  absl::BlockingCounter blocking_counter(5);
  auto pool = std::make_unique<ThreadPool>(2);
  std::atomic_int counter = 0;

  for (int i = 0; i < 5; ++i) {
    pool->Run([&]() {
      counter++;
      blocking_counter.DecrementCount();
    });
  }

  blocking_counter.Wait();
  EXPECT_EQ(counter, 5);
  pool->ShutDown();
}

TEST(ThreadPool, LargeThreadPoolExecutesTasks) {
  constexpr int kLargeTaskCount = 100;
  absl::BlockingCounter blocking_counter(kLargeTaskCount);
  auto pool = std::make_unique<ThreadPool>(10);
  std::atomic_int counter = 0;

  for (int i = 0; i < kLargeTaskCount; ++i) {
    pool->Run([&]() {
      counter++;
      blocking_counter.DecrementCount();
    });
  }

  blocking_counter.Wait();
  EXPECT_EQ(counter, kLargeTaskCount);
  pool->ShutDown();
}

TEST(ThreadPool, DestructorCallsShutdown) {
  std::atomic_int value = 0;
  std::atomic_bool task_started_ = false;
  {
    auto pool = std::make_unique<ThreadPool>(1);
    pool->Run([&]() {
      task_started_ = true;
      absl::SleepFor(absl::Milliseconds(200));
      value = 1;
    });
    while (!task_started_){ absl::SleepFor(absl::Milliseconds(50)); }
  }
  EXPECT_EQ(value, 1);
}

}  // namespace
}  // namespace linux
}  // namespace nearby
