#ifndef PLATFORM_API2_EXECUTOR_H_
#define PLATFORM_API2_EXECUTOR_H_

#include <memory>

#include "platform/runnable.h"

namespace location {
namespace nearby {

// This abstract class is the superclass of all classes representing an
// Executor.
class Executor {
 public:
  virtual ~Executor() = default;
  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executor.html#execute-java.lang.Runnable-
  virtual void Execute(std::unique_ptr<Runnable> runnable) = 0;

  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ExecutorService.html#shutdown--
  virtual void Shutdown() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_EXECUTOR_H_
