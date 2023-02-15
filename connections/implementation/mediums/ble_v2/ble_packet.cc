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

#include "connections/implementation/mediums/ble_v2/ble_packet.h"

#include <limits>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "internal/platform/base_input_stream.h"
#include "internal/platform/logging.h"
#include "proto/mediums/ble_frames.pb.h"

namespace nearby {
namespace connections {
namespace mediums {

using ::location::nearby::mediums::SocketControlFrame;
using ::location::nearby::mediums::SocketVersion;

// The 3 0x00 bytes are used in the control packet to identify the packet.
constexpr char kControlPacketServiceIdHash[] = "\x00\x00\x00";
constexpr std::uint32_t kMaxDataSize =
    std::numeric_limits<int32_t>::max() - BlePacket::kServiceIdHashLength;

absl::StatusOr<BlePacket> BlePacket::CreateControlIntroductionPacket(
    const ByteArray& service_id_hash) {
  if (service_id_hash.size() != kServiceIdHashLength) {
    return absl::InvalidArgumentError("service_id_hash is incorrect.");
  }

  SocketControlFrame frame;

  frame.set_type(SocketControlFrame::INTRODUCTION);
  auto* introduction_frame = frame.mutable_introduction();
  introduction_frame->set_service_id_hash(service_id_hash.data());
  introduction_frame->set_socket_version(SocketVersion::V2);

  ByteArray frame_bytes(frame.ByteSizeLong());
  frame.SerializeToArray(frame_bytes.data(), frame_bytes.size());
  return CreateControlPacket(frame_bytes);
}

absl::StatusOr<BlePacket> BlePacket::CreateControlDisconnectionPacket(
    const ByteArray& service_id_hash) {
  if (service_id_hash.size() != kServiceIdHashLength) {
    return absl::InvalidArgumentError("service_id_hash is incorrect.");
  }
  SocketControlFrame frame;

  frame.set_type(SocketControlFrame::DISCONNECTION);
  auto* disconnection_frame = frame.mutable_disconnection();
  disconnection_frame->set_service_id_hash(service_id_hash.data());

  ByteArray frame_bytes(frame.ByteSizeLong());
  frame.SerializeToArray(frame_bytes.data(), frame_bytes.size());
  return CreateControlPacket(frame_bytes);
}

absl::StatusOr<BlePacket> BlePacket::CreateControlPacketAcknowledgementPacket(
    const ByteArray& service_id_hash, int received_size) {
  if (service_id_hash.size() != kServiceIdHashLength) {
    return absl::InvalidArgumentError("service_id_hash is incorrect.");
  }
  SocketControlFrame frame;

  frame.set_type(SocketControlFrame::PACKET_ACKNOWLEDGEMENT);
  auto* packet_acknowledgement_frame = frame.mutable_packet_acknowledgement();
  packet_acknowledgement_frame->set_service_id_hash(service_id_hash.data());
  packet_acknowledgement_frame->set_received_size(received_size);

  ByteArray frame_bytes(frame.ByteSizeLong());
  frame.SerializeToArray(frame_bytes.data(), frame_bytes.size());
  return CreateControlPacket(frame_bytes);
}

absl::StatusOr<BlePacket> BlePacket::CreateControlPacket(
    const ByteArray& data) {
  if (data.size() > kMaxDataSize) {
    return absl::InvalidArgumentError(
        absl::StrCat("Packet size: ", data.size(), " > ", kMaxDataSize));
  }

  BlePacket ble_packet;
  ble_packet.packet_type_ = BlePacketType::kControl;
  ble_packet.service_id_hash_ =
      ByteArray(kControlPacketServiceIdHash, kServiceIdHashLength);
  ble_packet.data_ = data;
  return ble_packet;
}

absl::StatusOr<BlePacket> BlePacket::CreateDataPacket(
    const ByteArray& service_id_hash, const ByteArray& data) {
  if (data.size() > kMaxDataSize) {
    return absl::InvalidArgumentError(
        absl::StrCat("Packet size: ", data.size(), " > ", kMaxDataSize));
  }
  if (service_id_hash.size() != kServiceIdHashLength ||
      service_id_hash ==
          ByteArray(kControlPacketServiceIdHash, kServiceIdHashLength)) {
    return absl::InvalidArgumentError("service_id_hash is incorrect.");
  }

  BlePacket ble_packet;
  ble_packet.packet_type_ = BlePacketType::kData;
  ble_packet.service_id_hash_ = service_id_hash;
  ble_packet.data_ = data;
  return ble_packet;
}

BlePacket::BlePacket(const ByteArray& ble_packet_bytes) {
  if (ble_packet_bytes.Empty()) {
    NEARBY_LOG(ERROR, "Cannot deserialize BlePacket: null bytes passed in");
    return;
  }

  if (ble_packet_bytes.size() < kServiceIdHashLength) {
    NEARBY_LOG(
        INFO,
        "Cannot deserialize BlePacket: expecting min %u raw bytes, got %zu",
        kServiceIdHashLength, ble_packet_bytes.size());
    return;
  }

  ByteArray packet_bytes(ble_packet_bytes);
  BaseInputStream base_input_stream{packet_bytes};
  // The first 3 bytes are supposed to be the service_id_hash.
  service_id_hash_ = base_input_stream.ReadBytes(kServiceIdHashLength);
  if (service_id_hash_ ==
      ByteArray(kControlPacketServiceIdHash, kServiceIdHashLength)) {
    packet_type_ = BlePacketType::kControl;
  } else {
    packet_type_ = BlePacketType::kData;
  }

  // The rest bytes are supposed to be the data.
  data_ = base_input_stream.ReadBytes(ble_packet_bytes.size() -
                                      kServiceIdHashLength);
}

BlePacket::operator ByteArray() const {
  if (!IsValid()) {
    return ByteArray();
  }

  std::string out =
      absl::StrCat(std::string(service_id_hash_), std::string(data_));

  return ByteArray(std::move(out));
}

bool BlePacket::IsValid() const {
  return packet_type_ != BlePacketType::kInvalid;
}

bool BlePacket::IsControlPacket() const {
  return packet_type_ == BlePacketType::kControl;
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
