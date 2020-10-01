#ifndef PLATFORM_V2_IMPL_IOS_ATOMIC_BOOLEAN_H_
#define PLATFORM_V2_IMPL_IOS_ATOMIC_BOOLEAN_H_

#include <atomic>

#include "platform_v2/api/atomic_boolean.h"

namespace location {
namespace nearby {
namespace ios {

class AtomicBoolean : public api::AtomicBoolean {
 public:
  explicit AtomicBoolean(bool initial_value) : value_(initial_value) {}
  ~AtomicBoolean() override = default;

  bool Get() const override { return value_.load(); }
  bool Set(bool value) override { return value_.exchange(value); }

 private:
  std::atomic_bool value_;
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_IOS_ATOMIC_BOOLEAN_H_
