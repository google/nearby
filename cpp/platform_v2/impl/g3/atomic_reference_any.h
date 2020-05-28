#ifndef PLATFORM_V2_IMPL_G3_ATOMIC_REFERENCE_ANY_H_
#define PLATFORM_V2_IMPL_G3_ATOMIC_REFERENCE_ANY_H_

#include "platform_v2/api/atomic_reference.h"
#include "absl/base/integral_types.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/any.h"

namespace location {
namespace nearby {
namespace g3 {

// Provide implementation for absl::any.
class AtomicReferenceAny : public api::AtomicReference<absl::any> {
 public:
  explicit AtomicReferenceAny(absl::any initial_value)
      : value_(std::move(initial_value)) {}
  ~AtomicReferenceAny() override = default;

  absl::any Get() const & override {
    absl::MutexLock lock(&mutex_);
    return value_;
  }
  absl::any Get() && override {
    absl::MutexLock lock(&mutex_);
    return std::move(value_);
  }
  void Set(const absl::any& value) override {
    absl::MutexLock lock(&mutex_);
    value_ = value;
  }
  void Set(absl::any&& value) override {
    absl::MutexLock lock(&mutex_);
    value_ = std::move(value);
  }

 private:
  mutable absl::Mutex mutex_;
  absl::any value_;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_G3_ATOMIC_REFERENCE_ANY_H_
