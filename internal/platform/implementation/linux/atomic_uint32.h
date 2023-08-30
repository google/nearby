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

#ifndef PLATFORM_IMPL_LINUX_ATOMIC_UINT32_H_
#define PLATFORM_IMPL_LINUX_ATOMIC_UINT32_H_

#include <atomic>
#include <cstdint>
#include "internal/platform/implementation/atomic_reference.h"

namespace nearby {
namespace linux {
// A boolean value that may be updated atomically.
class AtomicUint32 : public api::AtomicUint32 {
 public:
  AtomicUint32(std::uint32_t initial_value) : atomic_uint_(initial_value) {}
  ~AtomicUint32() override = default;

  // Atomically read and return current value.
  std::uint32_t Get() const override { return atomic_uint_; };

  // Atomically exchange original value with a new one. Return previous value.
  void Set(std::uint32_t value) override { atomic_uint_ = value; };

 private:
  std::atomic_bool atomic_uint_ = false;
};
}  // namespace linux
}  // namespace nearby

#endif
