#ifndef PLATFORM_V2_API_ATOMIC_BOOLEAN_H_
#define PLATFORM_V2_API_ATOMIC_BOOLEAN_H_

namespace location {
namespace nearby {
namespace api {

// A boolean value that may be updated atomically.
class AtomicBoolean {
 public:
  virtual ~AtomicBoolean() = default;

  // Atomically read and return current value.
  virtual bool Get() const = 0;

  // Atomically exchange original value with a new one. Return previous value.
  virtual bool Set(bool value) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_ATOMIC_BOOLEAN_H_
