#ifndef PLATFORM_V2_PUBLIC_ATOMIC_REFERENCE_H_
#define PLATFORM_V2_PUBLIC_ATOMIC_REFERENCE_H_

#include <memory>

#include "platform_v2/api/atomic_reference.h"
#include "platform_v2/api/platform.h"
#include "absl/types/any.h"

namespace location {
namespace nearby {

// An object reference that may be updated atomically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/atomic/AtomicReference.html
template <typename T>
class AtomicReference final : public api::AtomicReference<T> {
 public:
  using Platform = api::ImplementationPlatform;
  explicit AtomicReference(const T& value)
      : impl_(Platform::CreateAtomicReferenceAny(value)) {}
  explicit AtomicReference(T&& value)
      : impl_(Platform::CreateAtomicReferenceAny(std::move(value))) {}
  ~AtomicReference() override = default;
  AtomicReference(AtomicReference&&) = default;
  AtomicReference& operator=(AtomicReference&&) = default;

  T Get() const& override { return absl::any_cast<T>(impl_->Get()); }
  T Get() && override { return absl::any_cast<T>(std::move(impl_->Get())); }
  void Set(const T& value) override { impl_->Set(absl::any(value)); }
  void Set(T&& value) override { impl_->Set(absl::any(value)); }

 private:
  std::unique_ptr<api::AtomicReference<absl::any>> impl_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_ATOMIC_REFERENCE_H_
