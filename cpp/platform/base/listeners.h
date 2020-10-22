#ifndef PLATFORM_BASE_LISTENERS_H_
#define PLATFORM_BASE_LISTENERS_H_

#include <functional>

namespace location {
namespace nearby {

// Provides default-initialization with a valid empty method,
// instead of nullptr. This allows partial initialization
// of a set of listeners.
template <typename... Args>
constexpr std::function<void(Args...)> DefaultCallback() {
  return std::function<void(Args...)>{[](Args...) {}};
}

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_LISTENERS_H_
