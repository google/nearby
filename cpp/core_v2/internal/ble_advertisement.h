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

#ifndef CORE_V2_INTERNAL_BLE_ADVERTISEMENT_H_
#define CORE_V2_INTERNAL_BLE_ADVERTISEMENT_H_

#include "core_v2/internal/pcp.h"
#include "platform_v2/base/byte_array.h"

namespace location {
namespace nearby {
namespace connections {

// Represents the format of the Connections Ble Advertisement used in
// Advertising + Discovery.
//
// <p>[VERSION][PCP][SERVICE_ID_HASH][ENDPOINT_ID][ENDPOINT_NAME_SIZE]
//    [ENDPOINT_NAME][BLUETOOTH_MAC]
//
// <p>See go/connections-ble-advertisement for more information.
class BleAdvertisement {
 public:
  // Versions of the BleAdvertisement.
  enum class Version {
    kUndefined = 0,
    kV1 = 1,
    // Version is only allocated 3 bits in the BleAdvertisement, so this
    // can never go beyond V7.
  };

  static constexpr int kServiceIdHashLength = 3;
  static constexpr int kVersionAndPcpLength = 1;
  // Should be defined as EndpointManager<Platform>::kEndpointIdLength, but that
  // involves making BleAdvertisement templatized on Platform just for
  // that one little thing, so forget it (at least for now).
  static constexpr int kEndpointIdLength = 4;
  static constexpr int kEndpointNameSizeLength = 1;
  static constexpr int kBluetoothMacAddressLength = 6;
  static constexpr int kMinAdvertisementLength =
      kVersionAndPcpLength + kServiceIdHashLength + kEndpointIdLength +
      kEndpointNameSizeLength + kBluetoothMacAddressLength;
  static constexpr int kMaxEndpointNameLength = 131;
  static constexpr int kVersionBitmask = 0x0E0;
  static constexpr int kPcpBitmask = 0x01F;
  static constexpr int kEndpointNameLengthBitmask = 0x0FF;

  BleAdvertisement() = default;
  BleAdvertisement(Version version, Pcp pcp, const ByteArray& service_id_hash,
                   const std::string& endpoint_id,
                   const std::string& endpoint_name,
                   const std::string& bluetooth_mac_address);
  explicit BleAdvertisement(const ByteArray& ble_advertisement_bytes);
  BleAdvertisement(const BleAdvertisement&) = default;
  BleAdvertisement& operator=(const BleAdvertisement&) = default;
  BleAdvertisement(BleAdvertisement&&) = default;
  BleAdvertisement& operator=(BleAdvertisement&&) = default;
  ~BleAdvertisement() = default;

  explicit operator ByteArray() const;

  bool IsValid() const { return !endpoint_id_.empty(); }
  Version GetVersion() const { return version_; }
  Pcp GetPcp() const { return pcp_; }
  ByteArray GetServiceIdHash() const { return service_id_hash_; }
  std::string GetEndpointId() const { return endpoint_id_; }
  std::string GetEndpointName() const { return endpoint_name_; }
  std::string GetBluetoothMacAddress() const { return bluetooth_mac_address_; }

 private:
  ByteArray BluetoothMacAddressHexStringToBytes(
      const std::string& bluetooth_mac_address) const;
  std::string HexBytesToColonDelimitedString(const ByteArray& hex_bytes) const;
  bool IsBluetoothMacAddressUnset(
      const ByteArray& bluetooth_mac_address_bytes) const;

  Version version_ = Version::kUndefined;
  Pcp pcp_ = Pcp::kUnknown;
  ByteArray service_id_hash_;
  std::string endpoint_id_;
  std::string endpoint_name_;
  std::string bluetooth_mac_address_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_BLE_ADVERTISEMENT_H_
