#ifndef PLATFORM_BASE_RUNNABLE_H_
#define PLATFORM_BASE_RUNNABLE_H_

#include <functional>

namespace location {
namespace nearby {

// The Runnable is an object intended to be executed by a thread.
// It must be invokable without arguments. It must return void.
//
// https://docs.oracle.com/javase/8/docs/api/java/lang/Runnable.html

using Runnable = std::function<void()>;

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_RUNNABLE_H_
