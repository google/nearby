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

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/stream_reader.h"

namespace nearby::connections::advertisements {
namespace {
constexpr absl::string_view kFakeEncryptionKey =
    "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f";
}

// Should always match the protocol implementation to read device name.
// LINT.IfChange
std::optional<std::string> ReadDeviceName(const ByteArray& endpoint_info) {
  StreamReader reader(endpoint_info);
  std::optional<uint8_t> version = reader.ReadBits(3);
  if (!version.has_value() || *version > 1) {
    return std::nullopt;
  }

  std::optional<uint8_t> has_device_name = reader.ReadBits(1);
  if (!has_device_name.has_value() || *has_device_name == 1) {
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
}

std::string BuildEndpointInfo(const std::string& device_name) {
  std::string endpoint_info;
  endpoint_info.reserve(18 + device_name.size());
  // VERSION | HAS_DEVICE_NAME | DEVICE_TYPE
  endpoint_info.push_back(0x22);
  // salt and encrypted_metadata_key
  endpoint_info.append(kFakeEncryptionKey.data(), kFakeEncryptionKey.size());
  endpoint_info.push_back(device_name.size());
  endpoint_info.append(device_name);
  return endpoint_info;
}
// LINT.ThenChange(//depot/google3/third_party/nearby/sharing/advertisement.cc)

}  // namespace nearby::connections::advertisements
