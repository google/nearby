#ifndef PLATFORM_API_FUTURE_H_
#define PLATFORM_API_FUTURE_H_

#include <cstdint>

#include "platform/exception.h"

namespace location {
namespace nearby {

// A Future represents the result of an asynchronous computation.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Future.html
template <typename T>
class Future {
 public:
  virtual ~Future() {}

  virtual ExceptionOr<T>
  get() = 0;  // throws Exception::INTERRUPTED, Exception::EXECUTION

  // throws Exception::INTERRUPTED, Exception::EXECUTION
  // throws Exception::TIMEOUT if |timeout_ms| is exceeded while waiting for
  // result.
  virtual ExceptionOr<T> get(std::int64_t timeout_ms) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_FUTURE_H_
