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

#include "internal/weave/packet.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace weave {

namespace {
constexpr char kControlType = 0b10000000;
constexpr char kDataType = 0b00000000;
constexpr char kMaskPacketCounter = 0b01110000;
constexpr char kMaskControlCommandNumber = 0b00001111;
constexpr int kConnectionRequestExtraDataSize = 13;
constexpr int kConnectionConfirmExtraDataSize = 15;
constexpr int kConnectionConfirmPacketHeaderLength = 4;
constexpr int kConnectionRequestPacketHeaderLength = 6;
}  // namespace

Packet Packet::CreateDataPacket(bool is_first_packet, bool is_last_packet,
                                ByteArray payload) {
  int next_four_bits = ((is_first_packet ? kFirstPacketBit : 0) |
                        (is_last_packet ? kLastPacketBit : 0));
  Packet packet = Packet(ByteArray(kPacketHeaderLength + payload.size()));
  packet.SetHeader(/* is_control_packet = */ false, next_four_bits);
  payload.AsStringView().copy(packet.bytes_.data() + kPacketHeaderLength,
                              payload.size());
  return packet;
}

Packet Packet::CreateControlPacket(ControlPacketType command_number,
                                   int payload_size) {
  Packet packet = Packet(ByteArray(kPacketHeaderLength + payload_size));
  packet.SetHeader(/* is_control_packet = */ true,
                   static_cast<int>(command_number));
  return packet;
}

absl::StatusOr<Packet> Packet::CreateConnectionRequestPacket(
    int16_t min_protocol_version, int16_t max_protocol_version,
    int16_t max_packet_size, absl::string_view extra_data) {
  if (extra_data.size() > kConnectionRequestExtraDataSize) {
    return absl::InvalidArgumentError(
        "Connection request packet may contain at most 13 bytes of extra "
        "data.");
  }
  Packet packet = CreateControlPacket(
      ControlPacketType::kControlConnectionRequest,
      kConnectionRequestPacketHeaderLength + extra_data.size());

  // Create a string of length |kConnectionRequestPacketHeaderLength| filled
  // with char 0. This string is used to store raw binary data.
  std::string header_data(kConnectionRequestPacketHeaderLength, 0);

  // Put min_protocol_version into the header in big-endian order.
  header_data[0] = (0xFF & (min_protocol_version >> 8));
  header_data[1] = (0xFF & min_protocol_version);

  // Put max_protocol_version into the header in big-endian order.
  header_data[2] = (0xFF & (max_protocol_version >> 8));
  header_data[3] = (0xFF & max_protocol_version);

  // Put max_packet_size into the header in big-endian order.
  header_data[4] = (0xFF & (max_packet_size >> 8));
  header_data[5] = (0xFF & max_packet_size);

  // Finally, move to packet.
  header_data.copy(packet.bytes_.data() + kPacketHeaderLength,
                   header_data.size());

  // Copy extra_data.
  extra_data.copy(packet.bytes_.data() + kConnectionRequestPacketHeaderLength +
                      kPacketHeaderLength,
                  extra_data.size());
  return packet;
}

absl::StatusOr<Packet> Packet::CreateConnectionConfirmPacket(
    int16_t selected_protocol_version, int16_t selected_packet_size,
    absl::string_view extra_data) {
  if (extra_data.size() > kConnectionConfirmExtraDataSize) {
    return absl::InvalidArgumentError(
        "Connection confirm packet may contain at most 15 bytes of extra "
        "data.");
  }
  Packet packet = CreateControlPacket(
      ControlPacketType::kControlConnectionConfirm,
      kConnectionConfirmPacketHeaderLength + extra_data.size());
  // Create a string of length |kConnectionConfirmPacketHeaderLength| filled
  // with char 0. This string is used to store raw binary data.
  std::string header_data(kConnectionConfirmPacketHeaderLength, 0);

  // Put selected_protocol_version into the header in big-endian order.
  header_data[0] = (0xFF & (selected_protocol_version >> 8));
  header_data[1] = (0xFF & selected_protocol_version);

  // Put selected_packet_size into the header in big-endian order.
  header_data[2] = (0xFF & (selected_packet_size >> 8));
  header_data[3] = (0xFF & selected_packet_size);

  // Finally, move to packet.
  header_data.copy(packet.bytes_.data() + kPacketHeaderLength,
                   header_data.size());

  // Copy extra_data.
  extra_data.copy(packet.bytes_.data() + kConnectionConfirmPacketHeaderLength +
                      kPacketHeaderLength,
                  extra_data.size());
  return packet;
}

void Packet::SetHeader(bool is_control_packet, int last_four_bits) {
  bytes_[0] = (is_control_packet ? kControlType : kDataType) |
              (last_four_bits & 0b00001111);
}

bool Packet::IsDataPacket() const { return !IsControlPacket(); }

Packet::ControlPacketType Packet::GetControlCommandNumber() const {
  return (ControlPacketType)(bytes_[0] & kMaskControlCommandNumber);
}

int Packet::GetPacketCounter() const {
  if (bytes_.empty()) return 0;
  return (bytes_[0] & kMaskPacketCounter) >> 4;
}

absl::Status Packet::SetPacketCounter(int packetCounter) {
  if (packetCounter < 0 || packetCounter > kMaxPacketCounter) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Packet counter %d out of range", packetCounter));
  }
  bytes_[0] |= ((packetCounter << 4) & kMaskPacketCounter);
  return absl::OkStatus();
}

std::string Packet::ToString() {
  return absl::StrFormat("Packet[header: 0b%08d + payload: %d bytes]",
                         bytes_.front(), bytes_.size() - 1);
}

std::string Packet::ControlPacketTypeToString(ControlPacketType type) {
  switch (type) {
    case ControlPacketType::kControlConnectionRequest:
      return "[ConnectionRequest]";
    case ControlPacketType::kControlConnectionConfirm:
      return "[ConnectionConfirm]";
    case ControlPacketType::kControlError:
      return "[Error]";
  }
}

}  // namespace weave
}  // namespace nearby
