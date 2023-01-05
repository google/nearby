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

#ifndef PLATFORM_IMPL_G3_ATOMIC_REFERENCE_H_
#define PLATFORM_IMPL_G3_ATOMIC_REFERENCE_H_

#include <atomic>
#include <cstdint>

#include "internal/platform/implementation/atomic_reference.h"

namespace nearby {
namespace g3 {

class AtomicUint32 : public api::AtomicUint32 {
 public:
  explicit AtomicUint32(std::int32_t value) : value_(value) {}
  ~AtomicUint32() override = default;

  std::uint32_t Get() const override { return value_; }
  void Set(std::uint32_t value) override { value_ = value; }

 private:
  std::atomic<std::uint32_t> value_;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_ATOMIC_REFERENCE_H_
