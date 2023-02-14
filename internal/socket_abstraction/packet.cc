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

#include "internal/socket_abstraction/packet.h"

#include <cstdint>
#include <cstring>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace socket_abstraction {

static constexpr char kMaskType = 0b10000000;
static constexpr char kMaskPacketCounter = 0b01110000;
static constexpr char kMaskControlCommandNumber = 0b00001111;
static constexpr char kFirstPacketBit = 0b00001000;
static constexpr char kLastPacketBit = 0b00000100;

Packet Packet::FromBytes(ByteArray bytes) { return Packet(bytes); }

Packet Packet::CreateDataPacket(bool isFirstPacket, bool isLastPacket,
                                ByteArray payload) {
  int nextFourBits = ((isFirstPacket ? kFirstPacketBit : 0) |
                      (isLastPacket ? kLastPacketBit : 0));
  Packet packet = FromBytes(ByteArray(1 + payload.size()));
  packet.SetHeader(false, nextFourBits);
  packet.payload_ = payload;
  packet.bytes_.CopyAt(1, payload);
  return packet;
}

Packet Packet::CreateControlPacket(int command_number, int payload_size) {
  Packet packet = FromBytes(ByteArray(1 + payload_size));
  packet.SetHeader(true, command_number);
  return packet;
}

absl::StatusOr<Packet> Packet::CreateConnectionRequestPacket(
    int16_t min_protocol_version, int16_t max_protocol_version,
    int16_t max_packet_size, ByteArray extra_data) {
  if (extra_data.size() > 13) {
    return absl::InvalidArgumentError(
        "Connection confirm packet may contain at most 13 bytes of extra "
        "data.");
  }
  Packet packet =
      CreateControlPacket(kControlConnectionRequest, 6 + extra_data.size());
  ByteArray data = ByteArray(6 + extra_data.size());
  // copy min_protocol_version as a short
  memcpy(data.data(), &min_protocol_version, sizeof(int16_t));
  // copy max_protocol_version as a short
  memcpy(data.data() + sizeof(int16_t), &max_protocol_version, sizeof(int16_t));
  // copy max_packet_size as a short
  memcpy(data.data() + (sizeof(int16_t) * 2), &max_packet_size,
         sizeof(int16_t));
  // copy extra_data
  data.CopyAt(6, extra_data);
  packet.payload_.SetData(data.data(), data.size());
  packet.bytes_.CopyAt(1, packet.payload_);
  return packet;
}

absl::StatusOr<Packet> Packet::CreateConnectionConfirmPacket(
    int16_t selected_protocol_version, int16_t selected_packet_size,
    ByteArray extra_data) {
  if (extra_data.size() > 15) {
    return absl::InvalidArgumentError(
        "Connection confirm packet may contain at most 15 bytes of extra "
        "data.");
  }
  Packet packet =
      CreateControlPacket(kControlConnectionConfirm, 4 + extra_data.size());
  ByteArray data = ByteArray(4 + extra_data.size());
  // copy selected_protocol_version as a short
  int16_t short_selected_proto_ver =
      static_cast<int16_t>(selected_protocol_version);
  memcpy(data.data(), &short_selected_proto_ver, sizeof(int16_t));
  // copy max_packet_size as a short
  int16_t short_selected_packet_size =
      static_cast<int16_t>(selected_packet_size);
  memcpy(data.data() + sizeof(int16_t), &short_selected_packet_size,
         sizeof(int16_t));
  // copy extra_data
  data.CopyAt(4, extra_data);
  packet.payload_.SetData(data.data(), data.size());
  packet.bytes_.CopyAt(1, ByteArray(data));
  return packet;
}

Packet Packet::CreateErrorPacket() {
  return CreateControlPacket(kControlError, 0);
}

void Packet::SetHeader(bool is_control_packet, int lastFourBits) {
  bytes_.data()[0] = (is_control_packet ? 0b10000000 : 0b00000000) |
                     (lastFourBits & 0b00001111);
}

Packet::Packet(ByteArray bytes) {
  bytes_ = bytes;
  std::string payload(bytes_.data() + 1, bytes_.size() - 1);
  payload_ = ByteArray(payload);
}

bool Packet::IsFirstPacket() {
  return (bytes_.data()[0] & kFirstPacketBit) != 0;
}

bool Packet::IsLastPacket() { return (bytes_.data()[0] & kLastPacketBit) != 0; }

bool Packet::IsControlPacket() { return (bytes_.data()[0] & kMaskType) != 0; }

bool Packet::IsDataPacket() { return !IsControlPacket(); }

int Packet::GetControlCommandNumber() {
  return bytes_.data()[0] & kMaskControlCommandNumber;
}

int Packet::GetPacketCounter() {
  return (bytes_.data()[0] & kMaskPacketCounter) >> 4;
}

absl::Status Packet::SetPacketCounter(int packetCounter) {
  if (packetCounter < 0 || packetCounter > kMaxPacketCounter) {
    return absl::InvalidArgumentError("packet counter out of range");
  }
  bytes_.data()[0] |= ((packetCounter << 4) & kMaskPacketCounter);
  return absl::OkStatus();
}

std::string Packet::ToString() {
  return absl::StrFormat("Packet[%08d + %d bytes payload]",
                         bytes_.data()[0] & 0xFF, payload_.size());
}

}  // namespace socket_abstraction
}  // namespace nearby
