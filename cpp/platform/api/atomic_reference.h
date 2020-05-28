#ifndef PLATFORM_API_ATOMIC_REFERENCE_H_
#define PLATFORM_API_ATOMIC_REFERENCE_H_

#include "platform/api/atomic_reference_def.h"
#include "platform/api/platform.h"
#include "platform/ptr.h"
#include "absl/types/any.h"

namespace location {
namespace nearby {

// "Common" part of implementation.
// Placed here for textual compatibility to minimize scope of changes.
// Can be (and should be) moved to a separate file outside "api" folder.
// TODO(apolyudov): for API v2.0
namespace platform {
namespace impl {
template <typename T>
class AtomicReferenceImpl : public AtomicReference<T> {
 public:
  explicit AtomicReferenceImpl(T initial_value) {
    atomic_ = platform::ImplementationPlatform::createAtomicReferenceAny(
        absl::any(initial_value));
  }

  ~AtomicReferenceImpl() override = default;

  void set(T new_value) override { atomic_->set(absl::any(new_value)); }

  T get() override { return absl::any_cast<T>(atomic_->get()); }

 private:
  Ptr<AtomicReference<absl::any>> atomic_;
};

}  // namespace impl

template <typename T>
Ptr<AtomicReference<T>> ImplementationPlatform::createAtomicReference(
    T initial_value) {
  return Ptr<AtomicReference<T>>(
      new impl::AtomicReferenceImpl<T>{initial_value});
}

}  // namespace platform
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_ATOMIC_REFERENCE_H_
