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

#include "internal/platform/implementation/windows/thread_pool.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

#include "gtest/gtest.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace nearby {
namespace windows {
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

TEST(ThreadPool, ShutdownWaitsForRunningTasks) {
  auto pool = ThreadPool::Create(1);
  std::atomic_int value = 0;
  pool->Run([&]() {
    absl::SleepFor(absl::Seconds(1));
    value += 1;
  });

  pool->ShutDown();

  EXPECT_EQ(value, 1);
}

TEST(ThreadPool, RunNoDelayedTask) {
  auto pool = ThreadPool::Create(1);
  std::atomic_int value = 0;
  std::optional<uint64_t> delayed_task_id =
      pool->Run([&]() { value += 1; }, absl::ZeroDuration());
  EXPECT_TRUE(delayed_task_id.has_value());
  absl::SleepFor(absl::Milliseconds(200));
  EXPECT_EQ(value, 1);
  pool->ShutDown();
}

TEST(ThreadPool, RunDelayedTask) {
  auto pool = ThreadPool::Create(1);
  std::atomic_int value = 0;
  std::optional<uint64_t> delayed_task_id =
      pool->Run([&]() { value += 1; }, absl::Seconds(1));
  EXPECT_TRUE(delayed_task_id.has_value());
  absl::SleepFor(absl::Milliseconds(200));
  EXPECT_EQ(value, 0);
  absl::SleepFor(absl::Milliseconds(1000));
  EXPECT_EQ(value, 1);
  pool->ShutDown();
}

TEST(ThreadPool, CancelDelayedTask) {
  auto pool = ThreadPool::Create(1);
  std::atomic_int value = 0;
  std::optional<uint64_t> delayed_task_id =
      pool->Run([&]() { value += 1; }, absl::Seconds(1));
  EXPECT_TRUE(delayed_task_id.has_value());
  absl::SleepFor(absl::Milliseconds(200));
  EXPECT_TRUE(pool->CancelDelayedTask(delayed_task_id.value()));
  absl::SleepFor(absl::Milliseconds(1000));
  EXPECT_EQ(value, 0);
  pool->ShutDown();
}

TEST(ThreadPool, CancelRunningDelayedTask) {
  auto pool = ThreadPool::Create(1);
  std::atomic_int value = 0;
  std::optional<uint64_t> delayed_task_id = pool->Run(
      [&]() {
        value += 1;
        absl::SleepFor(absl::Milliseconds(1000));
      },
      absl::Milliseconds(100));
  EXPECT_TRUE(delayed_task_id.has_value());
  absl::SleepFor(absl::Milliseconds(300));
  EXPECT_TRUE(pool->CancelDelayedTask(delayed_task_id.value()));
  EXPECT_EQ(value, 1);
  pool->ShutDown();
}

TEST(ThreadPool, CancelDelayedTaskAfterShutDown) {
  auto pool = ThreadPool::Create(1);
  std::atomic_int value = 0;
  std::optional<uint64_t> delayed_task_id =
      pool->Run([&]() { value += 1; }, absl::Seconds(1));
  absl::SleepFor(absl::Milliseconds(200));
  pool->ShutDown();
  EXPECT_FALSE(pool->CancelDelayedTask(delayed_task_id.value()));
}

TEST(ThreadPool, CancelDelayedTaskInCallback) {
  auto pool = ThreadPool::Create(1);
  std::atomic_int value = 0;
  std::optional<uint64_t> delayed_task_id;
  delayed_task_id = pool->Run(
      [&]() {
        value += 1;
        pool->CancelDelayedTask(delayed_task_id.value());
      },
      absl::Milliseconds(100));
  EXPECT_TRUE(delayed_task_id.has_value());
  absl::SleepFor(absl::Milliseconds(500));
  EXPECT_EQ(value, 1);
  pool->ShutDown();
}

TEST(ThreadPool, RunRepeatedTask) {
  auto pool = ThreadPool::Create(1);
  std::atomic_int value = 0;
  std::optional<uint64_t> delayed_task_id;
  delayed_task_id = pool->Run([&]() { value += 1; }, absl::Milliseconds(500),
                              absl::Milliseconds(500));
  EXPECT_TRUE(delayed_task_id.has_value());
  absl::SleepFor(absl::Milliseconds(1200));
  pool->CancelDelayedTask(delayed_task_id.value());
  EXPECT_EQ(value, 2);
  pool->ShutDown();
}

TEST(ThreadPool, CancelRepeatedTaskInCallback) {
  auto pool = ThreadPool::Create(1);
  std::atomic_int value = 0;
  std::optional<uint64_t> delayed_task_id;
  delayed_task_id = pool->Run(
      [&]() {
        value += 1;
        pool->CancelDelayedTask(delayed_task_id.value());
      },
      absl::Milliseconds(500), absl::Milliseconds(500));
  EXPECT_TRUE(delayed_task_id.has_value());
  absl::SleepFor(absl::Milliseconds(2000));
  EXPECT_EQ(value, 1);
  pool->ShutDown();
}

}  // namespace
}  // namespace windows
}  // namespace nearby
