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

#include "absl/status/statusor.h"
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
  static constexpr int kServiceIdHashLength = 3;

  // Creates an Introduction frame for Control packet with the given
  // 'service_id_hash'. The size of 'service_id_hash' must equal to
  // 'kServiceIdHashLength', otherwise returns an error.
  //
  // service_id_hash - hash of client service id.
  static absl::StatusOr<BlePacket> CreateControlIntroductionPacket(
      const ByteArray& service_id_hash);

  // Creates an Disconnection frame for Control packet with the given
  // 'service_id_hash'. The size of 'service_id_hash' must equal to
  // 'kServiceIdHashLength', otherwise returns an error.
  //
  // service_id_hash - hash of client service id.
  static absl::StatusOr<BlePacket> CreateControlDisconnectionPacket(
      const ByteArray& service_id_hash);

  // Creates an Packet Acknowledgement frame for Control packet with the given
  // 'service_id_hash'. The size of 'service_id_hash' must equal to
  // 'kServiceIdHashLength', otherwise returns an error.
  //
  // service_id_hash - hash of client service id.
  // received_size - payload size received sucessfully.
  static absl::StatusOr<BlePacket> CreateControlPacketAcknowledgementPacket(
      const ByteArray& service_id_hash, int received_size);

  // Creates a Control packet. Returns error when data size has exceeded maximum
  // size.
  //
  // data - the raw bytes exported by |SocketControlFrame| that supports all the
  // |ControlFrameType|.
  static absl::StatusOr<BlePacket> CreateControlPacket(const ByteArray& data);

  // Creates a Data packet. Returns error when the size of 'service_id_hash' is
  // not equal to 'kServiceIdHashLength', or data size has exceeded maximum
  // size.
  //
  // service_id_hash : hash of client service id. It should be other than
  // 0x000000 that is reserved for Control packet.
  static absl::StatusOr<BlePacket> CreateDataPacket(
      const ByteArray& service_id_hash, const ByteArray& data);

  explicit BlePacket(const ByteArray& ble_packet_byte);
  BlePacket(const BlePacket&) = default;
  BlePacket& operator=(const BlePacket&) = default;
  BlePacket(BlePacket&&) = default;
  BlePacket& operator=(BlePacket&&) = default;
  ~BlePacket() = default;

  explicit operator ByteArray() const;

  bool IsValid() const;
  ByteArray GetServiceIdHash() const { return service_id_hash_; }
  ByteArray GetData() const { return data_; }
  int GetPacketSize() const { return data_.size() + kServiceIdHashLength; }
  bool IsControlPacket() const;

 private:
  enum class BlePacketType {
    kInvalid = 0,
    kData,
    kControl,
  };

  BlePacket() = default;

  BlePacketType packet_type_ = BlePacketType::kInvalid;
  ByteArray service_id_hash_;
  ByteArray data_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_BLE_PACKET_H_
