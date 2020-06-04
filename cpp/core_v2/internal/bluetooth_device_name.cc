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
#include "platform_v2/public/logging.h"

namespace location {
namespace nearby {
namespace connections {

// TODO(edwinwu): Define bitfield struct to replace pointer arithmetic for
// those bit parsing.

BluetoothDeviceName::BluetoothDeviceName(Version version, Pcp pcp,
                                         absl::string_view endpoint_id,
                                         const ByteArray& service_id_hash,
                                         absl::string_view endpoint_name) {
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
  endpoint_id_ = endpoint_id;
  service_id_hash_ = service_id_hash;
  endpoint_name_ = endpoint_name;
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

  // The upper 3 bits are supposed to be the version.
  version_ = static_cast<Version>(
      (bluetooth_device_name_bytes.data()[0] & kVersionBitmask) >> 5);
  const char* read_ptr = bluetooth_device_name_bytes.data();
  switch (version_) {
    case Version::kV1:
      // The lower 5 bits of the V1 payload are supposed to be the Pcp.
      pcp_ = static_cast<Pcp>(*read_ptr & kPcpBitmask);
      read_ptr++;
      switch (pcp_) {
        case Pcp::kP2pCluster:  // Fall through
        case Pcp::kP2pStar:     // Fall through
        case Pcp::kP2pPointToPoint: {
          // The next 32 bits are supposed to be the endpoint_id.
          endpoint_id_ = std::string(read_ptr, kEndpointIdLength);
          read_ptr += kEndpointIdLength;

          // The next 24 bits are supposed to be the service_id_hash.
          service_id_hash_ = ByteArray(read_ptr, kServiceIdHashLength);
          read_ptr += kServiceIdHashLength;

          // The next 56 bits are supposed to be reserved, and can be left
          // untouched.
          read_ptr += kReservedLength;

          // The next 8 bits are supposed to be the length of the endpoint_name.
          std::uint32_t expected_endpoint_name_length =
              static_cast<std::uint32_t>(*read_ptr &
                                         kEndpointNameLengthBitmask);
          read_ptr++;

          // Check that the stated endpoint_name_length is the same as what we
          // received (based off of the length of bluetooth_device_name_bytes).
          std::uint32_t actual_endpoint_name_length =
              kMaxBluetoothDeviceNameLength -
              bluetooth_device_name_bytes.size();
          if (actual_endpoint_name_length != expected_endpoint_name_length) {
            NEARBY_LOG(INFO,
                       "Cannot deserialize BluetoothDeviceName: expected "
                       "endpointName to be %d bytes, got %d bytes",
                       expected_endpoint_name_length,
                       actual_endpoint_name_length);

            endpoint_id_.empty();
            return;
          }

          endpoint_name_ = std::string{read_ptr, actual_endpoint_name_length};
          read_ptr += actual_endpoint_name_length;
        } break;

        default:
          // TODO(edwinwu): [ANALYTICIZE] This either represents corruption over
          // the air, or older versions of GmsCore intermingling with newer
          // ones.
          NEARBY_LOG(
              INFO,
              "Cannot deserialize BluetoothDeviceName: unsupported V1 PCP %d",
              pcp_);
          break;
      }
      break;

    default:
      // TODO(edwinwu): [ANALYTICIZE] This either represents corruption over
      // the air, or older versions of GmsCore intermingling with newer ones.
      NEARBY_LOG(
          INFO,
          "Cannot deserialize BluetoothDeviceName: unsupported Version %d",
          version_);
      break;
  }
}

BluetoothDeviceName::operator std::string() const {
  if (!IsValid()) {
    return "";
  }

  std::string usable_endpoint_name(endpoint_name_);
  if (endpoint_name_.size() > kMaxEndpointNameLength) {
    NEARBY_LOG(INFO,
               "While serializing Advertisement, truncating Endpoint Name %s "
               "(%lu bytes) down to %d bytes",
               endpoint_name_.c_str(), endpoint_name_.size(),
               kMaxEndpointNameLength);
    usable_endpoint_name.erase(kMaxEndpointNameLength);
  }

  std::string out;

  // The upper 3 bits are the Version.
  auto version_and_pcp_byte = static_cast<char>(
      (static_cast<uint32_t>(Version::kV1) << 5) & kVersionBitmask);
  // The lower 5 bits are the PCP.
  version_and_pcp_byte |=
      static_cast<char>(static_cast<uint32_t>(pcp_) & kPcpBitmask);
  // TODO(edwinwu): Change to StrCat to gain performance.
  out.reserve(kMaxBluetoothDeviceNameLength -
              (kMaxEndpointNameLength - usable_endpoint_name.length()));
  out.append(1, version_and_pcp_byte);
  out.append(endpoint_id_);
  out.append(std::string(service_id_hash_));
  ByteArray reserverdBytes{kReservedLength};
  out.append(std::string(reserverdBytes));
  out.append(1, usable_endpoint_name.size());
  out.append(usable_endpoint_name);

  return Base64Utils::Encode(ByteArray{std::move(out)});
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
