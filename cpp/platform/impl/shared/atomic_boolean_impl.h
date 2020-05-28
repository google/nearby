#ifndef PLATFORM_IMPL_SHARED_ATOMIC_BOOLEAN_IMPL_H_
#define PLATFORM_IMPL_SHARED_ATOMIC_BOOLEAN_IMPL_H_

#include <atomic>

#include "platform/api/atomic_boolean.h"

namespace location {
namespace nearby {

class AtomicBooleanImpl : public AtomicBoolean {
 public:
  explicit AtomicBooleanImpl(bool initial_value) : value_(initial_value) {}
  ~AtomicBooleanImpl() override = default;

  // AtomicBoolean:
  bool get() override {
    return value_.load();
  }

  // AtomicBoolean:
  void set(bool value) override {
    value_.store(value);
  }

 private:
  std::atomic_bool value_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_SHARED_ATOMIC_BOOLEAN_IMPL_H_
