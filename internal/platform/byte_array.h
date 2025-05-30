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

#ifndef PLATFORM_BASE_BYTE_ARRAY_H_
#define PLATFORM_BASE_BYTE_ARRAY_H_

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace nearby {

class ByteArray {
 public:
  using iterator = std::string::iterator;
  using const_iterator = std::string::const_iterator;

  // Create an empty ByteArray
  ByteArray() = default;
  template <size_t N>
  explicit ByteArray(const std::array<char, N>& data) {
    SetData(data.data(), data.size());
  }
  ByteArray(const ByteArray&) = default;
  ByteArray& operator=(const ByteArray&) = default;
  ByteArray(ByteArray&&) = default;
  ByteArray& operator=(ByteArray&&) = default;

  // Moves string out of temporary, allowing for a zero-copy constructions.
  // This is an optimization for very large strings.
  explicit ByteArray(std::string&& source) : data_(std::move(source)) {}

  // Create ByteArray by copy of a std::string. This can't be a string_view,
  // because it will conflict with std::string&& version of constructor.
  explicit ByteArray(const std::string& source) {
    SetData(source.data(), source.size());
  }

  // Create default-initialized ByteArray of a given size.
  explicit ByteArray(size_t size) { SetData(size); }

  // Create value-initialized ByteArray of a given size.
  ByteArray(const char* data, size_t size) { SetData(data, size); }

  // Assign a new value to this ByteArray, as a copy of data, with a given size.
  void SetData(const char* data, size_t size) {
    if (data == nullptr) {
      size = 0;
    }
    data_.assign(data, size);
  }

  // Assign a new value of a given size to this ByteArray
  // (as a repeated char value).
  void SetData(size_t size, char value = 0) { data_.assign(size, value); }

  // Returns true, if changes were performed to container, false otherwise.
  bool CopyAt(size_t offset, const ByteArray& from, size_t source_offset = 0) {
    if (offset >= size()) return false;
    if (source_offset >= from.size()) return false;
    memcpy(data() + offset, from.data() + source_offset,
           std::min(size() - offset, from.size() - source_offset));
    return true;
  }

  // Returns the value of the ByteArray as a uint64_t. If the ByteArray is
  // not exactly 6 bytes, returns an error status instead.
  absl::StatusOr<uint64_t> Read6BytesAsUint64() const {
    if (data_.size() != 6) {
      return absl::FailedPreconditionError("ByteArray must be 6 bytes.");
    }

    uint64_t result = 0;
    for (size_t i = 0; i < data_.size(); i++) {
      result <<= 8;
      result |= ((static_cast<uint64_t>(data()[i])) & 0xFF);
    }

    return result;
  }

  char* data() { return &data_[0]; }
  std::string string_data() const { return data_; }
  const char* data() const { return data_.data(); }
  size_t size() const { return data_.size(); }
  bool Empty() const { return data_.empty(); }
  void resize(size_t new_size) { data_.resize(new_size); }

  // Iterators. These allow `ByteArray` to meet the requirements of
  // `std::ranges::contiguous_range`, which in turn make it implicitly
  // convertible to e.g. `std::span`.
  iterator begin() { return data_.begin(); }
  const_iterator begin() const { return data_.begin(); }
  const_iterator cbegin() const { return data_.cbegin(); }
  iterator end() { return data_.end(); }
  const_iterator end() const { return data_.end(); }
  const_iterator cend() const { return data_.cend(); }

  friend bool operator==(const ByteArray& lhs, const ByteArray& rhs);
  friend bool operator!=(const ByteArray& lhs, const ByteArray& rhs);
  friend bool operator<(const ByteArray& lhs, const ByteArray& rhs);

  // Returns a copy of internal representation as std::string.
  explicit operator std::string() const& { return data_; }

  // Moves string out of temporary ByteArray, allowing for a zero-copy
  // operation.
  explicit operator std::string() && { return std::move(data_); }

  // Returns the representation of the underlying data as a string view.
  absl::string_view AsStringView() const {
    return absl::string_view(data(), size());
  }

  // Hashable
  template <typename H>
  friend H AbslHashValue(H h, const ByteArray& m) {
    return H::combine(std::move(h), m.data_);
  }

 private:
  std::string data_;
};

inline bool operator==(const ByteArray& lhs, const ByteArray& rhs) {
  return lhs.data_ == rhs.data_;
}

inline bool operator!=(const ByteArray& lhs, const ByteArray& rhs) {
  return !(lhs == rhs);
}

inline bool operator<(const ByteArray& lhs, const ByteArray& rhs) {
  return lhs.data_ < rhs.data_;
}

}  // namespace nearby

#endif  // PLATFORM_BASE_BYTE_ARRAY_H_
