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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_ADVERTISEMENT_H_
#define CORE_INTERNAL_MEDIUMS_BLE_ADVERTISEMENT_H_

#include "platform/byte_array.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// Represents the format of the Mediums BLE Advertisement used in advertising
// and discovery.
//
// [VERSION][SOCKET_VERSION][2_RESERVED_BITS][SERVICE_ID_HASH][DATA_SIZE][DATA]
//
// See go/nearby-ble-design for more information.
class BLEAdvertisement {
 public:
  // Versions of the BLEAdvertisement.
  struct Version {
    enum Value {
      UNKNOWN = 0,
      V1 = 1,
      V2 = 2,
      // Version is only allocated 3 bits in the BLEAdvertisement, so this can
      // never go beyond V7.
    };
  };

  // Versions of the BLESocket.
  struct SocketVersion {
    enum Value {
      UNKNOWN = 0,
      V1 = 1,
      V2 = 2,
      // SocketVersion is only allocated 3 bits in the BLEAdvertisement, so this
      // can never go beyond V7.
    };
  };

  static ConstPtr<BLEAdvertisement> fromBytes(
      ConstPtr<ByteArray> ble_advertisement_bytes);

  static ConstPtr<ByteArray> toBytes(Version::Value version,
                                     SocketVersion::Value socket_version,
                                     ConstPtr<ByteArray> service_id_hash,
                                     ConstPtr<ByteArray> data);

  static const std::uint32_t kServiceIdHashLength;

  ~BLEAdvertisement();

  Version::Value getVersion() const;
  SocketVersion::Value getSocketVersion() const;
  ConstPtr<ByteArray> getServiceIdHash() const;
  ConstPtr<ByteArray> getData() const;

  // Operator overloads when comparing ConstPtr<BLEAdvertisement>.
  bool operator==(const BLEAdvertisement &rhs) const;
  bool operator<(const BLEAdvertisement &rhs) const;

 private:
  static bool isSupportedVersion(Version::Value version);
  static bool isSupportedSocketVersion(SocketVersion::Value socket_version);
  static Version::Value parseVersionFromByte(std::uint16_t byte);
  static SocketVersion::Value parseSocketVersionFromByte(std::uint16_t byte);
  static size_t deserializeDataSize(const char *data_size_bytes_read_ptr);
  static size_t computeDataSize(ConstPtr<ByteArray> ble_advertisement_bytes);
  static size_t computeAdvertisementLength(ConstPtr<ByteArray> data);
  static void serializeVersionByte(char *version_byte_write_ptr,
                                   Version::Value version);
  static void serializeSocketVersionByte(char *socket_version_byte_write_ptr,
                                         SocketVersion::Value socket_version);
  static void serializeDataSize(char *data_size_bytes_write_ptr,
                                size_t data_size);

  static const std::uint32_t kVersionLength;
  static const std::uint32_t kDataSizeLength;
  static const std::uint32_t kMinAdvertisementLength;
  static const std::uint32_t kMaxDataSize;
  static const std::uint16_t kVersionBitmask;
  static const std::uint16_t kSocketVersionBitmask;

  BLEAdvertisement(Version::Value version, SocketVersion::Value socket_version,
                   ConstPtr<ByteArray> service_id_hash,
                   ConstPtr<ByteArray> data);

  const Version::Value version_;
  const SocketVersion::Value socket_version_;
  ScopedPtr<ConstPtr<ByteArray> > service_id_hash_;
  ScopedPtr<ConstPtr<ByteArray> > data_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_BLE_ADVERTISEMENT_H_
