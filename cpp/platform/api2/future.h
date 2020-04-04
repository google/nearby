#ifndef PLATFORM_API2_FUTURE_H_
#define PLATFORM_API2_FUTURE_H_

#include "platform/exception.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {

// A Future represents the result of an asynchronous computation.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Future.html
template <typename T>
class Future {
 public:
  virtual ~Future() = default;

  // throws Exception::kInterrupted, Exception::kExecution
  virtual ExceptionOr<T> Get() = 0;

  // throws Exception::kInterrupted, Exception::kExecution
  // throws Exception::kTimeout if timeout is exceeded while waiting for
  // result.
  virtual ExceptionOr<T> Get(absl::Duration timeout) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_FUTURE_H_
