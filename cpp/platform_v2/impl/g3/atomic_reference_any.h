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

#ifndef PLATFORM_V2_IMPL_G3_ATOMIC_REFERENCE_ANY_H_
#define PLATFORM_V2_IMPL_G3_ATOMIC_REFERENCE_ANY_H_

#include "platform_v2/api/atomic_reference.h"
#include "absl/base/integral_types.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/any.h"

namespace location {
namespace nearby {
namespace g3 {

// Provide implementation for absl::any.
class AtomicReferenceAny : public api::AtomicReference<absl::any> {
 public:
  explicit AtomicReferenceAny(absl::any initial_value)
      : value_(std::move(initial_value)) {}
  ~AtomicReferenceAny() override = default;

  absl::any Get() const & override {
    absl::MutexLock lock(&mutex_);
    return value_;
  }
  absl::any Get() && override {
    absl::MutexLock lock(&mutex_);
    return std::move(value_);
  }
  void Set(const absl::any& value) override {
    absl::MutexLock lock(&mutex_);
    value_ = value;
  }
  void Set(absl::any&& value) override {
    absl::MutexLock lock(&mutex_);
    value_ = std::move(value);
  }

 private:
  mutable absl::Mutex mutex_;
  absl::any value_;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_G3_ATOMIC_REFERENCE_ANY_H_
