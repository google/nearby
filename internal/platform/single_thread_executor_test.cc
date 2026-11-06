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

#include "internal/platform/single_thread_executor.h"

#include <atomic>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/future.h"
#include "internal/platform/implementation/executor.h"

namespace nearby {

TEST(SingleThreadExecutorTest, ConsructorDestructorWorks) {
  SingleThreadExecutor executor;
}

TEST(SingleThreadExecutorTest, CanExecute) {
  absl::CondVar cond;
  std::atomic_bool done = false;
  SingleThreadExecutor executor;
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

TEST(SingleThreadExecutorTest, CanExecuteNamedTask) {
  absl::CondVar cond;
  std::atomic_bool done = false;
  SingleThreadExecutor executor;
  executor.Execute("my task", [&done, &cond]() {
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

TEST(SingleThreadExecutorTest, JobsExecuteInOrder) {
  std::vector<int> results;
  SingleThreadExecutor executor;

  for (int i = 0; i < 10; ++i) {
    executor.Execute([i, &results]() { results.push_back(i); });
  }

  absl::CondVar cond;
  std::atomic_bool done = false;
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
  EXPECT_EQ(results, (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
}

TEST(SingleThreadExecutorTest, CanSubmit) {
  SingleThreadExecutor executor;
  Future<bool> future;
  bool submitted =
      executor.Submit<bool>([]() { return ExceptionOr<bool>{true}; }, &future);
  EXPECT_TRUE(submitted);
  EXPECT_TRUE(future.Get().result());
}

TEST(SingleThreadExecutorTest, ShutdownWaitsForRunningTasks) {
  SingleThreadExecutor executor;
  std::atomic_int value = 0;
  executor.Execute([&]() {
    absl::SleepFor(absl::Seconds(1));
    value += 1;
  });

  executor.Shutdown();

  EXPECT_EQ(value, 1);
}

TEST(SingleThreadExecutorTest, ExecuteAfterShutdownFails) {
  SingleThreadExecutor executor;

  executor.Shutdown();
  executor.Execute([&]() { FAIL() << "Task should not run"; });
}

TEST(SingleThreadExecutorTest, ExecuteDuringShutdownFails) {
  CountDownLatch latch(1);
  SingleThreadExecutor executor;

  executor.Execute([&]() {
    latch.CountDown();
    absl::SleepFor(absl::Seconds(1));
    executor.Execute([&]() { FAIL() << "Task should not run"; });
  });
  latch.Await();
  executor.Shutdown();
}

struct ThreadCheckTestClass {
  SingleThreadExecutor executor;
  int value ABSL_GUARDED_BY(executor) = 0;

  void incValue() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor) { value++; }
  int getValue() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor) { return value; }
};

TEST(SingleThreadExecutorTest, ThreadCheck_ExecuteRunnable) {
  ThreadCheckTestClass test_class;

  test_class.executor.Execute(
      [&test_class]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(test_class.executor) {
        test_class.incValue();
      });
}

TEST(SingleThreadExecutorTest, ThreadCheck_SubmitCallable) {
  ThreadCheckTestClass test_class;
  test_class.executor.Execute(
      [&test_class]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(test_class.executor) {
        test_class.incValue();
      });
  Future<int> future;

  bool submitted = test_class.executor.Submit<int>(
      [&test_class]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(test_class.executor) {
        return ExceptionOr<int>{test_class.getValue()};
      },
      &future);

  EXPECT_TRUE(submitted);
  EXPECT_EQ(future.Get().result(), 1);
}

TEST(SingleThreadExecutorTest, GetThreadIdMatchesExecutionThread) {
  SingleThreadExecutor executor;
  const ThreadId caller_tid = api::GetCurrentThreadId();
  const ThreadId executor_tid = executor.GetThreadId();
  EXPECT_NE(caller_tid, 0u);
  EXPECT_NE(executor_tid, 0u);
  EXPECT_NE(executor_tid, caller_tid);

  std::vector<ThreadId> observed_tids;
  executor.Execute([&observed_tids]() {
    observed_tids.push_back(api::GetCurrentThreadId());
  });
  executor.Execute("named-task", [&observed_tids]() {
    observed_tids.push_back(api::GetCurrentThreadId());
  });

  Future<ThreadId> future;
  ASSERT_TRUE(executor.Submit<ThreadId>(
      []() { return ExceptionOr<ThreadId>{api::GetCurrentThreadId()}; },
      &future));
  EXPECT_EQ(future.Get().result(), executor_tid);

  ASSERT_EQ(observed_tids.size(), 2u);
  EXPECT_EQ(observed_tids[0], executor_tid);
  EXPECT_EQ(observed_tids[1], executor_tid);
}

TEST(SingleThreadExecutorTest, DifferentExecutorsHaveDifferentThreadIds) {
  SingleThreadExecutor executor1;
  SingleThreadExecutor executor2;

  EXPECT_NE(executor1.GetThreadId(), 0u);
  EXPECT_NE(executor2.GetThreadId(), 0u);
  EXPECT_NE(executor1.GetThreadId(), executor2.GetThreadId());

  Future<ThreadId> future1;
  Future<ThreadId> future2;
  ASSERT_TRUE(executor1.Submit<ThreadId>(
      []() { return ExceptionOr<ThreadId>{api::GetCurrentThreadId()}; },
      &future1));
  ASSERT_TRUE(executor2.Submit<ThreadId>(
      []() { return ExceptionOr<ThreadId>{api::GetCurrentThreadId()}; },
      &future2));

  EXPECT_EQ(future1.Get().result(), executor1.GetThreadId());
  EXPECT_EQ(future2.Get().result(), executor2.GetThreadId());
}

TEST(SingleThreadExecutorTest, MovePreservesThreadId) {
  SingleThreadExecutor executor1;
  const ThreadId original_tid = executor1.GetThreadId();
  ASSERT_NE(original_tid, 0u);

  SingleThreadExecutor executor2(std::move(executor1));
  EXPECT_EQ(executor2.GetThreadId(), original_tid);

  SingleThreadExecutor executor3;
  const ThreadId executor3_initial_tid = executor3.GetThreadId();
  EXPECT_NE(executor3_initial_tid, original_tid);
  executor3 = std::move(executor2);
  EXPECT_EQ(executor3.GetThreadId(), original_tid);

  absl::Notification done;
  ThreadId observed_tid = 0;
  executor3.Execute([&observed_tid, &done]() {
    observed_tid = api::GetCurrentThreadId();
    done.Notify();
  });
  ASSERT_TRUE(done.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_EQ(observed_tid, original_tid);
}

TEST(SingleThreadExecutorTest, GetThreadIdConsistentAfterShutdown) {
  SingleThreadExecutor executor;
  const ThreadId tid_before = executor.GetThreadId();
  EXPECT_NE(tid_before, 0u);

  executor.Shutdown();
  EXPECT_EQ(executor.GetThreadId(), tid_before);
}

}  // namespace nearby
