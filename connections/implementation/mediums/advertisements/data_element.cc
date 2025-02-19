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

#include "connections/implementation/mediums/advertisements/data_element.h"

#include <cstdint>
#include <optional>
#include <string>

#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"
#include "internal/platform/stream_reader.h"
#include "internal/platform/stream_writer.h"

namespace nearby::connections::advertisements {

std::optional<DataElement> DataElement::From(uint8_t type, std::string value) {
  if (type > kMaxTypeValue || type == 0 || value.size() > kMaxLengthValue) {
    LOG(WARNING) << "Invalid data element: type=" << type
                 << ", length=" << value.size();
    return std::nullopt;
  }

  DataElement data_element;
  data_element.type_ = type;
  data_element.value_ = value;
  return data_element;
}

std::optional<DataElement> DataElement::FromStreamReader(StreamReader& reader) {
  std::optional<uint8_t> is_two_bytes = reader.ReadBits(1);
  if (!is_two_bytes.has_value()) {
    LOG(WARNING) << "Failed to read type";
    return std::nullopt;
  }

  uint8_t type;
  uint8_t length;
  std::string value;

  if (*is_two_bytes) {
    // two bytes DE header
    std::optional<uint8_t> length_result = reader.ReadBits(7);
    if (!length_result.has_value()) {
      LOG(WARNING) << "Failed to read length";
      return std::nullopt;
    }
    std::optional<uint8_t> type_result = reader.ReadUint8();
    if (!type_result.has_value() || *type_result == 0) {
      LOG(WARNING) << "Failed to read type";
      return std::nullopt;
    }

    type = *type_result;
    length = *length_result;

  } else {
    // one byte DE header
    std::optional<uint8_t> length_result = reader.ReadBits(3);
    if (!length_result.has_value()) {
      LOG(WARNING) << "Failed to read length";
      return std::nullopt;
    }
    std::optional<uint8_t> type_result = reader.ReadBits(4);
    if (!type_result.has_value() || *type_result == 0) {
      LOG(WARNING) << "Failed to read type";
      return std::nullopt;
    }

    type = *type_result;
    length = *length_result;
  }

  if (length > 0) {
    std::optional<ByteArray> bytes = reader.ReadBytes(length);
    if (!bytes.has_value()) {
      LOG(WARNING) << "Failed to read bytes";
      return std::nullopt;
    }
    value = bytes->string_data();
  }

  DataElement data_element;
  data_element.type_ = type;
  data_element.value_ = value;
  return data_element;
}

std::optional<DataElement> DataElement::FromData(const std::string& data) {
  ByteArray bytes(data);
  StreamReader reader(bytes);
  return FromStreamReader(reader);
}

std::optional<std::string> DataElement::ToData() const {
  StreamWriter writer;

  if (!ToStreamWriter(writer)) {
    LOG(WARNING) << "Failed to write data element";
    return std::nullopt;
  }

  return writer.GetData();
}

bool DataElement::ToStreamWriter(StreamWriter& writer) const {
  bool is_one_byte = true;
  if (type_ > 15 || value_.size() > 7) {
    is_one_byte = false;
  }

  if (is_one_byte) {
    if (!writer.WriteBits(0, 1).Ok()) {
      LOG(WARNING) << "Failed to write is_one_byte";
      return false;
    }
    if (!writer.WriteBits(value_.size(), 3).Ok()) {
      LOG(WARNING) << "Failed to write length";
      return false;
    }
    if (!writer.WriteBits(type_, 4).Ok()) {
      LOG(WARNING) << "Failed to write type";
      return false;
    }
  } else {
    if (!writer.WriteBits(1, 1).Ok()) {
      LOG(WARNING) << "Failed to write is_one_byte";
      return false;
    }
    if (!writer.WriteBits(value_.size(), 7).Ok()) {
      LOG(WARNING) << "Failed to write length";
      return false;
    }
    if (!writer.WriteUint8(type_).Ok()) {
      LOG(WARNING) << "Failed to write type";
      return false;
    }
  }

  if (!writer.WriteBytes(value_).Ok()) {
    LOG(WARNING) << "Failed to write value";
    return false;
  }

  return true;
}

}  // namespace nearby::connections::advertisements
