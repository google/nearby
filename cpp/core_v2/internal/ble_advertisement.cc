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
                                   const ByteArray& endpoint_info,
                                   const std::string& bluetooth_mac_address) {
  DoInitialize(/*fast_advertisement=*/false, version, pcp, service_id_hash,
               endpoint_id, endpoint_info, bluetooth_mac_address);
}

BleAdvertisement::BleAdvertisement(Version version, Pcp pcp,
                                   const std::string& endpoint_id,
                                   const ByteArray& endpoint_info) {
  DoInitialize(/*fast_advertisement=*/true, version, pcp, {}, endpoint_id,
               endpoint_info, {});
}

void BleAdvertisement::DoInitialize(bool fast_advertisement, Version version,
                                    Pcp pcp, const ByteArray& service_id_hash,
                                    const std::string& endpoint_id,
                                    const ByteArray& endpoint_info,
                                    const std::string& bluetooth_mac_address) {
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
  if (!fast_advertisement_) {
    if (!BluetoothUtils::FromString(bluetooth_mac_address).Empty()) {
      bluetooth_mac_address_ = bluetooth_mac_address;
    }
  }
}

BleAdvertisement::BleAdvertisement(bool fast_advertisement,
                                   const ByteArray& ble_advertisement_bytes) {
  fast_advertisement_ = fast_advertisement;

  if (ble_advertisement_bytes.Empty()) {
    NEARBY_LOG(ERROR,
               "Cannot deserialize BleAdvertisement: null bytes passed in.");
    return;
  }

  int min_advertisement_length = fast_advertisement_
                                     ? kMinFastAdvertisementLength
                                     : kMinAdvertisementLength;

  if (ble_advertisement_bytes.size() < min_advertisement_length) {
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

  // The next 3 bytes are supposed to be the service_id_hash if not fast
  // advertisment.
  if (!fast_advertisement_)
    service_id_hash_ = base_input_stream.ReadBytes(kServiceIdHashLength);

  // The next 4 bytes are supposed to be the endpoint_id.
  endpoint_id_ = std::string{base_input_stream.ReadBytes(kEndpointIdLength)};

  // The next 1 byte are supposed to be the length of the endpoint_info.
  std::uint32_t expected_endpoint_info_length = base_input_stream.ReadUint8();

  // The next x bytes are the endpoint info. (Max length is 131 bytes or 17
  // bytes as fast_advertisement being true).
  endpoint_info_ = base_input_stream.ReadBytes(expected_endpoint_info_length);
  const int max_endpoint_info_length =
      fast_advertisement_ ? kMaxFastEndpointInfoLength : kMaxEndpointInfoLength;
  if (endpoint_info_.Empty() ||
      endpoint_info_.size() != expected_endpoint_info_length ||
      endpoint_info_.size() > max_endpoint_info_length) {
    NEARBY_LOG(INFO,
               "Cannot deserialize BleAdvertisement(fast advertisement=%d): "
               "expected endpointInfo to be %d bytes, got %" PRIu64,
               fast_advertisement_, expected_endpoint_info_length,
               endpoint_info_.size());

    // Clear enpoint_id for validadity.
    endpoint_id_.clear();
    return;
  }

  // The next 6 bytes are the bluetooth mac address if not fast advertisment.
  if (!fast_advertisement_) {
    auto bluetooth_mac_address_bytes =
        base_input_stream.ReadBytes(BluetoothUtils::kBluetoothMacAddressLength);
    bluetooth_mac_address_ =
        BluetoothUtils::ToString(bluetooth_mac_address_bytes);
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
    // is invalid or empty, we get back a null byte array.
    auto bluetooth_mac_address_bytes{
        BluetoothUtils::FromString(bluetooth_mac_address_)};
    if (!bluetooth_mac_address_bytes.Empty()) {
      absl::StrAppend(&out, std::string(bluetooth_mac_address_bytes));
    }
  }

  return ByteArray(std::move(out));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
