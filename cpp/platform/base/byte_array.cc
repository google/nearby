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

#include "platform/base/byte_array.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>

#include "platform/base/byte_array_impl.h"

namespace location {
namespace nearby {

// Create an empty ByteArray
ByteArray::ByteArray() { byte_array_impl_ = new ByteArrayImpl(); }
template <size_t N>
ByteArray::ByteArray(const std::array<char, N>& data) {
  byte_array_impl_ = new ByteArrayImpl(data);
}
ByteArray::ByteArray(const ByteArray& other) {
  byte_array_impl_ = new ByteArrayImpl(other.data(), other.size());
}
ByteArray& ByteArray::operator=(const ByteArray&) = default;
ByteArray::ByteArray(ByteArray&&) noexcept = default;
ByteArray& ByteArray::operator=(ByteArray&&) noexcept = default;

ByteArray::~ByteArray() = default;

// Moves string out of temporary, allowing for a
// zero-copy constructions.
// This is an optimization for very large strings.
ByteArray::ByteArray(char* source) {
  byte_array_impl_ = new ByteArrayImpl(std::move(source));
}

// Create ByteArray by copy of a std::string. This can't be a string_view,
// because it will conflict with std::string&& version of constructor.
ByteArray::ByteArray(const char* source) {
  byte_array_impl_ = new ByteArrayImpl(source);
}

// Create default-initialized ByteArray of a given size.
ByteArray::ByteArray(size_t size) {
  byte_array_impl_ = new ByteArrayImpl(size);
}

// Create value-initialized ByteArray of a given size.
ByteArray::ByteArray(const char* data, size_t size) {
  byte_array_impl_ = new ByteArrayImpl(data, size);
}

// Assign a new value to this ByteArray, as a copy of data, with a given size.
void ByteArray::SetData(const char* data, size_t size) {
  byte_array_impl_->SetData(data, size);
}

// Assign a new value of a given size to this ByteArray
// (as a repeated char value).
void ByteArray::SetData(size_t size, char value) {
  byte_array_impl_->SetData(size, value);
}

// Returns true, if changes were performed to container, false otherwise.
bool ByteArray::CopyAt(size_t offset, const ByteArray& from,
                       size_t source_offset) {
  return byte_array_impl_->CopyAt(offset, *from.byte_array_impl_,
                                  source_offset);
}

char* ByteArray::data() { return byte_array_impl_->data(); }
const char* ByteArray::data() const { return byte_array_impl_->data(); }
size_t ByteArray::size() const { return byte_array_impl_->size(); }
bool ByteArray::Empty() const { return byte_array_impl_->Empty(); }

bool operator==(const ByteArray& lhs, const ByteArray& rhs) {
  if (lhs.size() == rhs.size()) {
    return memcmp(lhs.data(), rhs.data(), rhs.size()) == 0;
  }
  return false;
}

bool operator!=(const ByteArray& lhs, const ByteArray& rhs) {
  if (lhs.size() == rhs.size()) {
    return memcmp(lhs.data(), rhs.data(), rhs.size()) != 0;
  }
  return true;
}

bool operator<(const ByteArray& lhs, const ByteArray& rhs) {
  return lhs.data() < rhs.data();
}

}  // namespace nearby
}  // namespace location
