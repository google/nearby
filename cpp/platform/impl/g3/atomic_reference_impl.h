#ifndef PLATFORM_IMPL_G3_ATOMIC_REFERENCE_IMPL_H_
#define PLATFORM_IMPL_G3_ATOMIC_REFERENCE_IMPL_H_

#include "platform/api/atomic_reference.h"
#include "platform/ptr.h"
#include "absl/base/integral_types.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/any.h"

namespace location {
namespace nearby {
namespace platform {

// Provide implementation for absl::any.
class AtomicReferenceImpl : public AtomicReference<absl::any> {
 public:
  explicit AtomicReferenceImpl(absl::any initial_value)
      : value_(std::move(initial_value)) {}
  ~AtomicReferenceImpl() override = default;

  absl::any get() override {
    absl::MutexLock lock(&mutex_);
    return value_;
  }
  void set(absl::any value) override {
    absl::MutexLock lock(&mutex_);
    value_ = std::move(value);
  }

 private:
  absl::Mutex mutex_;
  absl::any value_;
};

}  // namespace platform
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_G3_ATOMIC_REFERENCE_IMPL_H_
