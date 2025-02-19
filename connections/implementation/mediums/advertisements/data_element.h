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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_ADVERTISEMENTS_DATA_ELEMENT_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_ADVERTISEMENTS_DATA_ELEMENT_H_

#include <cstdint>
#include <optional>
#include <string>

#include "internal/platform/stream_reader.h"
#include "internal/platform/stream_writer.h"

namespace nearby::connections::advertisements {

class DataElement {
 public:
  constexpr static uint8_t kMaxLengthValue = 0x7f;
  constexpr static uint8_t kMaxTypeValue = 0xff;

  ~DataElement() = default;

  static std::optional<DataElement> From(uint8_t type, std::string value);
  static std::optional<DataElement> FromStreamReader(StreamReader& reader);
  static std::optional<DataElement> FromData(const std::string& data);

  uint8_t type() const { return type_; }
  uint8_t length() const { return value_.size(); }
  std::string value() const { return value_; }

  std::optional<std::string> ToData() const;
  bool ToStreamWriter(StreamWriter& writer) const;

 private:
  // Each DE has a defined meaning and follows a common TLV structure.
  uint8_t type_{0};
  std::string value_;
};

}  // namespace nearby::connections::advertisements

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_ADVERTISEMENTS_DATA_ELEMENT_H_
