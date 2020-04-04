#ifndef PLATFORM_API2_LISTENABLE_FUTURE_H_
#define PLATFORM_API2_LISTENABLE_FUTURE_H_

#include <memory>

#include "platform/api2/executor.h"
#include "platform/api2/future.h"
#include "platform/exception.h"
#include "platform/runnable.h"

namespace location {
namespace nearby {

// A Future that accepts completion listeners.
//
// https://guava.dev/releases/20.0/api/docs/com/google/common/util/concurrent/ListenableFuture.html
template <typename T>
class ListenableFuture : public Future<T> {
 public:
  ~ListenableFuture() override = default;

  virtual void AddListener(std::unique_ptr<Runnable> runnable,
                           Executor* executor) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_LISTENABLE_FUTURE_H_
