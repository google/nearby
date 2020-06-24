#include "platform_v2/public/condition_variable.h"

#include "platform_v2/public/logging.h"
#include "platform_v2/public/mutex.h"
#include "platform_v2/public/single_thread_executor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace {

TEST(ConditionVariableTest, CanCreate) {
  Mutex mutex;
  ConditionVariable cond{&mutex};
}

TEST(ConditionVariableTest, CanWakeupWaiter) {
  Mutex mutex;
  ConditionVariable cond{&mutex};
  bool done = false;
  bool waiting = false;
  NEARBY_LOG(INFO, "At start; done=%d", done);
  {
    SingleThreadExecutor executor;
    executor.Execute([&cond, &mutex, &done, &waiting]() {
      MutexLock lock(&mutex);
      NEARBY_LOG(INFO, "Before cond.Wait(); done=%d", done);
      waiting = true;
      cond.Wait();
      waiting = false;
      done = true;
      NEARBY_LOG(INFO, "After cond.Wait(); done=%d", done);
    });
    while (true) {
      {
        MutexLock lock(&mutex);
        if (waiting) break;
      }
      SystemClock::Sleep(absl::Milliseconds(100));
    }
    {
      MutexLock lock(&mutex);
      cond.Notify();
      EXPECT_FALSE(done);
    }
  }
  NEARBY_LOG(INFO, "After executor shutdown: done=%d", done);
  EXPECT_TRUE(done);
}

TEST(ConditionVariableTest, WaitTerminatesOnTimeoutWithoutNotify) {
  Mutex mutex;
  ConditionVariable cond{&mutex};
  MutexLock lock(&mutex);
  EXPECT_EQ(cond.Wait(absl::Milliseconds(100)), Exception{Exception::kTimeout});
}

}  // namespace
}  // namespace nearby
}  // namespace location
