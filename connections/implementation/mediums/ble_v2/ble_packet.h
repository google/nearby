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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_V2_BLE_PACKET_H_
#define CORE_INTERNAL_MEDIUMS_BLE_V2_BLE_PACKET_H_

#include <limits>

#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace mediums {

// Represents the format of data sent over Ble sockets.
//
// [SERVICE_ID_HASH][DATA]
//
// See go/nearby-ble-design for more information.
class BlePacket {
 public:
  static const std::uint32_t kServiceIdHashLength = 3;

  BlePacket() = default;
  BlePacket(const ByteArray& service_id_hash, const ByteArray& data);
  explicit BlePacket(const ByteArray& ble_packet_byte);
  BlePacket(const BlePacket&) = default;
  BlePacket& operator=(const BlePacket&) = default;
  BlePacket(BlePacket&&) = default;
  BlePacket& operator=(BlePacket&&) = default;
  ~BlePacket() = default;

  explicit operator ByteArray() const;

  bool IsValid() const { return !service_id_hash_.Empty(); }
  ByteArray GetServiceIdHash() const { return service_id_hash_; }
  ByteArray GetData() const { return data_; }

 private:
  static const std::uint32_t kMaxDataSize =
      std::numeric_limits<int32_t>::max() - kServiceIdHashLength;

  ByteArray service_id_hash_;
  ByteArray data_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_BLE_PACKET_H_
