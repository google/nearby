#ifndef PLATFORM_API_LISTENABLE_FUTURE_H_
#define PLATFORM_API_LISTENABLE_FUTURE_H_

#include "platform/api/executor.h"
#include "platform/api/future.h"
#include "platform/exception.h"
#include "platform/ptr.h"
#include "platform/runnable.h"

namespace location {
namespace nearby {

// A Future that accepts completion listeners.
//
// https://guava.dev/releases/20.0/api/docs/com/google/common/util/concurrent/ListenableFuture.html
template <typename T>
class ListenableFuture : public Future<T> {
 public:
  ~ListenableFuture() override {}

  // Executor is shared among multiple runnables. It is not owned by any future.
  virtual void addListener(Ptr<Runnable> runnable, Executor* executor) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_LISTENABLE_FUTURE_H_
