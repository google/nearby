#ifndef PLATFORM_IMPL_SAMPLE_ATOMIC_REFERENCE_IMPL_H_
#define PLATFORM_IMPL_SAMPLE_ATOMIC_REFERENCE_IMPL_H_

#include "platform/api/atomic_reference_def.h"
#include "absl/types/any.h"

namespace location {
namespace nearby {
namespace platform {

// Provide implementation for absl::any.
class AtomicReferenceImpl : public AtomicReference<absl::any> {
 public:
  explicit AtomicReferenceImpl(absl::any initial_value) {}
  ~AtomicReferenceImpl() override = default;

  absl::any get() override { return {}; }
  void set(absl::any value) override {}
};

}  // namespace platform
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_SAMPLE_ATOMIC_REFERENCE_IMPL_H_
