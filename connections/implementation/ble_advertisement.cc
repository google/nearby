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

#include "connections/implementation/ble_advertisement.h"

#include <inttypes.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "connections/implementation/base_pcp_handler.h"
#include "connections/implementation/pcp.h"
#include "internal/platform/base_input_stream.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {

BleAdvertisement::BleAdvertisement(Version version, Pcp pcp,
                                   const ByteArray& service_id_hash,
                                   const std::string& endpoint_id,
                                   const ByteArray& endpoint_info,
                                   const std::string& bluetooth_mac_address,
                                   const ByteArray& uwb_address,
                                   WebRtcState web_rtc_state) {
  DoInitialize(/*fast_advertisement=*/false, version, pcp, service_id_hash,
               endpoint_id, endpoint_info, bluetooth_mac_address, uwb_address,
               web_rtc_state);
}

BleAdvertisement::BleAdvertisement(Version version, Pcp pcp,
                                   const std::string& endpoint_id,
                                   const ByteArray& endpoint_info,
                                   const ByteArray& uwb_address) {
  DoInitialize(/*fast_advertisement=*/true, version, pcp, {}, endpoint_id,
               endpoint_info, {}, uwb_address, WebRtcState::kUndefined);
}

void BleAdvertisement::DoInitialize(bool fast_advertisement, Version version,
                                    Pcp pcp, const ByteArray& service_id_hash,
                                    const std::string& endpoint_id,
                                    const ByteArray& endpoint_info,
                                    const std::string& bluetooth_mac_address,
                                    const ByteArray& uwb_address,
                                    WebRtcState web_rtc_state) {
  fast_advertisement_ = fast_advertisement;
  if (!fast_advertisement_) {
    if (service_id_hash.size() != kServiceIdHashLength) return;
  }
  int max_endpoint_info_length =
      fast_advertisement_ ? kMaxFastEndpointInfoLength : kMaxEndpointInfoLength;
  if (version != Version::kV1 || endpoint_id.empty() ||
      endpoint_id.length() != kEndpointIdLength ||
      endpoint_info.size() > max_endpoint_info_length) {
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
  endpoint_info_ = endpoint_info;
  uwb_address_ = uwb_address;
  if (!fast_advertisement_) {
    if (!BluetoothUtils::FromString(bluetooth_mac_address).Empty()) {
      bluetooth_mac_address_ = bluetooth_mac_address;
    }

    web_rtc_state_ = web_rtc_state;
  }
}

absl::StatusOr<BleAdvertisement> BleAdvertisement::CreateBleAdvertisement(
    bool fast_advertisement, const ByteArray& ble_advertisement_bytes) {
  if (ble_advertisement_bytes.Empty()) {
    return absl::InvalidArgumentError(
        "Cannot deserialize BleAdvertisement: null bytes passed in.");
  }

  int min_advertisement_length = fast_advertisement
                                     ? kMinFastAdvertisementLength
                                     : kMinAdvertisementLength;

  if (ble_advertisement_bytes.size() < min_advertisement_length) {
    return absl::InvalidArgumentError(
        absl::StrCat("Cannot deserialize BleAdvertisement: expecting min ",
                     min_advertisement_length, " raw bytes, got ",
                     ble_advertisement_bytes.size()));
  }

  ByteArray advertisement_bytes{ble_advertisement_bytes};
  BaseInputStream base_input_stream{advertisement_bytes};
  // The first 1 byte is supposed to be the version and pcp.
  auto version_and_pcp_byte = static_cast<char>(base_input_stream.ReadUint8());
  // The upper 3 bits are supposed to be the version.
  Version version =
      static_cast<Version>((version_and_pcp_byte & kVersionBitmask) >> 5);
  if (version != Version::kV1) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Cannot deserialize BleAdvertisement: unsupported Version: ", version));
  }

  // The lower 5 bits are supposed to be the Pcp.
  Pcp pcp = static_cast<Pcp>(version_and_pcp_byte & kPcpBitmask);
  switch (pcp) {
    case Pcp::kP2pCluster:  // Fall through
    case Pcp::kP2pStar:     // Fall through
    case Pcp::kP2pPointToPoint:
      break;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Cannot deserialize BleAdvertisement: unsupported V1 PCP ", pcp));
  }

  // The next 3 bytes are supposed to be the service_id_hash if not fast
  // advertisement.
  ByteArray service_id_hash;
  if (!fast_advertisement) {
    service_id_hash = base_input_stream.ReadBytes(kServiceIdHashLength);
  }

  // The next 4 bytes are supposed to be the endpoint_id.
  std::string endpoint_id =
      std::string{base_input_stream.ReadBytes(kEndpointIdLength)};

  // The next 1 byte is supposed to be the length of the endpoint_info.
  auto expected_endpoint_info_length = base_input_stream.ReadUint8();

  // The next x bytes are the endpoint info. (Max length is 131 bytes or 17
  // bytes as fast_advertisement being true).
  auto endpoint_info =
      base_input_stream.ReadBytes(expected_endpoint_info_length);
  const int max_endpoint_info_length =
      fast_advertisement ? kMaxFastEndpointInfoLength : kMaxEndpointInfoLength;
  if (endpoint_info.Empty() ||
      endpoint_info.size() != expected_endpoint_info_length ||
      endpoint_info.size() > max_endpoint_info_length) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Cannot deserialize BleAdvertisement(fast advertisement=",
        fast_advertisement, "): expected endpointInfo to be ",
        expected_endpoint_info_length, " bytes, got ", endpoint_info.size()));
  }

  // The next 6 bytes are the bluetooth mac address if not fast advertisement.
  std::string bluetooth_mac_address;
  if (!fast_advertisement) {
    auto bluetooth_mac_address_bytes =
        base_input_stream.ReadBytes(BluetoothUtils::kBluetoothMacAddressLength);
    bluetooth_mac_address =
        BluetoothUtils::ToString(bluetooth_mac_address_bytes);
  }

  // The next 1 byte is supposed to be the length of the uwb_address. If the
  // next byte is not available then it should be a fast advertisement and skip
  // it for remaining bytes.
  ByteArray uwb_address;
  BleAdvertisement ble_advertisement;
  if (base_input_stream.IsAvailable(1)) {
    auto expected_uwb_address_length = base_input_stream.ReadUint8();
    // If the length of uwb_address is not zero, then retrieve it.
    if (expected_uwb_address_length != 0) {
      uwb_address = base_input_stream.ReadBytes(expected_uwb_address_length);
      if (uwb_address.Empty() ||
          uwb_address.size() != expected_uwb_address_length) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Cannot deserialize BleAdvertisement: expected uwbAddress size to "
            "be ",
            expected_uwb_address_length, " bytes, got ", uwb_address.size()));
      }
    }

    // The next 1 byte is extra field.
    if (!fast_advertisement) {
      if (base_input_stream.IsAvailable(kExtraFieldLength)) {
        auto extra_field = static_cast<char>(base_input_stream.ReadUint8());
        ble_advertisement.web_rtc_state_ =
            (extra_field & kWebRtcConnectableFlagBitmask) == 1
                ? WebRtcState::kConnectable
                : WebRtcState::kUnconnectable;
      }
    }
  }

  base_input_stream.Close();

  ble_advertisement.fast_advertisement_ = fast_advertisement;
  ble_advertisement.version_ = version;
  ble_advertisement.pcp_ = pcp;
  ble_advertisement.service_id_hash_ = service_id_hash;
  ble_advertisement.endpoint_id_ = endpoint_id;
  ble_advertisement.endpoint_info_ = endpoint_info;
  ble_advertisement.bluetooth_mac_address_ = bluetooth_mac_address;
  ble_advertisement.uwb_address_ = uwb_address;
  return ble_advertisement;
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

  std::string out;
  if (fast_advertisement_) {
    // clang-format off
    out = absl::StrCat(std::string(1, version_and_pcp_byte),
                       endpoint_id_,
                       std::string(1, endpoint_info_.size()),
                       std::string(endpoint_info_));
    // clang-format on
  } else {
    // clang-format off
    out = absl::StrCat(std::string(1, version_and_pcp_byte),
                       std::string(service_id_hash_),
                       endpoint_id_,
                       std::string(1, endpoint_info_.size()),
                       std::string(endpoint_info_));
    // clang-format on

    // The next 6 bytes are the bluetooth mac address. If bluetooth_mac_address
    // is invalid or empty, we get back an empty byte array.
    auto bluetooth_mac_address_bytes{
        BluetoothUtils::FromString(bluetooth_mac_address_)};
    if (!bluetooth_mac_address_bytes.Empty()) {
      absl::StrAppend(&out, std::string(bluetooth_mac_address_bytes));
    } else {
      // If bluetooth MAC address is invalid, then reserve the bytes.
      auto fake_bt_mac_address_bytes =
          ByteArray(BluetoothUtils::kBluetoothMacAddressLength);
      absl::StrAppend(&out, std::string(fake_bt_mac_address_bytes));
    }
  }

  // The next bytes are UWB address field.
  if (!uwb_address_.Empty()) {
    absl::StrAppend(&out, std::string(1, uwb_address_.size()));
    absl::StrAppend(&out, std::string(uwb_address_));
  } else if (!fast_advertisement_) {
    // Write UWB address with length 0 to be able to read the next field when
    // decode.
    absl::StrAppend(&out, std::string(1, uwb_address_.size()));
  }

  // The next 1 byte is extra field.
  if (!fast_advertisement_) {
    int web_rtc_connectable_flag =
        (web_rtc_state_ == WebRtcState::kConnectable) ? 1 : 0;
    char extra_field_byte = static_cast<char>(web_rtc_connectable_flag) &
                            kWebRtcConnectableFlagBitmask;
    absl::StrAppend(&out, std::string(1, extra_field_byte));
  }

  return ByteArray(std::move(out));
}

}  // namespace connections
}  // namespace nearby
