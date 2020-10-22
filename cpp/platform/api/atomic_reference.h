#ifndef PLATFORM_API_ATOMIC_REFERENCE_H_
#define PLATFORM_API_ATOMIC_REFERENCE_H_

#include <cstdint>

namespace location {
namespace nearby {
namespace api {

// Type that allows 32-bit atomic reads and writes.
class AtomicUint32 {
 public:
  virtual ~AtomicUint32() = default;

  // Atomically reads and returns stored value.
  virtual std::uint32_t Get() const = 0;

  // Atomically stores value.
  virtual void Set(std::uint32_t value) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_ATOMIC_REFERENCE_H_
