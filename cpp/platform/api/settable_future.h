#ifndef PLATFORM_API_SETTABLE_FUTURE_H_
#define PLATFORM_API_SETTABLE_FUTURE_H_

#include "platform/api/listenable_future.h"
#include "platform/base/exception.h"

namespace location {
namespace nearby {
namespace api {

// A SettableFuture is a type of Future whose result can be set.
//
// https://google.github.io/guava/releases/20.0/api/docs/com/google/common/util/concurrent/SettableFuture.html
template <typename T>
class SettableFuture : public ListenableFuture<T> {
 public:
  ~SettableFuture() override = default;

  // Completes the future successfully. The value is returned to any waiters.
  // Returns true, if value was set.
  // Returns false, if Future is already in "done" state.
  virtual bool Set(T value) = 0;

  // Completes the future unsuccessfully. The exception value is returned to any
  // waiters.
  // Returns true, if exception was set.
  // Returns false, if Future is already in "done" state.
  virtual bool SetException(Exception exception) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SETTABLE_FUTURE_H_
