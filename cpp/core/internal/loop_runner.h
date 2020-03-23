#ifndef CORE_INTERNAL_LOOP_RUNNER_H_
#define CORE_INTERNAL_LOOP_RUNNER_H_

#include "platform/callable.h"
#include "platform/exception.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

// Construct to run a loop repeatedly. This class is useful to increase
// testability for multi-threaded code that runs loops; it shouldn't be used for
// general purpose loops unless tests require fine-grained control over the
// looping procedure.
class LoopRunner {
 public:
  explicit LoopRunner(const std::string& name);

  // Runs the provided callable repeatedly until it returns false.
  //
  // @return true if the loop completed successfully, false if an exception was
  // encountered.
  bool loop(Ptr<Callable<bool> > callable);

 protected:
  void onEnterLoop();
  void onEnterIteration();
  void onExitIteration();
  void onExitLoop();
  void onExceptionExitLoop(Exception::Value exception);

 private:
  const std::string name_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_LOOP_RUNNER_H_
