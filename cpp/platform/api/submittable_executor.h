#ifndef PLATFORM_API_SUBMITTABLE_EXECUTOR_H_
#define PLATFORM_API_SUBMITTABLE_EXECUTOR_H_

#include "platform/api/executor.h"
#include "platform/api/future.h"
#include "platform/callable.h"
#include "platform/port/down_cast.h"
#include "platform/ptr.h"
#include "platform/runnable.h"

namespace location {
namespace nearby {

// Each per-platform concrete implementation is expected to extend from
// SubmittableExecutor and provide an override of its submit() method.
//
// e.g.
// class IOSSubmittableExecutor
//                      : public SubmittableExecutor<IOSSubmittableExecutor> {
//  public:
//   template <typename T>
//   Ptr<Future<T> > submit(Ptr<Callable<T> > callable) {
//     ...
//   }
// }
template <typename ConcreteSubmittableExecutor>
class SubmittableExecutor : public Executor {
 public:
  ~SubmittableExecutor() override {}

  template <typename T>
  Ptr<Future<T> > submit(Ptr<Callable<T> > callable) {
    return DOWN_CAST<ConcreteSubmittableExecutor*>(this)->submit(callable);
  }

  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executor.html#execute-java.lang.Runnable-
  virtual void execute(Ptr<Runnable> runnable) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SUBMITTABLE_EXECUTOR_H_
