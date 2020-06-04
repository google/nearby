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

#ifndef PLATFORM_V2_BASE_BYTE_ARRAY_H_
#define PLATFORM_V2_BASE_BYTE_ARRAY_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"

namespace location {
namespace nearby {

class ByteArray {
 public:
  // Create an empty ByteArray
  ByteArray() = default;
  ByteArray(const ByteArray&) = default;
  ByteArray& operator=(const ByteArray&) = default;
  ByteArray(ByteArray&&) = default;
  ByteArray& operator=(ByteArray&&) = default;

  // Create ByteArray from string.
  explicit ByteArray(absl::string_view source) { data_ = source; }

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

  char* data() { return &data_[0]; }
  const char* data() const { return data_.data(); }
  size_t size() const { return data_.size(); }
  bool Empty() const { return data_.empty(); }

  friend bool operator==(const ByteArray& lhs, const ByteArray& rhs);
  friend bool operator!=(const ByteArray& lhs, const ByteArray& rhs);
  friend bool operator<(const ByteArray& lhs, const ByteArray& rhs);

  explicit operator std::string() const { return data_; }

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
}  // namespace location

#endif  // PLATFORM_V2_BASE_BYTE_ARRAY_H_
