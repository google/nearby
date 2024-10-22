// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_WEAVE_PACKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_WEAVE_PACKET_H_

#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace weave {

/*
Spec: go/weave-ble-gatt-transport

A weave packet has a 1 byte header indicating its packet counter, what type of
packet it is, and what type of control packet it may be.
Each control packet can also contain an optional payload, sized as follows:
Connection confirm packet: up to 15 bytes
Connection request packet: up to 13 bytes
Error packet: no payload

A data packet by contrast, is sized to fit the connection as so:
min(server_packet_size, client_packet_size, connection_packet_size).

The general format of the header of the Weave packet is as follows:
---------------------------
| 7 | 6 5 4 | 3 | 2 | 1 0 |
---------------------------
7) This bit is set to 0 if the packet is a data packet, and 1 if it is a control
packet.

6, 5, 4) These bits represent the packet counter, which wraps after every 7
packets.

3) If set to 1, this bit indicates that the packet is the first packet of the
message.

2) This bit indicates whether the packet is the last packet of the
message.

1, 0) These bits indicate the type of control packet where 0b00 is a
connection request packet, 0b01 is a connection confirm packet, and 0b10 is an
error packet. 0b11 is not a type of control packet.

If the packet is a control packet, and it is a connection request packet, the
first 6 bytes of the payload are as follows:
-------------------------------------
| 0x55 0x44 | 0x33 0x22 | 0x11 0x00 |
-------------------------------------
0x55, 0x44) The minimum protocol version the client supports.
0x33, 0x22) The maximum protocol version the client supports.
0x11, 0x00) The maximum packet size the client supports.

If the packet is a connection confirm packet, the first 4 bytes of the payload
are as follows:
-------------------------
| 0x44 0x33 | 0x22 0x11 |
-------------------------
0x44, 0x33) The selected protocol version for this connection based on the
server support.

0x22, 0x11) The selected packet size of this connection based on the server's
packet size.

*/
class Packet {
 public:
  static constexpr int kMaxPacketCounter = 0b111;
  static constexpr int kPacketHeaderLength = 1;
  enum class ControlPacketType {
    kControlConnectionRequest = 0,
    kControlConnectionConfirm = 1,
    kControlError = 2,
  };
  // For logging purposes.
  static std::string ControlPacketTypeToString(ControlPacketType type);

  // NOTE: The below Packet builders will take ownership of the bytes passed
  // into them.
  static absl::StatusOr<Packet> FromBytes(ByteArray bytes) {
    if (bytes.Empty()) {
      return absl::InvalidArgumentError(
          "Need at least one byte in this packet");
    }
    return Packet(std::move(bytes));
  }
  static Packet CreateDataPacket(bool is_first_packet, bool is_last_packet,
                                 ByteArray payload);
  static absl::StatusOr<Packet> CreateConnectionRequestPacket(
      int16_t min_protocol_version, int16_t max_protocol_version,
      int16_t max_packet_size, absl::string_view extra_data);
  static absl::StatusOr<Packet> CreateConnectionConfirmPacket(
      int16_t selected_protocol_version, int16_t selected_packet_size,
      absl::string_view extra_data);
  static Packet CreateErrorPacket() {
    return CreateControlPacket(ControlPacketType::kControlError,
                               /*payload_size=*/0);
  }

  Packet(Packet&& other) = default;
  Packet& operator=(Packet&& other) = default;

  bool IsFirstPacket() const {
    return !bytes_.empty() && (bytes_.data()[0] & kFirstPacketBit) != 0;
  }
  bool IsLastPacket() const {
    return !bytes_.empty() && (bytes_.data()[0] & kLastPacketBit) != 0;
  }
  bool IsControlPacket() const {
    return !bytes_.empty() && (bytes_.data()[0] & kMaskType) != 0;
  }
  bool IsDataPacket() const;
  int GetPacketCounter() const;
  ControlPacketType GetControlCommandNumber() const;
  std::string GetPayload() const { return bytes_.substr(kPacketHeaderLength); }
  std::string GetBytes() const { return bytes_; }
  absl::Status SetPacketCounter(int packetCounter);
  std::string ToString();

 private:
  static constexpr char kFirstPacketBit = 0b00001000;
  static constexpr char kLastPacketBit = 0b00000100;
  static constexpr char kMaskType = 0b10000000;

  static Packet CreateControlPacket(ControlPacketType command_number,
                                    int payload_size);

  explicit Packet(ByteArray&& bytes) : bytes_(std::move(bytes)) {}

  void SetHeader(bool is_control_packet, int last_four_bits);

  // Raw binary packet data.
  std::string bytes_;
};

}  // namespace weave
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_WEAVE_PACKET_H_
