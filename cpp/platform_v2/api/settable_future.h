#ifndef PLATFORM_V2_API_SETTABLE_FUTURE_H_
#define PLATFORM_V2_API_SETTABLE_FUTURE_H_

#include "platform_v2/api/listenable_future.h"
#include "platform_v2/base/exception.h"

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

  virtual bool Set(const T& value) = 0;
  virtual bool Set(T&& value) = 0;
  virtual bool SetException(Exception exception) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_SETTABLE_FUTURE_H_
