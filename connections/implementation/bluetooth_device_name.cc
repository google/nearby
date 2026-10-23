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

#include "connections/implementation/bluetooth_device_name.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/pcp.h"
#include "connections/implementation/webrtc_state.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"
#include "internal/platform/stream_reader.h"

namespace nearby {
namespace connections {

BluetoothDeviceName::BluetoothDeviceName(Version version, Pcp pcp,
                                         absl::string_view endpoint_id,
                                         const ByteArray& service_id_hash,
                                         const ByteArray& endpoint_info,
                                         const ByteArray& uwb_address,
                                         WebRtcState web_rtc_state) {
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
  uwb_address_ = uwb_address;
  web_rtc_state_ = web_rtc_state;
  if (endpoint_info_.size() > kMaxEndpointInfoLength) {
    LOG(INFO)
        << "While constructing bluetooth device name, truncating Endpoint Info "
        << absl::BytesToHexString(std::string(endpoint_info_)) << " ("
        << endpoint_info_.size() << " bytes) down to " << kMaxEndpointInfoLength
        << " bytes";
    endpoint_info_ = ByteArray(endpoint_info_.data(), kMaxEndpointInfoLength);
  }
}

BluetoothDeviceName::BluetoothDeviceName(
    absl::string_view bluetooth_device_name_string) {
  ByteArray bluetooth_device_name_bytes =
      Base64Utils::Decode(bluetooth_device_name_string);
  if (bluetooth_device_name_bytes.Empty()) {
    return;
  }

  if (bluetooth_device_name_bytes.size() < kMinBluetoothDeviceNameLength) {
    LOG(INFO) << "Cannot deserialize BluetoothDeviceName: expecting min "
              << kMinBluetoothDeviceNameLength << " raw bytes, got "
              << bluetooth_device_name_bytes.size();
    return;
  }

  StreamReader stream_reader{&bluetooth_device_name_bytes};
  // The first 1 byte is supposed to be the version and pcp.
  auto version_and_pcp_byte = stream_reader.ReadUint8();
  if (!version_and_pcp_byte.has_value()) {
    LOG(INFO) << "Cannot deserialize BluetoothDeviceName: version_and_pcp.";
    return;
  }
  // The upper 3 bits are supposed to be the version.
  version_ =
      static_cast<Version>((*version_and_pcp_byte & kVersionBitmask) >> 5);
  if (version_ != Version::kV1) {
    LOG(INFO) << "Cannot deserialize BluetoothDeviceName: unsupported version="
              << static_cast<int>(version_);
    return;
  }
  // The lower 5 bits are supposed to be the Pcp.
  pcp_ = static_cast<Pcp>(*version_and_pcp_byte & kPcpBitmask);
  switch (pcp_) {
    case Pcp::kP2pCluster:  // Fall through
    case Pcp::kP2pStar:     // Fall through
    case Pcp::kP2pPointToPoint:
      break;
    default:
      LOG(INFO) << "Cannot deserialize BluetoothDeviceName: unsupported V1 PCP "
                << static_cast<int>(pcp_);
      return;
  }

  // The next 4 bytes are supposed to be the endpoint_id.
  auto endpoint_id_bytes = stream_reader.ReadBytes(kEndpointIdLength);
  if (!endpoint_id_bytes.has_value()) {
    LOG(INFO) << "Cannot deserialize BluetoothDeviceName: endpoint_id.";
    return;
  }
  endpoint_id_ = std::string{*endpoint_id_bytes};

  // The next 3 bytes are supposed to be the service_id_hash.
  auto service_id_hash_bytes = stream_reader.ReadBytes(kServiceIdHashLength);
  if (!service_id_hash_bytes.has_value()) {
    LOG(INFO) << "Cannot deserialize BluetoothDeviceName: service_id_hash.";
    endpoint_id_.clear();
    return;
  }

  service_id_hash_ = *service_id_hash_bytes;

  // The next 1 byte is field containing WebRtc state.
  auto field_byte = stream_reader.ReadUint8();
  if (!field_byte.has_value()) {
    LOG(INFO) << "Cannot deserialize BluetoothDeviceName: extra_field.";
    endpoint_id_.clear();
    return;
  }
  web_rtc_state_ = (*field_byte & kWebRtcConnectableFlagBitmask) == 1
                       ? WebRtcState::kConnectable
                       : WebRtcState::kUnconnectable;

  // The next 6 bytes are supposed to be reserved, and can be left
  // untouched.
  stream_reader.ReadBytes(kReservedLength);

  // The next 1 byte is supposed to be the length of the endpoint_info.
  auto expected_endpoint_info_length = stream_reader.ReadUint8();
  if (!expected_endpoint_info_length.has_value()) {
    LOG(INFO)
        << "Cannot deserialize BluetoothDeviceName: endpoint_info_length.";
    endpoint_id_.clear();
    return;
  }

  // The rest bytes are supposed to be the endpoint_info
  auto endpoint_info_bytes =
      stream_reader.ReadBytes(*expected_endpoint_info_length);
  if (!endpoint_info_bytes.has_value()) {
    LOG(INFO) << "Cannot deserialize BluetoothDeviceName: endpoint_info.";
    endpoint_id_.clear();
    return;
  }
  endpoint_info_ = *endpoint_info_bytes;
  if (endpoint_info_.size() > kMaxEndpointInfoLength) {
    LOG(INFO) << "While deserializing bluetooth device name, truncating "
                 "Endpoint Info "
              << absl::BytesToHexString(std::string(endpoint_info_)) << " ("
              << endpoint_info_.size() << " bytes) down to "
              << kMaxEndpointInfoLength << " bytes";
    endpoint_info_ = ByteArray(endpoint_info_.data(), kMaxEndpointInfoLength);
  }

  // If the input stream has extra bytes, it's for UWB address. The first byte
  // is the address length. It can be 2-byte short address or 8-byte extended
  // address.
  if (stream_reader.IsAvailable(1)) {
    // The next 1 byte is supposed to be the length of the uwb_address.
    auto expected_uwb_address_length = stream_reader.ReadUint8();
    if (!expected_uwb_address_length.has_value()) {
      LOG(INFO)
          << "Cannot deserialize BluetoothDeviceName: uwb_address_length.";
      endpoint_id_.clear();
      return;
    }

    // If the length of usb_address is not zero, then retrieve it.
    if (expected_uwb_address_length != 0) {
      auto uwb_address_bytes =
          stream_reader.ReadBytes(*expected_uwb_address_length);
      if (!uwb_address_bytes.has_value()) {
        LOG(INFO) << "Cannot deserialize BluetoothDeviceName: uwb_address.";
        endpoint_id_.clear();
        return;
      }

      uwb_address_ = *uwb_address_bytes;
    }
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

  // A byte contains WebRtcState state.
  int web_rtc_connectable_flag =
      (web_rtc_state_ == WebRtcState::kConnectable) ? 1 : 0;
  char field_byte = static_cast<char>(web_rtc_connectable_flag) &
                    kWebRtcConnectableFlagBitmask;

  ByteArray reserved_bytes{kReservedLength};

  // clang-format off
  std::string out = absl::StrCat(std::string(1, version_and_pcp_byte),
                                 endpoint_id_,
                                 std::string(service_id_hash_),
                                 std::string(1, field_byte),
                                 std::string(reserved_bytes),
                                 std::string(1, endpoint_info_.size()),
                                 std::string(endpoint_info_));
  // clang-format on

  // If UWB address is available, attach it at the end.
  if (!uwb_address_.Empty()) {
    absl::StrAppend(&out, std::string(1, uwb_address_.size()));
    absl::StrAppend(&out, std::string(uwb_address_));
  }

  return Base64Utils::Encode(ByteArray{std::move(out)});
}

}  // namespace connections
}  // namespace nearby
