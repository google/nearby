#ifndef PLATFORM_IMPL_LINUX_ATOMIC_REFERENCE_H_
#define PLATFORM_IMPL_LINUX_ATOMIC_REFERENCE_H_

#include <atomic>

#include "internal/platform/implementation/atomic_reference.h"

namespace nearby {
namespace linux {

// Type that allows 32-bit atomic reads and writes.
class AtomicUint32 : public api::AtomicUint32 {
 public:
  ~AtomicUint32() override = default;

  // Atomically reads and returns stored value.
  std::uint32_t Get() const override { return atomic_uint32_; };

  // Atomically stores value.
  void Set(std::uint32_t value) override { atomic_uint32_ = value; }

 private:
  std::atomic_int32_t atomic_uint32_ = 0;
};

}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_ATOMIC_REFERENCE_H_