#ifndef PLATFORM_API2_COUNT_DOWN_LATCH_H_
#define PLATFORM_API2_COUNT_DOWN_LATCH_H_

#include <cstdint>

#include "platform/exception.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {

// A synchronization aid that allows one or more threads to wait until a set of
// operations being performed in other threads completes.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CountDownLatch.html
class CountDownLatch {
 public:
  virtual ~CountDownLatch() {}

  virtual Exception Await() = 0;  // throws Exception::kInterrupted
  virtual ExceptionOr<bool> Await(
      absl::Duration timeout) = 0;  // throws Exception::kInterrupted
  virtual void CountDown() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_COUNT_DOWN_LATCH_H_
