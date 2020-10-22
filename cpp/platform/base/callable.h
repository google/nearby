#ifndef PLATFORM_BASE_CALLABLE_H_
#define PLATFORM_BASE_CALLABLE_H_

#include <functional>

#include "platform/base/exception.h"

namespace location {
namespace nearby {

// The Callable is and object intended to be executed by a thread, that is able
// to return a value of specified type T.
// It must be invokable without arguments. It must return a value implicitly
// convertible to ExceptionOr<T>.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Callable.html
template <typename T>
using Callable = std::function<ExceptionOr<T>()>;

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_CALLABLE_H_
