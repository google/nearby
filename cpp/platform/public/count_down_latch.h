#ifndef PLATFORM_PUBLIC_COUNT_DOWN_LATCH_H_
#define PLATFORM_PUBLIC_COUNT_DOWN_LATCH_H_

#include <cstdint>

#include "platform/api/count_down_latch.h"
#include "platform/api/platform.h"
#include "platform/base/exception.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {

// A synchronization aid that allows one or more threads to wait until a set of
// operations being performed in other threads completes.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CountDownLatch.html
class CountDownLatch final {
 public:
  using Platform = api::ImplementationPlatform;
  explicit CountDownLatch(int count)
      : impl_(Platform::CreateCountDownLatch(count)) {}
  CountDownLatch(CountDownLatch&&) = default;
  CountDownLatch& operator=(CountDownLatch&&) = default;
  ~CountDownLatch() = default;

  Exception Await() { return impl_->Await(); }
  ExceptionOr<bool> Await(absl::Duration timeout) {
    return impl_->Await(timeout);
  }
  void CountDown() { impl_->CountDown(); }

 private:
  std::unique_ptr<api::CountDownLatch> impl_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_COUNT_DOWN_LATCH_H_
