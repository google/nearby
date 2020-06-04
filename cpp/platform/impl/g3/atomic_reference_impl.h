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

#ifndef PLATFORM_IMPL_G3_ATOMIC_REFERENCE_IMPL_H_
#define PLATFORM_IMPL_G3_ATOMIC_REFERENCE_IMPL_H_

#include "platform/api/atomic_reference.h"
#include "platform/ptr.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/any.h"

namespace location {
namespace nearby {
namespace platform {

// Provide implementation for absl::any.
class AtomicReferenceImpl : public AtomicReference<absl::any> {
 public:
  explicit AtomicReferenceImpl(absl::any initial_value)
      : value_(std::move(initial_value)) {}
  ~AtomicReferenceImpl() override = default;

  absl::any get() override {
    absl::MutexLock lock(&mutex_);
    return value_;
  }
  void set(absl::any value) override {
    absl::MutexLock lock(&mutex_);
    value_ = std::move(value);
  }

 private:
  absl::Mutex mutex_;
  absl::any value_;
};

}  // namespace platform
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_G3_ATOMIC_REFERENCE_IMPL_H_
