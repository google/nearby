// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PLATFORM_IMPL_LINUX_ATOMIC_BOOLEAN_H_
#define PLATFORM_IMPL_LINUX_ATOMIC_BOOLEAN_H_

#include <atomic>
#include "internal/platform/implementation/atomic_boolean.h"
namespace nearby {
namespace linux {
// A boolean value that may be updated atomically.
class AtomicBoolean : public api::AtomicBoolean {
 public:
  AtomicBoolean(bool initial_value = false) : atomic_boolean_(initial_value) {}
  ~AtomicBoolean() override = default;

  // Atomically read and return current value.
  bool Get() const override { return atomic_boolean_; };

  // Atomically exchange original value with a new one. Return previous value.
  bool Set(bool value) override { return atomic_boolean_.exchange(value); };

 private:
  std::atomic_bool atomic_boolean_ = false;
};

}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_ATOMIC_BOOLEAN_H_
