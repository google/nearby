#ifndef PLATFORM_IMPL_LINUX_FUTURE_H_
#define PLATFORM_IMPL_LINUX_FUTURE_H_

#include "internal/platform/implementation/future.h"

namespace nearby {
namespace linux {

// A Future represents the result of an asynchronous computation.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Future.html
template <typename T>
class Future : public api::Future<T> {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~Future() override = default;

  // throws Exception::kInterrupted, Exception::kExecution
  // TODO(b/184975123): replace with real implementation.
  ExceptionOr<T> Get() override { return ExceptionOr<T>{Exception::kFailed}; }

  // throws Exception::kInterrupted, Exception::kExecution
  // throws Exception::kTimeout if timeout is exceeded while waiting for
  // result.
  // TODO(b/184975123): replace with real implementation.
  ExceptionOr<T> Get(absl::Duration timeout) override {
    return ExceptionOr<T>{Exception::kFailed};
  }
};

}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_FUTURE_H_
