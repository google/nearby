#ifndef PLATFORM_API2_SUBMITTABLE_EXECUTOR_H_
#define PLATFORM_API2_SUBMITTABLE_EXECUTOR_H_

#include <memory>

#include "platform/api2/executor.h"
#include "platform/api2/future.h"
#include "platform/callable.h"

namespace location {
namespace nearby {

// Each per-platform concrete implementation is expected to extend from
// SubmittableExecutor and provide an override of its submit() method.
//
// e.g.
// class XyzSubmittableExecutor
//                      : public SubmittableExecutor<XyzSubmittableExecutor> {
//  public:
//   template <typename T>
//   std::unique_ptr<Future<T>> submit(std::unique_ptr<Callable<T>> callable) {
//     ...
//   }
// }
template <typename ConcreteSubmittableExecutor>
class SubmittableExecutor : public Executor {
 public:
  ~SubmittableExecutor() override {}

  template <typename T>
  std::unique_ptr<Future<T>> Submit(std::unique_ptr<Callable<T>> callable) {
    static_assert(
        std::is_base_of_v<SubmittableExecutor, ConcreteSubmittableExecutor>,
        "Class template type is not derived from SubmittableExecutor");
    return static_cast<ConcreteSubmittableExecutor*>(this)->submit(callable);
  }
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_SUBMITTABLE_EXECUTOR_H_
