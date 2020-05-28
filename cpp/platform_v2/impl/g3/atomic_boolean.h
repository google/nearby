#ifndef PLATFORM_V2_IMPL_G3_ATOMIC_BOOLEAN_H_
#define PLATFORM_V2_IMPL_G3_ATOMIC_BOOLEAN_H_

#include <atomic>

#include "platform_v2/api/atomic_boolean.h"

namespace location {
namespace nearby {
namespace g3 {

// See documentation in
// https://source.corp.google.com/piper///depot/google3/platform_v2/api/atomic_boolean.h
class AtomicBoolean : public api::AtomicBoolean {
 public:
  explicit AtomicBoolean(bool initial_value) : value_(initial_value) {}
  ~AtomicBoolean() override = default;

  bool Get() const override { return value_.load(); }
  bool Set(bool value) override { return value_.exchange(value); }

 private:
  std::atomic_bool value_;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_G3_ATOMIC_BOOLEAN_H_
