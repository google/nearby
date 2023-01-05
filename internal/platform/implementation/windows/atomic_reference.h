// Copyright 2020 Google LLC
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

#ifndef PLATFORM_IMPL_WINDOWS_ATOMIC_REFERENCE_H_
#define PLATFORM_IMPL_WINDOWS_ATOMIC_REFERENCE_H_

#include <atomic>

#include "internal/platform/implementation/atomic_reference.h"

namespace nearby {
namespace windows {

// Type that allows 32-bit atomic reads and writes.
class AtomicUint32 : public api::AtomicUint32 {
 public:
  ~AtomicUint32() override = default;

  // Atomically reads and returns stored value.
  std::uint32_t Get() const override { return atomic_uint32_; };

  // Atomically stores value.
  void Set(std::uint32_t value) override { atomic_uint32_ = value; }

 private:
  std::atomic_int32_t atomic_uint32_ = 0;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_ATOMIC_REFERENCE_H_
