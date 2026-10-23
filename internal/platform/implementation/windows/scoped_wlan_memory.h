// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_SCOPED_WLAN_MEMORY_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_SCOPED_WLAN_MEMORY_H_

#include <windows.h>
#include <wlanapi.h>

#include <utility>

#include "absl/base/attributes.h"

namespace nearby::windows {

// Smart pointer class for memory allocated by the WLAN API.
template <typename T>
class ScopedWlanMemory {
 public:
  explicit ScopedWlanMemory(T* ptr = nullptr) : ptr_(ptr) {}
  ~ScopedWlanMemory() {
    Reset();
  }
  ScopedWlanMemory& operator=(ScopedWlanMemory<T>&& other) {
    Reset(other.Release());
    return *this;
  }
  T* operator->() { return ptr_; }
  T* get() { return ptr_; }

  // Returns a pointer to the managed object and releases the ownership.
  ABSL_MUST_USE_RESULT T* Release() { return std::exchange(ptr_, nullptr); }

  // Used as an output parameter for functions that return a pointer as an
  // output parameter.
  // If the current managed object is not null, it will be freed.
  T** OutParam() {
    Reset();
    return &ptr_;
  }

  // Swaps a new pointer into this instance. Frees the old pointer if it is not
  // null.
  void Reset(T* new_ptr = nullptr) {
    if (ptr_ != nullptr) {
      ::WlanFreeMemory(ptr_);
    }
    ptr_ = new_ptr;
  }
  explicit operator bool() const { return ptr_ != nullptr; }

 private:
  T* ptr_;
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_SCOPED_WLAN_MEMORY_H_
