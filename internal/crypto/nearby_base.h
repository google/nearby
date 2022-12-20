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

// This header file defines base utility functions needed by Nearby Sharing.
// They are from Chromium //base with no equivalence in Abseil or pre C++20
// Standard C++ Library.

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_NEARBY_BASE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_NEARBY_BASE_H_

#include <cstddef>
#include <limits>
#include <string>

#include "absl/types/span.h"

namespace nearbybase {

// Copying C++20 std::span implementation, but returns absl::Span and has no
// static extent template parameter.
template <typename T>
absl::Span<const uint8_t> as_bytes(absl::Span<T> s) noexcept {
  return {reinterpret_cast<const uint8_t*>(s.data()), sizeof(T) * s.size()};
}

// Copy of Chromium base::WriteInto.
char* WriteInto(std::string* str, size_t length_with_null);

// Copy of Chromium base::STLClearObject.
// Clears internal memory of an STL object.
// STL clear()/reserve(0) does not always free internal memory allocated
// This function uses swap/destructor to ensure the internal memory is freed.
template <class T>
void STLClearObject(T* obj) {
  T tmp;
  tmp.swap(*obj);
  // Sometimes "T tmp" allocates objects with memory (arena implementation?).
  // Hence using additional reserve(0) even if it doesn't always work.
  obj->reserve(0);
}

// Copy of Chrome base::HexEncode.
std::string HexEncode(const void* bytes, size_t size);
}  // namespace nearbybase
#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_NEARBY_BASE_H_
