#ifndef PLATFORM_V2_IMPL_IOS_COUNT_DOWN_LATCH_H_
#define PLATFORM_V2_IMPL_IOS_COUNT_DOWN_LATCH_H_

#include "platform_v2/api/count_down_latch.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"

namespace location {
namespace nearby {
namespace ios {

class CountDownLatch final : public api::CountDownLatch {
 public:
  explicit CountDownLatch(int count) : count_(count) {}
  CountDownLatch(const CountDownLatch&) = delete;
  CountDownLatch& operator=(const CountDownLatch&) = delete;
  CountDownLatch(CountDownLatch&&) = delete;
  CountDownLatch& operator=(CountDownLatch&&) = delete;
  ExceptionOr<bool> Await(absl::Duration timeout) override {
    absl::MutexLock lock(&mutex_);
    absl::Time deadline = absl::Now() + timeout;
    while (count_ > 0) {
      if (cond_.WaitWithDeadline(&mutex_, deadline)) {
        return ExceptionOr<bool>(false);
      }
    }
    return ExceptionOr<bool>(true);
  }
  Exception Await() override {
    absl::MutexLock lock(&mutex_);
    while (count_ > 0) {
      cond_.Wait(&mutex_);
    }
    return {Exception::kSuccess};
  }
  void CountDown() override {
    absl::MutexLock lock(&mutex_);
    if (count_ > 0 && --count_ == 0) {
      cond_.SignalAll();
    }
  }

 private:
  absl::Mutex mutex_;   // Mutex to be used with cond_.Wait...() method family.
  absl::CondVar cond_;  // Condition to synchronize up to N waiting threads.
  int count_
      ABSL_GUARDED_BY(mutex_);  // When zero, latch should release all waiters.
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_IOS_COUNT_DOWN_LATCH_H_
