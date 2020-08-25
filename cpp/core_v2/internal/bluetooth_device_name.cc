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

#include "core_v2/internal/bluetooth_device_name.h"

#include <inttypes.h>

#include <cstring>
#include <utility>

#include "platform_v2/base/base64_utils.h"
#include "platform_v2/base/base_input_stream.h"
#include "platform_v2/public/logging.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"

namespace location {
namespace nearby {
namespace connections {

BluetoothDeviceName::BluetoothDeviceName(Version version, Pcp pcp,
                                         absl::string_view endpoint_id,
                                         const ByteArray& service_id_hash,
                                         const ByteArray& endpoint_info) {
  if (version != Version::kV1 || endpoint_id.empty() ||
      endpoint_id.length() != kEndpointIdLength ||
      service_id_hash.size() != kServiceIdHashLength) {
    return;
  }
  switch (pcp) {
    case Pcp::kP2pCluster:  // Fall through
    case Pcp::kP2pStar:     // Fall through
    case Pcp::kP2pPointToPoint:
      break;
    default:
      return;
  }

  version_ = version;
  pcp_ = pcp;
  endpoint_id_ = std::string(endpoint_id);
  service_id_hash_ = service_id_hash;
  endpoint_info_ = endpoint_info;
}

BluetoothDeviceName::BluetoothDeviceName(
    absl::string_view bluetooth_device_name_string) {
  ByteArray bluetooth_device_name_bytes =
      Base64Utils::Decode(bluetooth_device_name_string);

  if (bluetooth_device_name_bytes.Empty()) {
    NEARBY_LOG(
        INFO,
        "Cannot deserialize BluetoothDeviceName: failed Base64 decoding of %s",
        std::string(bluetooth_device_name_string).c_str());
    return;
  }

  if (bluetooth_device_name_bytes.size() > kMaxBluetoothDeviceNameLength) {
    NEARBY_LOG(INFO,
               "Cannot deserialize BluetoothDeviceName: expecting max %d raw "
               "bytes, got %" PRIu64,
               kMaxBluetoothDeviceNameLength,
               bluetooth_device_name_bytes.size());
    return;
  }

  if (bluetooth_device_name_bytes.size() < kMinBluetoothDeviceNameLength) {
    NEARBY_LOG(INFO,
               "Cannot deserialize BluetoothDeviceName: expecting min %d raw "
               "bytes, got %" PRIu64,
               kMinBluetoothDeviceNameLength,
               bluetooth_device_name_bytes.size());
    return;
  }

  BaseInputStream base_input_stream{bluetooth_device_name_bytes};
  // The first 1 byte is supposed to be the version and pcp.
  auto version_and_pcp_byte = static_cast<char>(base_input_stream.ReadUint8());
  // The upper 3 bits are supposed to be the version.
  version_ =
      static_cast<Version>((version_and_pcp_byte & kVersionBitmask) >> 5);
  if (version_ != Version::kV1) {
    NEARBY_LOG(INFO,
               "Cannot deserialize BluetoothDeviceName: unsupported version=%d",
               version_);
    return;
  }
  // The lower 5 bits are supposed to be the Pcp.
  pcp_ = static_cast<Pcp>(version_and_pcp_byte & kPcpBitmask);
  switch (pcp_) {
    case Pcp::kP2pCluster:  // Fall through
    case Pcp::kP2pStar:     // Fall through
    case Pcp::kP2pPointToPoint:
      break;
    default:
      NEARBY_LOG(
          INFO, "Cannot deserialize BluetoothDeviceName: unsupported V1 PCP %d",
          pcp_);
      return;
  }

  // The next 4 bytes are supposed to be the endpoint_id.
  endpoint_id_ = std::string{base_input_stream.ReadBytes(kEndpointIdLength)};

  // The next 3 bytes are supposed to be the service_id_hash.
  service_id_hash_ = base_input_stream.ReadBytes(kServiceIdHashLength);

  // The next 7 bytes are supposed to be reserved, and can be left
  // untouched.
  base_input_stream.ReadBytes(kReservedLength);

  // The next 1 byte are supposed to be the length of the endpoint_info.
  std::uint32_t expected_endpoint_info_length = base_input_stream.ReadUint8();

  // The rest bytes are supposed to be the endpoint_info
  endpoint_info_ = base_input_stream.ReadBytes(expected_endpoint_info_length);
  if (endpoint_info_.Empty() ||
      endpoint_info_.size() != expected_endpoint_info_length) {
    NEARBY_LOG(INFO,
               "Cannot deserialize BluetoothDeviceName: expected "
               "endpoint info to be %d bytes, got %" PRIu64,
               expected_endpoint_info_length, endpoint_info_.size());

    // Clear enpoint_id for validadity.
    endpoint_id_.clear();
    return;
  }
}

BluetoothDeviceName::operator std::string() const {
  if (!IsValid()) {
    return "";
  }

  // The upper 3 bits are the Version.
  auto version_and_pcp_byte = static_cast<char>(
      (static_cast<uint32_t>(Version::kV1) << 5) & kVersionBitmask);
  // The lower 5 bits are the PCP.
  version_and_pcp_byte |=
      static_cast<char>(static_cast<uint32_t>(pcp_) & kPcpBitmask);

  ByteArray reserved_bytes{kReservedLength};

  ByteArray usable_endpoint_info(endpoint_info_);
  if (endpoint_info_.size() > kMaxEndpointInfoLength) {
    NEARBY_LOG(INFO,
               "While serializing Advertisement, truncating Endpoint Name %s "
               "(%lu bytes) down to %d bytes",
               absl::BytesToHexString(endpoint_info_.data()).c_str(),
               endpoint_info_.size(), kMaxEndpointInfoLength);
    usable_endpoint_info.SetData(endpoint_info_.data(), kMaxEndpointInfoLength);
  }

  // clang-format off
  std::string out = absl::StrCat(std::string(1, version_and_pcp_byte),
                                 endpoint_id_,
                                 std::string(service_id_hash_),
                                 std::string(reserved_bytes),
                                 std::string(1, usable_endpoint_info.size()),
                                 std::string(usable_endpoint_info));
  // clang-format on

  return Base64Utils::Encode(ByteArray{std::move(out)});
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
