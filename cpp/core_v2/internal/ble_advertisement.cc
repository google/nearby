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

#include "core_v2/internal/ble_advertisement.h"

#include <inttypes.h>

#include "platform_v2/base/base_input_stream.h"
#include "platform_v2/public/logging.h"
#include "absl/strings/escaping.h"

namespace location {
namespace nearby {
namespace connections {

BleAdvertisement::BleAdvertisement(Version version, Pcp pcp,
                                   const ByteArray& service_id_hash,
                                   const std::string& endpoint_id,
                                   const std::string& endpoint_name,
                                   const std::string& bluetooth_mac_address) {
  if (version != Version::kV1 ||
      service_id_hash.size() != kServiceIdHashLength || endpoint_id.empty() ||
      endpoint_id.length() != kEndpointIdLength ||
      endpoint_name.length() > kMaxEndpointNameLength) {
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
  service_id_hash_ = service_id_hash;
  endpoint_id_ = endpoint_id;
  endpoint_name_ = endpoint_name;
  if (!BluetoothMacAddressHexStringToBytes(bluetooth_mac_address).Empty()) {
    bluetooth_mac_address_ = bluetooth_mac_address;
  }
}

BleAdvertisement::BleAdvertisement(const ByteArray& ble_advertisement_bytes) {
  if (ble_advertisement_bytes.Empty()) {
    NEARBY_LOG(ERROR,
               "Cannot deserialize BleAdvertisement: null bytes passed in.");
    return;
  }

  if (ble_advertisement_bytes.size() < kMinAdvertisementLength) {
    NEARBY_LOG(ERROR,
               "Cannot deserialize BleAdvertisement: expecting min %d raw "
               "bytes, got %" PRIu64,
               kMinAdvertisementLength, ble_advertisement_bytes.size());
    return;
  }

  ByteArray advertisement_bytes{ble_advertisement_bytes};
  BaseInputStream base_input_stream{advertisement_bytes};
  // The first 1 byte is supposed to be the version and pcp.
  auto version_and_pcp_byte = static_cast<char>(base_input_stream.ReadUint8());
  // The upper 3 bits are supposed to be the version.
  version_ =
      static_cast<Version>((version_and_pcp_byte & kVersionBitmask) >> 5);
  if (version_ != Version::kV1) {
    NEARBY_LOG(INFO,
               "Cannot deserialize BleAdvertisement: unsupported Version %d",
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
      NEARBY_LOG(INFO,
                 "Cannot deserialize BleAdvertisement: uunsupported V1 PCP %d",
                 pcp_);
  }

  // The next 3 bytes are supposed to be the service_id_hash.
  service_id_hash_ = base_input_stream.ReadBytes(kServiceIdHashLength);

  // The next 4 bytes are supposed to be the endpoint_id.
  endpoint_id_ = std::string{base_input_stream.ReadBytes(kEndpointIdLength)};

  // The next 1 byte are supposed to be the length of the endpoint_name.
  std::uint32_t expected_endpoint_name_length = base_input_stream.ReadUint8();

  // The next x bytes are the endpoint name. (Max length is 131 bytes).
  // Check that the stated endpoint_name_length is the same as what we
  // received.
  auto endpoint_name_bytes =
      base_input_stream.ReadBytes(expected_endpoint_name_length);
  if (endpoint_name_bytes.Empty() ||
      endpoint_name_bytes.size() != expected_endpoint_name_length) {
    NEARBY_LOG(INFO,
               "Cannot deserialize BleAdvertisement: expected "
               "endpointName to be %d bytes, got %" PRIu64,
               expected_endpoint_name_length, endpoint_name_bytes.size());

    // Clear enpoint_id for validadity.
    endpoint_id_.clear();
    return;
  }
  endpoint_name_ = std::string{endpoint_name_bytes};

  // The next 6 bytes are the bluetooth mac address.
  auto bluetooth_mac_address_bytes =
      base_input_stream.ReadBytes(kBluetoothMacAddressLength);
  // If the Bluetooth MAC Address bytes are unset or invalid, leave the
  // string empty. Otherwise, convert it to the proper colon delimited
  // format.
  if (!IsBluetoothMacAddressUnset(bluetooth_mac_address_bytes)) {
    bluetooth_mac_address_ =
        HexBytesToColonDelimitedString(bluetooth_mac_address_bytes);
  }
  base_input_stream.Close();
}

BleAdvertisement::operator ByteArray() const {
  if (!IsValid()) {
    return ByteArray();
  }

  // The first 3 bits are the Version.
  char version_and_pcp_byte =
      (static_cast<char>(version_) << 5) & kVersionBitmask;
  // The next 5 bits are the Pcp.
  version_and_pcp_byte |= static_cast<char>(pcp_) & kPcpBitmask;

  // clang-format off
  std::string out = absl::StrCat(std::string(1, version_and_pcp_byte),
                                 std::string(service_id_hash_),
                                 endpoint_id_,
                                 std::string(1, endpoint_name_.size()),
                                 endpoint_name_);
  // clang-format on

  // The next 6 bytes are the bluetooth mac address. If bluetooth_mac_address is
  // invalid or empty, we get back a null byte array.
  auto bluetooth_mac_address_bytes(
      BluetoothMacAddressHexStringToBytes(bluetooth_mac_address_));
  if (!bluetooth_mac_address_bytes.Empty()) {
    absl::StrAppend(&out, std::string(bluetooth_mac_address_bytes));
  }

  return ByteArray(std::move(out));
}

ByteArray BleAdvertisement::BluetoothMacAddressHexStringToBytes(
    const std::string& bluetooth_mac_address) const {
  std::string bt_mac_address(bluetooth_mac_address);

  // Remove the colon delimiters.
  bt_mac_address.erase(
      std::remove(bt_mac_address.begin(), bt_mac_address.end(), ':'),
      bt_mac_address.end());

  // If the bluetooth mac address is invalid (wrong size), return a null byte
  // array.
  if (bt_mac_address.length() != kBluetoothMacAddressLength * 2) {
    return ByteArray();
  }

  // Convert to bytes. If MAC Address bytes are unset, return a null byte array.
  auto bt_mac_address_string(absl::HexStringToBytes(bt_mac_address));
  auto bt_mac_address_bytes =
      ByteArray(bt_mac_address_string.data(), bt_mac_address_string.size());
  if (IsBluetoothMacAddressUnset(bt_mac_address_bytes)) {
    return ByteArray();
  }
  return bt_mac_address_bytes;
}

std::string BleAdvertisement::HexBytesToColonDelimitedString(
    const ByteArray& hex_bytes) const {
  // Convert the hex bytes to a string.
  std::string colon_delimited_string(
      absl::BytesToHexString(std::string(hex_bytes.data(), hex_bytes.size())));
  absl::AsciiStrToUpper(&colon_delimited_string);

  // Insert the colons.
  for (int i = colon_delimited_string.length() - 2; i > 0; i -= 2) {
    colon_delimited_string.insert(i, ":");
  }
  return colon_delimited_string;
}

bool BleAdvertisement::IsBluetoothMacAddressUnset(
    const ByteArray& bluetooth_mac_address_bytes) const {
  for (int i = 0; i < bluetooth_mac_address_bytes.size(); i++) {
    if (bluetooth_mac_address_bytes.data()[i] != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
