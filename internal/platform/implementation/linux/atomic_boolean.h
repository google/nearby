//
// Created by root on 10/2/25.
//

#ifndef LINUX_ATOMIC_BOOLEAN_H
#define LINUX_ATOMIC_BOOLEAN_H

#include  <atomic>
#include "internal/platform/implementation/atomic_boolean.h"

namespace nearby {
  namespace linux {

// A boolean value that may be updated atomically.
class AtomicBoolean : public api::AtomicBoolean {
 public:
  explicit AtomicBoolean(bool value = false) : atomic_boolean_(value) {}
  ~AtomicBoolean() override = default;

  // Atomically read and return current value.
  bool Get() const override { return atomic_boolean_; };

  // Atomically exchange original value with a new one. Return previous value.
  bool Set(bool value) override { return atomic_boolean_.exchange(value); };

 private:
  std::atomic_bool atomic_boolean_ = false;
};

  }  // namespace api
}  // namespace nearby

#endif //LINUX_ATOMIC_BOOLEAN_H