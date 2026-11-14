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

#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
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
  executor.Execute([&done, &cond]() {
    done = true;
    cond.SignalAll();
  });
  absl::Mutex mutex;
  {
    absl::MutexLock lock(mutex);
    if (!done) {
      cond.WaitWithTimeout(&mutex, absl::Seconds(1));
    }
  }
  EXPECT_TRUE(done);
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

}  // namespace
}  // namespace nearby
