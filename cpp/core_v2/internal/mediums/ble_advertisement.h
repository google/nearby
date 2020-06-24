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

#ifndef CORE_V2_INTERNAL_MEDIUMS_BLE_ADVERTISEMENT_H_
#define CORE_V2_INTERNAL_MEDIUMS_BLE_ADVERTISEMENT_H_

#include <utility>

#include "platform_v2/base/byte_array.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// Represents the format of the Mediums Ble Advertisement used in advertising
// and discovery.
//
// [VERSION][SOCKET_VERSION][2_RESERVED_BITS][SERVICE_ID_HASH][DATA_SIZE][DATA]
//
// See go/nearby-ble-design for more information.
class BleAdvertisement {
 public:
  // Versions of the BleAdvertisement.
  enum class Version {
    kUndefined = 0,
    kV1 = 1,
    kV2 = 2,
    // Version is only allocated 3 bits in the BleAdvertisement, so this can
    // never go beyond V7.
  };

  // Versions of the BLESocket.
  enum class SocketVersion {
    kUndefined = 0,
    kV1 = 1,
    kV2 = 2,
    // SocketVersion is only allocated 3 bits in the BleAdvertisement, so this
    // can never go beyond V7.
  };

  static constexpr int kServiceIdHashLength = 3;

  BleAdvertisement() = default;
  BleAdvertisement(Version version, SocketVersion socket_version,
                   const ByteArray &service_id_hash, const ByteArray &data);
  explicit BleAdvertisement(const ByteArray &ble_advertisement_bytes);
  BleAdvertisement(const BleAdvertisement &) = default;
  BleAdvertisement &operator=(const BleAdvertisement &) = default;
  BleAdvertisement(BleAdvertisement &&) = default;
  BleAdvertisement &operator=(BleAdvertisement &&) = default;
  ~BleAdvertisement() = default;

  explicit operator ByteArray() const;
  // Operator overloads when comparing BleAdvertisement.
  bool operator==(const BleAdvertisement &rhs) const;
  bool operator<(const BleAdvertisement &rhs) const;

  bool IsValid() const { return IsSupportedVersion(version_); }
  Version GetVersion() const { return version_; }
  SocketVersion GetSocketVersion() const { return socket_version_; }
  ByteArray GetServiceIdHash() const { return service_id_hash_; }
  ByteArray &GetData() & { return data_; }
  const ByteArray &GetData() const & { return data_; }
  ByteArray &&GetData() && { return std::move(data_); }
  const ByteArray &&GetData() const && { return std::move(data_); }

 private:
  bool IsSupportedVersion(Version version) const;
  bool IsSupportedSocketVersion(SocketVersion socket_version) const;
  void SerializeDataSize(char *data_size_bytes_write_ptr,
                         size_t data_size) const;

  static constexpr int kVersionLength = 1;
  // Length of one int. Be sure to re-evaluate how we compute data size in this
  // class if this constant ever changes!
  static constexpr int kDataSizeLength = 4;
  static constexpr int kMinAdvertisementLength =
      kVersionLength + kServiceIdHashLength + kDataSizeLength;
  // The maximum length for a Gatt characteristic value is 512 bytes, so make
  // sure the entire advertisement is less than that. The data can take up
  // whatever space is remaining after the bytes preceding it.
  static constexpr int kMaxGattCharacteristicValueSize = 512;
  static constexpr int kMaxDataSize =
      kMaxGattCharacteristicValueSize - kMinAdvertisementLength;
  static constexpr int kVersionBitmask = 0x0E0;
  static constexpr int kSocketVersionBitmask = 0x01C;

  Version version_{Version::kUndefined};
  SocketVersion socket_version_{SocketVersion::kUndefined};
  ByteArray service_id_hash_;
  ByteArray data_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_BLE_ADVERTISEMENT_H_
