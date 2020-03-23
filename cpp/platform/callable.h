#ifndef PLATFORM_CALLABLE_H_
#define PLATFORM_CALLABLE_H_

#include "platform/exception.h"

namespace location {
namespace nearby {

// The Callable interface should be implemented by any class whose instances are
// intended to be executed by a thread, and need to return a result. The class
// must define a method named call() with no arguments and a specific return
// type.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Callable.html
template <typename T>
class Callable {
 public:
  virtual ~Callable() {}

  virtual ExceptionOr<T> call() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_CALLABLE_H_
