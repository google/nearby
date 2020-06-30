#ifndef PLATFORM_V2_API_EXECUTOR_H_
#define PLATFORM_V2_API_EXECUTOR_H_

#include "platform_v2/base/runnable.h"

namespace location {
namespace nearby {
namespace api {

int GetCurrentTid();

// This abstract class is the superclass of all classes representing an
// Executor.
class Executor {
 public:
  // Before returning from destructor, executor must wait for all pending
  // jobs to finish.
  virtual ~Executor() = default;
  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executor.html#execute-java.lang.Runnable-
  virtual void Execute(Runnable&& runnable) = 0;

  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ExecutorService.html#shutdown--
  virtual void Shutdown() = 0;

  virtual int GetTid(int index) const = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_EXECUTOR_H_
