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

#ifndef PLATFORM_IMPL_APPLE_ATOMIC_BOOLEAN_H_
#define PLATFORM_IMPL_APPLE_ATOMIC_BOOLEAN_H_

#include <atomic>

#include "internal/platform/implementation/atomic_boolean.h"

namespace nearby {
namespace apple {

// Concrete AtomicBoolean implementation.
class AtomicBoolean : public api::AtomicBoolean {
 public:
  explicit AtomicBoolean(bool initial_value) : value_(initial_value) {}
  ~AtomicBoolean() override = default;

  AtomicBoolean(const AtomicBoolean&) = delete;
  AtomicBoolean& operator=(const AtomicBoolean&) = delete;

  bool Get() const override { return value_.load(); }
  bool Set(bool value) override { return value_.exchange(value); }

 private:
  std::atomic_bool value_;
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_ATOMIC_BOOLEAN_H_
