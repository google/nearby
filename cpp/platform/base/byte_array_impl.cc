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

#include "platform/base/byte_array_impl.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>

namespace location {
namespace nearby {

// Create an empty ByteArray
ByteArrayImpl::ByteArrayImpl() = default;
template <size_t N>
ByteArrayImpl::ByteArrayImpl(const std::array<char, N>& data) {
  SetData(data.data(), data.size());
}
ByteArrayImpl::ByteArrayImpl(const ByteArrayImpl&) = default;
ByteArrayImpl& ByteArrayImpl::operator=(const ByteArrayImpl&) = default;
ByteArrayImpl::ByteArrayImpl(ByteArrayImpl&&) noexcept = default;
ByteArrayImpl& ByteArrayImpl::operator=(ByteArrayImpl&&) noexcept = default;

ByteArrayImpl::~ByteArrayImpl() {}

// Moves string out of temporary, allowing for a zero-copy constructions.
// This is an optimization for very large strings.
ByteArrayImpl::ByteArrayImpl(std::string&& source) : data_(std::move(source)) {}

// Create ByteArray by copy of a std::string. This can't be a string_view,
// because it will conflict with std::string&& version of constructor.
ByteArrayImpl::ByteArrayImpl(const std::string& source) {
  SetData(source.data(), source.size());
}

// Create default-initialized ByteArray of a given size.
ByteArrayImpl::ByteArrayImpl(size_t size) { SetData(size); }

// Create value-initialized ByteArray of a given size.
ByteArrayImpl::ByteArrayImpl(const char* data, size_t size) {
  SetData(data, size);
}

// Assign a new value to this ByteArray, as a copy of data, with a given size.
void ByteArrayImpl::SetData(const char* data, size_t size) {
  if (data == nullptr) {
    size = 0;
  }
  data_.assign(data, size);
}

// Assign a new value of a given size to this ByteArray
// (as a repeated char value).
void ByteArrayImpl::SetData(size_t size, char value) {
  data_.assign(size, value);
}

// Returns true, if changes were performed to container, false otherwise.
bool ByteArrayImpl::CopyAt(size_t offset, const ByteArrayImpl& from,
                           size_t source_offset) {
  if (offset >= size()) return false;
  if (source_offset >= from.size()) return false;
  memcpy(data() + offset, from.data() + source_offset,
         std::min(size() - offset, from.size() - source_offset));
  return true;
}

char* ByteArrayImpl::data() { return &data_[0]; }
const char* ByteArrayImpl::data() const { return data_.data(); }
size_t ByteArrayImpl::size() const { return data_.size(); }
bool ByteArrayImpl::Empty() const { return data_.empty(); }

// Returns a copy of internal representation as std::string.
ByteArrayImpl::operator std::string() const& { return data_; }

// Moves string out of temporary ByteArray, allowing for a zero-copy
// operation.
ByteArrayImpl::operator std::string() && { return std::move(data_); }

bool operator==(const ByteArrayImpl& lhs, const ByteArrayImpl& rhs) {
  return lhs.data_ == rhs.data_;
}

bool operator!=(const ByteArrayImpl& lhs, const ByteArrayImpl& rhs) {
  return !(lhs == rhs);
}

bool operator<(const ByteArrayImpl& lhs, const ByteArrayImpl& rhs) {
  return lhs.data_ < rhs.data_;
}

}  // namespace nearby
}  // namespace location
