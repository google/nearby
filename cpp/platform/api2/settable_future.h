#ifndef PLATFORM_API2_SETTABLE_FUTURE_H_
#define PLATFORM_API2_SETTABLE_FUTURE_H_

#include "platform/api2/listenable_future.h"

namespace location {
namespace nearby {

// A SettableFuture is a type of Future whose result can be set.
//
// https://google.github.io/guava/releases/20.0/api/docs/com/google/common/util/concurrent/SettableFuture.html
template <typename T>
class SettableFuture : public ListenableFuture<T> {
 public:
  ~SettableFuture() override = default;

  virtual bool Set(const T& value) = 0;
  virtual bool SetException(Exception exception) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_SETTABLE_FUTURE_H_
