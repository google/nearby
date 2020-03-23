#ifndef PLATFORM_API_COUNT_DOWN_LATCH_H_
#define PLATFORM_API_COUNT_DOWN_LATCH_H_

#include <cstdint>

#include "platform/exception.h"

namespace location {
namespace nearby {

// A synchronization aid that allows one or more threads to wait until a set of
// operations being performed in other threads completes.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CountDownLatch.html
class CountDownLatch {
 public:
  virtual ~CountDownLatch() {}

  virtual Exception::Value await() = 0;  // throws Exception::INTERRUPTED
  virtual ExceptionOr<bool> await(
      std::int32_t timeout_millis) = 0;  // throws Exception::INTERRUPTED
  virtual void countDown() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_COUNT_DOWN_LATCH_H_
