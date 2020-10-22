#include "platform/public/multi_thread_executor.h"

#include <atomic>
#include <functional>

#include "platform/base/exception.h"
#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {

namespace {
const int kMaxThreads = 5;
}

TEST(MultiThreadExecutorTest, ConsructorDestructorWorks) {
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
    absl::MutexLock lock(&mutex);
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
    executor.Execute([&count, &mutex, &test_cond, &thread_cond]() {
      absl::MutexLock lock(&mutex);
      count++;
      test_cond.Signal();
      thread_cond.Wait(&mutex);
      count--;
      test_cond.Signal();
    });
  }

  {
    absl::Duration duration = absl::Milliseconds(kMaxThreads * 100);
    absl::MutexLock lock(&mutex);
    while (count < kMaxThreads) {
      absl::Time start = absl::Now();
      if (test_cond.WaitWithTimeout(&mutex, duration)) break;
      duration -= absl::Now() - start;
    }
  }

  EXPECT_EQ(count, kMaxThreads);
  thread_cond.SignalAll();

  {
    absl::Duration duration = absl::Milliseconds(kMaxThreads * 100);
    absl::MutexLock lock(&mutex);
    while (count > 0) {
      absl::Time start = absl::Now();
      if (test_cond.WaitWithTimeout(&mutex, duration)) break;
      duration -= absl::Now() - start;
    }
  }
  EXPECT_EQ(count, 0);
}

TEST(MultiThreadExecutorTest, CanSubmit) {
  MultiThreadExecutor executor(kMaxThreads);
  Future<bool> future;
  bool submitted =
      executor.Submit<bool>([]() { return ExceptionOr<bool>{true}; }, &future);
  EXPECT_TRUE(submitted);
  EXPECT_TRUE(future.Get().result());
}

}  // namespace nearby
}  // namespace location
