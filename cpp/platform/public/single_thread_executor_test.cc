#include "platform/public/single_thread_executor.h"

#include <atomic>
#include <functional>

#include "platform/base/exception.h"
#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"

namespace location {
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
    absl::MutexLock lock(&mutex);
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
    absl::MutexLock lock(&mutex);
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

}  // namespace nearby
}  // namespace location
