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
#include <memory>
#include <string>
#include <thread>  // NOLINT(build/c++11)
#include <vector>

#include "gtest/gtest.h"
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/timer.h"

namespace nearby {
namespace {

TEST(SingleThreadExecutorTest, ConsructorDestructorWorks) {
  SingleThreadExecutor executor;
}

TEST(SingleThreadExecutorTest, CanExecute) {
  absl::CondVar cond;
  std::atomic_bool done = false;
  SingleThreadExecutor executor;
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
}

TEST(SingleThreadExecutorTest, CanExecuteNamedTask) {
  absl::CondVar cond;
  std::atomic_bool done = false;
  SingleThreadExecutor executor;
  EXPECT_TRUE(executor.Execute("my task", [&done, &cond]() {
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
  EXPECT_FALSE(executor.Execute(nullptr));
  EXPECT_FALSE(executor.Execute("empty", nullptr));

  executor.Shutdown();
  EXPECT_FALSE(executor.Execute([&]() { FAIL() << "Task should not run"; }));
  EXPECT_FALSE(
      executor.Execute("named", [&]() { FAIL() << "Task should not run"; }));
}

TEST(SingleThreadExecutorTest, ExecuteDuringShutdownFails) {
  absl::Notification notification;
  SingleThreadExecutor executor;

  executor.Execute([&]() {
    notification.Notify();
    absl::SleepFor(absl::Seconds(1));
    executor.Execute([&]() { FAIL() << "Task should not run"; });
  });
  notification.WaitForNotification();
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

TEST(SingleThreadExecutorTest, DelayedExecute) {
  SingleThreadExecutor executor;
  std::atomic_bool first_task_started = false;
  absl::BlockingCounter counter(2);

  EXPECT_TRUE(executor.DelayedExecute(absl::Milliseconds(100), [&]() {
    first_task_started.store(true, std::memory_order_seq_cst);
    counter.DecrementCount();
  }));

  executor.Execute([&]() {
    EXPECT_FALSE(first_task_started.load(std::memory_order_seq_cst));
    counter.DecrementCount();
  });

  counter.Wait();
  EXPECT_TRUE(first_task_started.load(std::memory_order_seq_cst));
}

TEST(SingleThreadExecutorTest, TwoDelayedExecuteTasks) {
  SingleThreadExecutor executor;
  std::atomic_bool first_task_started = false;
  absl::BlockingCounter counter(2);

  EXPECT_TRUE(executor.DelayedExecute(absl::Milliseconds(500), [&]() {
    first_task_started.store(true, std::memory_order_seq_cst);
    counter.DecrementCount();
  }));

  EXPECT_TRUE(executor.DelayedExecute(absl::Milliseconds(1), [&]() {
    EXPECT_FALSE(first_task_started.load(std::memory_order_seq_cst));
    counter.DecrementCount();
  }));

  counter.Wait();
  EXPECT_TRUE(first_task_started.load(std::memory_order_seq_cst));
}

TEST(SingleThreadExecutorTest, DelayedExecuteEmptyTask) {
  SingleThreadExecutor executor;
  EXPECT_TRUE(executor.DelayedExecute(absl::Milliseconds(100), nullptr));
}

TEST(SingleThreadExecutorTest, DelayedExecuteAfterShutdownFails) {
  SingleThreadExecutor executor;
  executor.Shutdown();

  EXPECT_FALSE(executor.DelayedExecute(
      absl::Milliseconds(50), [&]() { FAIL() << "Task should not run"; }));
}

TEST(SingleThreadExecutorTest, ShutdownCancelsPendingDelayedExecute) {
  SingleThreadExecutor executor;
  std::atomic_bool called = false;

  EXPECT_TRUE(executor.DelayedExecute(absl::Seconds(10), [&]() {
    called.store(true, std::memory_order_seq_cst);
  }));
  executor.Shutdown();

  EXPECT_FALSE(called.load(std::memory_order_seq_cst));
}

TEST(SingleThreadExecutorTest, DelayedExecuteZeroDelay) {
  SingleThreadExecutor executor;
  absl::Notification done;

  EXPECT_TRUE(executor.DelayedExecute(absl::ZeroDuration(),
                                      [&done]() { done.Notify(); }));

  EXPECT_TRUE(done.WaitForNotificationWithTimeout(absl::Seconds(5)));
}

TEST(SingleThreadExecutorTest, DelayedExecuteNegativeDelayExecutesImmediately) {
  SingleThreadExecutor executor;
  absl::BlockingCounter counter(2);

  EXPECT_TRUE(executor.DelayedExecute(
      -absl::Seconds(1), [&counter]() { counter.DecrementCount(); }));
  EXPECT_TRUE(executor.DelayedExecute(
      -absl::InfiniteDuration(), [&counter]() { counter.DecrementCount(); }));

  counter.Wait();
}

TEST(SingleThreadExecutorTest, MultipleShutdownCallsAreIdempotent) {
  SingleThreadExecutor executor;
  std::atomic_bool called = false;

  EXPECT_TRUE(executor.DelayedExecute(absl::Seconds(10), [&called]() {
    called.store(true, std::memory_order_seq_cst);
  }));
  executor.Shutdown();
  executor.Shutdown();

  EXPECT_FALSE(called.load(std::memory_order_seq_cst));
}

TEST(SingleThreadExecutorTest, DelayedExecuteLargeDelayDoesNotFireImmediately) {
  SingleThreadExecutor executor;
  std::atomic_bool called = false;
  absl::Notification fence_done;

  EXPECT_TRUE(executor.DelayedExecute(absl::Hours(24 * 30), [&called]() {
    called.store(true, std::memory_order_seq_cst);
  }));
  EXPECT_TRUE(executor.DelayedExecute(absl::InfiniteDuration(), [&called]() {
    called.store(true, std::memory_order_seq_cst);
  }));
  executor.Execute([&fence_done]() { fence_done.Notify(); });

  EXPECT_TRUE(fence_done.WaitForNotificationWithTimeout(absl::Seconds(5)));
  absl::SleepFor(absl::Milliseconds(50));
  executor.Shutdown();
  EXPECT_FALSE(called.load(std::memory_order_seq_cst));
}

TEST(SingleThreadExecutorTest, ShutdownConcurrentWithDelayedExecuteCallbacks) {
  // Stress-tests that concurrent timer callbacks racing with Shutdown() and
  // destruction do not deadlock or trigger use-after-free.
  constexpr int kIterations = 50;
  constexpr int kTimersPerIteration = 20;
  std::atomic_int immediate_count = 0;
  std::atomic_int delayed_count = 0;
  for (int iter = 0; iter < kIterations; ++iter) {
    SingleThreadExecutor executor;
    executor.Execute([&immediate_count]() {
      immediate_count.fetch_add(1, std::memory_order_relaxed);
    });
    for (int i = 0; i < kTimersPerIteration; ++i) {
      EXPECT_TRUE(executor.DelayedExecute(
          absl::Milliseconds(i % 3), [&delayed_count]() {
            delayed_count.fetch_add(1, std::memory_order_relaxed);
          }));
    }
    absl::SleepFor(absl::Milliseconds(1));
    executor.Shutdown();
  }
  EXPECT_EQ(immediate_count.load(std::memory_order_relaxed), kIterations);
  EXPECT_LE(delayed_count.load(std::memory_order_relaxed),
            kIterations * kTimersPerIteration);
}

TEST(SingleThreadExecutorTest, ThreadCheck_DelayedExecuteRunnable) {
  ThreadCheckTestClass test_class;
  absl::Notification notification;

  EXPECT_TRUE(test_class.executor.DelayedExecute(
      absl::Milliseconds(10),
      [&test_class,
       &notification]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(test_class.executor) {
        test_class.incValue();
        EXPECT_EQ(test_class.getValue(), 1);
        notification.Notify();
      }));
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(absl::Seconds(5)));
}

class FailingTimer : public Timer {
 public:
  bool Start(int delay, int period,
             absl::AnyInvocable<void()> callback) override {
    return false;
  }
  void Stop() override {}
  bool IsRunning() override { return false; }
};

class FailingTimerSingleThreadExecutor : public SingleThreadExecutor {
 protected:
  absl_nonnull std::unique_ptr<Timer> CreateTimer() override {
    return std::make_unique<FailingTimer>();
  }
};

TEST(SingleThreadExecutorTest, DelayedExecuteTimerStartFailureReturnsFalse) {
  FailingTimerSingleThreadExecutor executor;

  EXPECT_FALSE(executor.DelayedExecute(
      absl::Milliseconds(10), []() { FAIL() << "Task should not run"; }));
}

TEST(SingleThreadExecutorTest, ConcurrentExecuteAndShutdown) {
  constexpr int kIterations = 25;
  constexpr int kProducerThreads = 4;
  constexpr int kTasksPerThread = 50;

  for (int iter = 0; iter < kIterations; ++iter) {
    SingleThreadExecutor executor;
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
