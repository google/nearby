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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_PACKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_PACKET_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace socket_abstraction {
class Packet {
 public:
  Packet(const Packet&) = default;
  Packet& operator=(const Packet&) = default;
  Packet(Packet&& other) = default;
  Packet& operator=(Packet&& other) = default;
  static Packet FromBytes(ByteArray bytes);
  static Packet CreateDataPacket(bool isFirstPacket, bool isLastPacket,
                                 ByteArray payload);
  static Packet CreateControlPacket(int command_number, int payload_size);
  static absl::StatusOr<Packet> CreateConnectionRequestPacket(
      int16_t min_protocol_version, int16_t max_protocol_version,
      int16_t max_packet_size, ByteArray extra_data);
  static absl::StatusOr<Packet> CreateConnectionConfirmPacket(
      int16_t selected_protocol_version, int16_t selected_packet_size,
      ByteArray extra_data);
  static Packet CreateErrorPacket();
  bool IsFirstPacket();
  bool IsLastPacket();
  bool IsControlPacket();
  bool IsDataPacket();
  int GetPacketCounter();
  int GetControlCommandNumber();
  ByteArray GetPayload() { return payload_; }
  ByteArray GetBytes() { return bytes_; }
  absl::Status SetPacketCounter(int packetCounter);
  std::string ToString();
  static constexpr int kMaxPacketCounter = 0b111;
  static constexpr int kControlConnectionRequest = 0;
  static constexpr int kControlConnectionConfirm = 1;
  static constexpr int kControlError = 2;

 private:
  explicit Packet(ByteArray bytes);
  void SetHeader(bool is_control_packet, int lastFourBits);
  ByteArray bytes_;
  ByteArray payload_;
};

}  // namespace socket_abstraction
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_PACKET_H_
