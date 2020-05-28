#ifndef PLATFORM_V2_API_FUTURE_H_
#define PLATFORM_V2_API_FUTURE_H_

#include "platform_v2/base/exception.h"
#include "absl/time/clock.h"

namespace location {
namespace nearby {
namespace api {

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

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_FUTURE_H_
