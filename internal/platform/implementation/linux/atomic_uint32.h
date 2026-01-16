#ifndef PLATFORM_IMPL_LINUX_ATOMIC_UINT32_H_
#define PLATFORM_IMPL_LINUX_ATOMIC_UINT32_H_

#include <atomic>
#include <cstdint>
#include "internal/platform/implementation/atomic_reference.h"

namespace nearby {
namespace linux {
// A boolean value that may be updated atomically.
class AtomicUint32 : public api::AtomicUint32 {
 public:
  AtomicUint32(std::uint32_t initial_value) : atomic_uint_(initial_value) {}
  ~AtomicUint32() override = default;

  // Atomically read and return current value.
  std::uint32_t Get() const override { return atomic_uint_; };

  // Atomically exchange original value with a new one. Return previous value.
  void Set(std::uint32_t value) override { atomic_uint_ = value; };

 private:
  std::atomic_bool atomic_uint_ = false;
};
}  // namespace linux
}  // namespace nearby

#endif
