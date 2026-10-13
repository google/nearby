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

#ifndef PLATFORM_PUBLIC_ATOMIC_REFERENCE_H_
#define PLATFORM_PUBLIC_ATOMIC_REFERENCE_H_

#include <memory>
#include <type_traits>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/atomic_reference.h"
#include "internal/platform/implementation/platform.h"

namespace nearby {

// An object reference that may be updated atomically.
template <typename, typename = void>
class AtomicReference;

// Platform-based atomic type, for something convertible to std::uint32_t.
template <typename T>
class AtomicReference<T, std::enable_if_t<sizeof(T) <= sizeof(std::uint32_t) &&
                                          std::is_trivially_copyable<T>::value>>
    final {
 public:
  using Platform = api::ImplementationPlatform;
  explicit AtomicReference(T value)
      : impl_(Platform::CreateAtomicUint32(static_cast<std::uint32_t>(value))) {
  }
  ~AtomicReference() = default;
  AtomicReference(AtomicReference&&) = default;
  AtomicReference& operator=(AtomicReference&&) = default;

  T Get() const { return static_cast<T>(impl_->Get()); }
  void Set(T value) { impl_->Set(static_cast<std::uint32_t>(value)); }

 private:
  std::unique_ptr<api::AtomicUint32> impl_;
};

// Atomic type that is using Platform mutex to provide atomicity.
// Supports any copyable type.
template <typename T>
class AtomicReference<T,
                      std::enable_if_t<(sizeof(T) > sizeof(std::uint32_t) ||
                                        !std::is_trivially_copyable<T>::value)>>
    final {
 public:
  explicit AtomicReference(T value) {
    absl::MutexLock lock(mutex_);
    value_ = std::move(value);
  }
  void Set(T value) {
    absl::MutexLock lock(mutex_);
    value_ = std::move(value);
  }
  T Get() const& {
    absl::MutexLock lock(mutex_);
    return value_;
  }
  T&& Get() const&& {
    absl::MutexLock lock(mutex_);
    return std::move(value_);
  }

 private:
  mutable absl::Mutex mutex_;
  T value_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_ATOMIC_REFERENCE_H_
