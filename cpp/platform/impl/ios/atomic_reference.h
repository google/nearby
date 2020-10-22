#ifndef PLATFORM_IMPL_IOS_ATOMIC_REFERENCE_H_
#define PLATFORM_IMPL_IOS_ATOMIC_REFERENCE_H_

#include <atomic>
#include <cstdint>

#include "platform/api/atomic_reference.h"

namespace location {
namespace nearby {
namespace ios {

class AtomicUint32 : public api::AtomicUint32 {
 public:
  explicit AtomicUint32(std::int32_t value) : value_(value) {}
  ~AtomicUint32() override = default;

  std::uint32_t Get() const override {
    return value_;
  }
  void Set(std::uint32_t value) override {
    value_ = value;
  }

 private:
  std::atomic<std::uint32_t> value_;
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_IOS_ATOMIC_REFERENCE_H_
