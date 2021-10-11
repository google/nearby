// Copyright 2021 Google LLC
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

#ifndef PLATFORM_BASE_BYTE_ARRAY_IMPL_H_
#define PLATFORM_BASE_BYTE_ARRAY_IMPL_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>

#include "platform/base/core_config.h"

namespace location {
namespace nearby {

class ByteArrayImpl {
 public:
  // Create an empty ByteArray
  ByteArrayImpl();
  template <size_t N>
  explicit ByteArrayImpl(const std::array<char, N>& data);
  ByteArrayImpl(const ByteArrayImpl&);
  ByteArrayImpl& operator=(const ByteArrayImpl&);
  ByteArrayImpl(ByteArrayImpl&&) noexcept;
  ByteArrayImpl& operator=(ByteArrayImpl&&) noexcept;
  ~ByteArrayImpl();

  // Moves string out of temporary, allowing for a zero-copy constructions.
  // This is an optimization for very large strings.
  ByteArrayImpl(std::string&& source);

  // Create ByteArray by copy of a std::string. This can't be a string_view,
  // because it will conflict with std::string&& version of constructor.
  explicit ByteArrayImpl(const std::string& source);

  // Create default-initialized ByteArray of a given size.
  explicit ByteArrayImpl(size_t size);

  // Create value-initialized ByteArray of a given size.
  ByteArrayImpl(const char* data, size_t size);

  // Assign a new value to this ByteArray, as a copy of data, with a given size.
  void SetData(const char* data, size_t size);

  // Assign a new value of a given size to this ByteArray
  // (as a repeated char value).
  void SetData(size_t size, char value = 0);

  // Returns true, if changes were performed to container, false otherwise.
  bool CopyAt(size_t offset, const ByteArrayImpl& from,
              size_t source_offset = 0);

  char* data();
  const char* data() const;
  size_t size() const;
  bool Empty() const;

  friend bool operator==(const ByteArrayImpl& lhs, const ByteArrayImpl& rhs);
  friend bool operator!=(const ByteArrayImpl& lhs, const ByteArrayImpl& rhs);
  friend bool operator<(const ByteArrayImpl& lhs, const ByteArrayImpl& rhs);

  // Returns a copy of internal representation as std::string.
  explicit operator std::string() const&;

  // Moves string out of temporary ByteArray, allowing for a zero-copy
  // operation.
  explicit operator std::string() &&;

 private:
  std::string data_;
};

bool operator==(const ByteArrayImpl& lhs, const ByteArrayImpl& rhs);

bool operator!=(const ByteArrayImpl& lhs, const ByteArrayImpl& rhs);

bool operator<(const ByteArrayImpl& lhs, const ByteArrayImpl& rhs);

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_BYTE_ARRAY_IMPL_H_
