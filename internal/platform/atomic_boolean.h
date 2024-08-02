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

#ifndef PLATFORM_PUBLIC_ATOMIC_BOOLEAN_H_
#define PLATFORM_PUBLIC_ATOMIC_BOOLEAN_H_

#include <memory>

#include "internal/platform/implementation/atomic_boolean.h"
#include "internal/platform/implementation/platform.h"

namespace nearby {

// A boolean value that may be updated atomically.
// See documentation in
// cpp/platform/api/atomic_boolean.h
class AtomicBoolean final : public api::AtomicBoolean {
 public:
  explicit AtomicBoolean(bool value = false)
      : impl_(api::ImplementationPlatform::CreateAtomicBoolean(value)) {}
  ~AtomicBoolean() override = default;
  AtomicBoolean(AtomicBoolean&&) = default;
  AtomicBoolean& operator=(AtomicBoolean&&) = default;

  bool Get() const override { return impl_->Get(); }
  bool Set(bool value) override { return impl_->Set(value); }

  explicit operator bool() const { return Get(); }

 private:
  std::unique_ptr<api::AtomicBoolean> impl_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_ATOMIC_BOOLEAN_H_
