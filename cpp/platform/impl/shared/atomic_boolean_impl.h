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
