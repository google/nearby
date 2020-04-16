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

#ifndef CORE_INTERNAL_BLE_ADVERTISEMENT_H_
#define CORE_INTERNAL_BLE_ADVERTISEMENT_H_

#include <cstdint>

#include "core/internal/pcp.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

// Represents the format of the Connections BLE Advertisement used in
// Advertising + Discovery.
//
// <p>[VERSION][PCP][SERVICE_ID_HASH][ENDPOINT_ID][ENDPOINT_NAME_SIZE]
//    [ENDPOINT_NAME][BLUETOOTH_MAC]
//
// <p>See go/connections-ble-advertisement for more information.
class BLEAdvertisement {
 public:
  // Versions of the BLEAdvertisement.
  struct Version {
    enum Value {
      V1 = 1,
      // Version is only allocated 3 bits in the BLEAdvertisement, so this
      // can never go beyond V7.
    };
  };

  static Ptr<BLEAdvertisement> fromBytes(
      ConstPtr<ByteArray> ble_advertisement_bytes);

  static ConstPtr<ByteArray> toBytes(Version::Value version, PCP::Value pcp,
                                     ConstPtr<ByteArray> service_id_hash,
                                     const std::string& endpoint_id,
                                     const std::string& endpoint_name,
                                     const std::string& bluetooth_mac_address);

  static const std::uint32_t kServiceIdHashLength;
  static const std::uint32_t kMinAdvertisementLength;
  // TODO(ahlee): Make sure names match for both Java and C++ implementations.
  static const std::uint32_t kMaxEndpointNameLength;

  ~BLEAdvertisement();

  Version::Value getVersion() const;
  PCP::Value getPCP() const;
  ConstPtr<ByteArray> getServiceIdHash() const;
  std::string getEndpointId() const;
  std::string getEndpointName() const;
  std::string getBluetoothMacAddress() const;

 private:
  static std::string hexBytesToColonDelimitedString(
      ConstPtr<ByteArray> hex_bytes);
  // TODO(ahlee): Rename to bluetoothMacAddressHexStringToBytes
  static ConstPtr<ByteArray> bluetoothMacAddressToHexBytes(
      const std::string& bluetooth_mac_address);
  static std::uint32_t computeEndpointNameLength(
      ConstPtr<ByteArray> ble_advertisement_bytes);
  static std::uint32_t computeAdvertisementLength(
      const std::string& endpoint_name);
  static bool isBluetoothMacAddressUnset(
      ConstPtr<ByteArray> bluetooth_mac_address_bytes);

  static const std::uint32_t kVersionAndPcpLength;
  static const std::uint32_t kEndpointIdLength;
  static const std::uint32_t kEndpointNameSizeLength;
  static const std::uint32_t kBluetoothMacAddressLength;
  static const std::uint16_t kVersionBitmask;
  static const std::uint16_t kPCPBitmask;
  static const std::uint16_t kEndpointNameLengthBitmask;

  BLEAdvertisement(Version::Value version, PCP::Value pcp,
                   ConstPtr<ByteArray> service_id_hash,
                   const std::string& endpoint_id,
                   const std::string& endpoint_name,
                   const std::string& bluetooth_mac_address);

  const Version::Value version_;
  const PCP::Value pcp_;
  ScopedPtr<ConstPtr<ByteArray> > service_id_hash_;
  const std::string endpoint_id_;
  const std::string endpoint_name_;
  const std::string bluetooth_mac_address_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_BLE_ADVERTISEMENT_H_
