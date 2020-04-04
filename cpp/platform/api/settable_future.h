#ifndef PLATFORM_API_SETTABLE_FUTURE_H_
#define PLATFORM_API_SETTABLE_FUTURE_H_

#include "platform/api/listenable_future.h"

namespace location {
namespace nearby {

// A SettableFuture is a type of Future whose result can be set.
//
// https://google.github.io/guava/releases/20.0/api/docs/com/google/common/util/concurrent/SettableFuture.html
template <typename T>
class SettableFuture : public ListenableFuture<T> {
 public:
  ~SettableFuture() override {}

  virtual bool set(T value) = 0;

  virtual bool setException(Exception exception) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SETTABLE_FUTURE_H_
