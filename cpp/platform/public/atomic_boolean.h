#ifndef PLATFORM_PUBLIC_ATOMIC_BOOLEAN_H_
#define PLATFORM_PUBLIC_ATOMIC_BOOLEAN_H_

#include <memory>

#include "platform/api/atomic_boolean.h"
#include "platform/api/platform.h"

namespace location {
namespace nearby {

// A boolean value that may be updated atomically.
// See documentation in
// https://source.corp.google.com/piper///depot/google3/platform/api/atomic_boolean.h
class AtomicBoolean final  : public api::AtomicBoolean {
 public:
  using Platform = api::ImplementationPlatform;
  explicit AtomicBoolean(bool value = false)
      : impl_(Platform::CreateAtomicBoolean(value)) {}
  ~AtomicBoolean() override = default;
  AtomicBoolean(AtomicBoolean&&) = default;
  AtomicBoolean& operator=(AtomicBoolean&&) = default;

  bool Get() const override { return impl_->Get(); }
  bool Set(bool value) override { return impl_->Set(value); }

  explicit operator bool() const { return Get(); }

 private:
  std::unique_ptr<api::AtomicBoolean> impl_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_ATOMIC_BOOLEAN_H_
