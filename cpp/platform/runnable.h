#ifndef PLATFORM_RUNNABLE_H_
#define PLATFORM_RUNNABLE_H_

namespace location {
namespace nearby {

// The Runnable interface should be implemented by any class whose instances are
// intended to be executed by a thread. The class must define a method named
// run() with no arguments.
//
// https://docs.oracle.com/javase/8/docs/api/java/lang/Runnable.html
class Runnable {
 public:
  virtual ~Runnable() {}

  virtual void run() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_RUNNABLE_H_
