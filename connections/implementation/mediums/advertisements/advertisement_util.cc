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

#include "connections/implementation/mediums/advertisements/advertisement_util.h"

#include <cstdint>
#include <optional>
#include <string>

#include "internal/platform/byte_array.h"
#include "internal/platform/stream_reader.h"

namespace nearby::connections::advertisements {

std::optional<std::string> ReadDeviceName(const ByteArray& endpoint_info) {
  // Should always match the protocol implementation to read device name.
  // LINT.IfChange
  StreamReader reader(endpoint_info);
  std::optional<uint8_t> version = reader.ReadBits(3);
  if (!version.has_value() || *version != 1) {
    return std::nullopt;
  }

  std::optional<uint8_t> has_device_name = reader.ReadBits(1);
  if (!has_device_name.has_value() || *has_device_name == 0) {
    return std::nullopt;
  }

  std::optional<uint8_t> device_type = reader.ReadBits(4);
  if (!device_type.has_value()) {
    return std::nullopt;
  }

  if (!reader.ReadBytes(16).has_value()) {
    return std::nullopt;
  }

  std::optional<uint8_t> device_length = reader.ReadUint8();
  if (!device_length.has_value() || *device_length == 0) {
    return std::nullopt;
  }

  std::optional<ByteArray> device_name = reader.ReadBytes(*device_length);
  if (!device_name.has_value()) {
    return std::nullopt;
  }

  return std::string(*device_name);
  // LINT.ThenChange(//depot/google3/third_party/nearby/sharing/advertisement.cc)
}

}  // namespace nearby::connections::advertisements
