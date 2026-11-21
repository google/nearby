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

#include "internal/platform/multi_thread_executor.h"

#include <atomic>
#include <string>
#include <thread>  // NOLINT(build/c++11)
#include <vector>

#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace nearby {
namespace {

const int kMaxThreads = 5;

TEST(MultiThreadExecutorTest, ConstructorDestructorWorks) {
  MultiThreadExecutor executor(kMaxThreads);
}

TEST(MultiThreadExecutorTest, CanExecute) {
  absl::CondVar cond;
  std::atomic_bool done = false;
  MultiThreadExecutor executor(kMaxThreads);
  EXPECT_FALSE(executor.Execute(nullptr));
  EXPECT_FALSE(executor.Execute("empty", nullptr));
  EXPECT_TRUE(executor.Execute([&done, &cond]() {
    done = true;
    cond.SignalAll();
  }));
  absl::Mutex mutex;
  {
    absl::MutexLock lock(mutex);
    if (!done) {
      cond.WaitWithTimeout(&mutex, absl::Seconds(1));
    }
  }
  EXPECT_TRUE(done);

  absl::Notification named_done;
  EXPECT_TRUE(
      executor.Execute("named_task", [&named_done]() { named_done.Notify(); }));
  EXPECT_TRUE(named_done.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(MultiThreadExecutorTest, JobsExecuteInParallel) {
  absl::Mutex mutex;
  absl::CondVar thread_cond;
  absl::CondVar test_cond;
  MultiThreadExecutor executor(kMaxThreads);
  int count = 0;

  for (int i = 0; i < kMaxThreads; ++i) {
    executor.Execute([&]() {
      absl::MutexLock lock(mutex);
      count++;
      test_cond.Signal();
      thread_cond.Wait(&mutex);
      count--;
      test_cond.Signal();
    });
  }

  {
    absl::MutexLock lock(mutex);
    while (count < kMaxThreads) {
      if (test_cond.WaitWithTimeout(&mutex, absl::Seconds(30))) break;
    }
  }

  EXPECT_EQ(count, kMaxThreads);
  thread_cond.SignalAll();

  {
    absl::MutexLock lock(mutex);
    while (count > 0) {
      if (test_cond.WaitWithTimeout(&mutex, absl::Seconds(30))) break;
    }
  }
  EXPECT_EQ(count, 0);
}

struct ThreadCheckTestClass {
  MultiThreadExecutor executor{kMaxThreads};
  int value ABSL_GUARDED_BY(executor) = 0;

  void incValue() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor) { value++; }
};

TEST(MultiThreadExecutorTest, ThreadCheck_ExecuteRunnable) {
  ThreadCheckTestClass test_class;

  test_class.executor.Execute(
      [&test_class]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(test_class.executor) {
        test_class.incValue();
      });
}

TEST(MultiThreadExecutorTest, ShutdownWaitsForRunningTasks) {
  MultiThreadExecutor executor(kMaxThreads);
  absl::Notification started;
  std::atomic_int completed = 0;

  executor.Execute([&]() {
    started.Notify();
    absl::SleepFor(absl::Milliseconds(100));
    completed.fetch_add(1, std::memory_order_relaxed);
  });

  ASSERT_TRUE(started.WaitForNotificationWithTimeout(absl::Seconds(5)));
  executor.Shutdown();
  EXPECT_EQ(completed.load(std::memory_order_relaxed), 1);
}

TEST(MultiThreadExecutorTest, ExecuteAfterShutdownFails) {
  MultiThreadExecutor executor(kMaxThreads);
  executor.Shutdown();
  EXPECT_FALSE(executor.Execute([]() { FAIL() << "Task should not run"; }));
  EXPECT_FALSE(executor.Execute(
      "named_after_shutdown", []() { FAIL() << "Named task should not run"; }));
}

TEST(MultiThreadExecutorTest, ExecuteDuringShutdownFails) {
  absl::Notification started;
  absl::Notification calling_shutdown;
  MultiThreadExecutor executor(kMaxThreads);

  EXPECT_TRUE(executor.Execute([&]() {
    started.Notify();
    if (!calling_shutdown.WaitForNotificationWithTimeout(absl::Seconds(5))) {
      ADD_FAILURE() << "Timed out waiting for shutdown to start";
      return;
    }
    absl::SleepFor(absl::Milliseconds(200));
    EXPECT_FALSE(executor.Execute([]() { FAIL() << "Task should not run"; }));
  }));

  ASSERT_TRUE(started.WaitForNotificationWithTimeout(absl::Seconds(5)));
  calling_shutdown.Notify();
  executor.Shutdown();
}

TEST(MultiThreadExecutorTest, MultipleShutdownCallsAreIdempotent) {
  MultiThreadExecutor executor(kMaxThreads);
  std::atomic_int completed = 0;

  executor.Execute(
      [&]() { completed.fetch_add(1, std::memory_order_relaxed); });
  executor.Shutdown();
  executor.Shutdown();

  EXPECT_EQ(completed.load(std::memory_order_relaxed), 1);
}

TEST(MultiThreadExecutorTest, ConcurrentExecuteAndShutdown) {
  constexpr int kIterations = 25;
  constexpr int kProducerThreads = 4;
  constexpr int kTasksPerThread = 50;

  for (int iter = 0; iter < kIterations; ++iter) {
    MultiThreadExecutor executor(kMaxThreads);
    std::atomic_bool after_shutdown = false;
    std::atomic_bool ran_after_shutdown = false;
    std::vector<std::thread> producers;
    producers.reserve(kProducerThreads);

    for (int t = 0; t < kProducerThreads; ++t) {
      producers.emplace_back([&]() {
        for (int i = 0; i < kTasksPerThread; ++i) {
          executor.Execute([&]() {
            if (after_shutdown.load(std::memory_order_acquire)) {
              ran_after_shutdown.store(true, std::memory_order_relaxed);
            }
          });
        }
      });
    }

    executor.Shutdown();
    after_shutdown.store(true, std::memory_order_release);

    for (std::thread& producer : producers) {
      producer.join();
    }
    EXPECT_FALSE(ran_after_shutdown.load(std::memory_order_relaxed));
  }
}

}  // namespace
}  // namespace nearby
