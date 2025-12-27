// filepath: /workspace/internal/platform/implementation/linux/multi_thread_executor_test.cc
// Copyright 2025 Google LLC
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

#include "internal/platform/implementation/linux/multi_thread_executor.h"

#include <atomic>

#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace nearby {
namespace linux {

namespace {
const int kMaxThreads = 4;
}

TEST(LinuxMultiThreadExecutorTest, ConstructorDestructorWorks) {
  MultiThreadExecutor executor(kMaxThreads);
}

TEST(LinuxMultiThreadExecutorTest, CanExecute) {
  absl::CondVar cond;
  std::atomic_bool done = false;
  MultiThreadExecutor executor(kMaxThreads);
  executor.Execute([&done, &cond]() {
    done = true;
    cond.SignalAll();
  });
  absl::Mutex mutex;
  {
    absl::MutexLock lock(&mutex);
    if (!done) {
      cond.WaitWithTimeout(&mutex, absl::Seconds(1));
    }
  }
  EXPECT_TRUE(done);
}

TEST(LinuxMultiThreadExecutorTest, JobsExecuteInParallel) {
  absl::Mutex mutex;
  absl::CondVar thread_cond;
  absl::CondVar test_cond;
  MultiThreadExecutor executor(kMaxThreads);
  int count = 0;

  for (int i = 0; i < kMaxThreads; ++i) {
    executor.Execute([&]() {
      absl::MutexLock lock(&mutex);
      count++;
      test_cond.Signal();
      thread_cond.Wait(&mutex);
      count--;
      test_cond.Signal();
    });
  }

  {
    absl::MutexLock lock(&mutex);
    while (count < kMaxThreads) {
      if (test_cond.WaitWithTimeout(&mutex, absl::Seconds(30))) break;
    }
  }

  EXPECT_EQ(count, kMaxThreads);
  thread_cond.SignalAll();

  {
    absl::MutexLock lock(&mutex);
    while (count > 0) {
      if (test_cond.WaitWithTimeout(&mutex, absl::Seconds(30))) break;
    }
  }
  EXPECT_EQ(count, 0);
}

TEST(LinuxMultiThreadExecutorTest, CanScheduleDelayedTask) {
  MultiThreadExecutor executor(kMaxThreads);
  std::atomic_bool ran = false;
  auto start = absl::Now();
  executor.Schedule([&ran]() { ran = true; }, absl::Milliseconds(100));
  // Busy-wait using absl sleep to allow scheduled task to run
  for (int i = 0; i < 20 && !ran; ++i) {
    absl::SleepFor(absl::Milliseconds(20));
  }
  EXPECT_TRUE(ran);
  auto elapsed = absl::Now() - start;
  EXPECT_GE(absl::ToInt64Milliseconds(elapsed), 80);
}

TEST(LinuxMultiThreadExecutorTest, ShutdownPreventsSubmit) {
  MultiThreadExecutor executor(kMaxThreads);
  executor.Shutdown();
  // After Shutdown, DoSubmit should return false when trying to submit work.
  bool submitted = executor.DoSubmit([]() {});
  EXPECT_FALSE(submitted);
}

}  // namespace linux
}  // namespace nearby
