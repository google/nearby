#ifndef PLATFORM_API_SETTABLE_FUTURE_DEF_H_
#define PLATFORM_API_SETTABLE_FUTURE_DEF_H_

#include "platform/api/listenable_future.h"
#include "platform/exception.h"

namespace location {
namespace nearby {

// A SettableFuture is a type of Future whose result can be set.
//
// https://google.github.io/guava/releases/20.0/api/docs/com/google/common/util/concurrent/SettableFuture.html
//
// Platform must implentent non-template static member functions
//   Ptr<SettableFuture<size_t>> CreateSettableFutureSizeT()
//   Ptr<SettableFuture<std::shared_ptr<void>>> CreateSettableFuturePtr()
// in the location::nearby::platform::ImplementationPlatform class.
template <typename T>
class SettableFuture : public ListenableFuture<T> {
 public:
  ~SettableFuture() override = default;

  virtual bool set(T value) = 0;

  virtual bool setException(Exception exception) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SETTABLE_FUTURE_DEF_H_
