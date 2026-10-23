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

#include "connections/implementation/mediums/ble/ble_packet.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"
#include "internal/platform/stream_reader.h"
#include "proto/mediums/ble_frames.pb.h"

namespace nearby {
namespace connections {
namespace mediums {

using ::location::nearby::mediums::SocketControlFrame;
using ::location::nearby::mediums::SocketVersion;

// The 3 0x00 bytes are used in the control packet to identify the packet.
constexpr char kControlPacketBytes[] = "\x00\x00\x00";
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
  ble_packet.ParseControlPacketData(data.AsStringView());
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
      service_id_hash == ByteArray(kControlPacketBytes, kServiceIdHashLength)) {
    return absl::InvalidArgumentError("service_id_hash is incorrect.");
  }

  BlePacket ble_packet;
  ble_packet.packet_type_ = BlePacketType::kData;
  ble_packet.service_id_hash_ = service_id_hash;
  ble_packet.data_ = data;
  return ble_packet;
}

bool BlePacket::IsControlPacketBytes(const ByteArray& packet_bytes) {
  return packet_bytes == ByteArray(kControlPacketBytes, kServiceIdHashLength);
}

BlePacket::BlePacket(const ByteArray& ble_packet_bytes) {
  if (ble_packet_bytes.Empty()) {
    LOG(WARNING) << "Cannot deserialize BlePacket: null bytes passed in";
    return;
  }

  if (ble_packet_bytes.size() < kServiceIdHashLength) {
    LOG(WARNING) << "Cannot deserialize BlePacket: expecting min "
                 << kServiceIdHashLength << " raw bytes, got "
                 << ble_packet_bytes.size();
    return;
  }

  ByteArray packet_bytes(ble_packet_bytes);
  StreamReader stream_reader{&packet_bytes};
  // The first 3 bytes are supposed to be the service_id_hash.
  auto service_id_hash_bytes = stream_reader.ReadBytes(kServiceIdHashLength);
  if (!service_id_hash_bytes.has_value()) {
    LOG(WARNING) << "Cannot deserialize BlePacket: service_id_hash.";
    return;
  }

  service_id_hash_ = *service_id_hash_bytes;
  if (IsControlPacketBytes(service_id_hash_)) {
    packet_type_ = BlePacketType::kControl;
  } else {
    packet_type_ = BlePacketType::kData;
  }

  // The rest bytes are supposed to be the data.
  auto data_bytes =
      stream_reader.ReadBytes(ble_packet_bytes.size() - kServiceIdHashLength);
  if (!data_bytes.has_value()) {
    packet_type_ = BlePacketType::kInvalid;
    LOG(WARNING) << "Cannot deserialize BlePacket: data.";
    return;
  }

  data_ = *data_bytes;
  if (packet_type_ == BlePacketType::kControl) {
    ParseControlPacketData(data_.AsStringView());
  }
}

BlePacket::operator ByteArray() const {
  if (!IsValid()) {
    return ByteArray();
  }

  std::string out = absl::StrCat(
      IsControlPacket() ? absl::string_view(kControlPacketBytes,
                                            sizeof(kControlPacketBytes) - 1)
                        : service_id_hash_.AsStringView(),
      data_.AsStringView());

  return ByteArray(std::move(out));
}

bool BlePacket::IsValid() const {
  return packet_type_ != BlePacketType::kInvalid;
}

bool BlePacket::IsControlPacket() const {
  return packet_type_ == BlePacketType::kControl;
}

void BlePacket::ParseControlPacketData(absl::string_view data) {
  SocketControlFrame socket_control_frame;
  if (!socket_control_frame.ParseFromString(std::string(data))) {
    packet_type_ = BlePacketType::kInvalid;
    return;
  }

  control_frame_type_ = socket_control_frame.type();
  switch (control_frame_type_) {
    case SocketControlFrame::INTRODUCTION:
      if (!socket_control_frame.has_introduction()) {
        packet_type_ = BlePacketType::kInvalid;
        return;
      }
      introducton_socket_version_ =
          socket_control_frame.introduction().socket_version();
      if (introducton_socket_version_ != SocketVersion::V2) {
        packet_type_ = BlePacketType::kInvalid;
        return;
      }
      if (!socket_control_frame.introduction().has_service_id_hash()) {
        packet_type_ = BlePacketType::kInvalid;
        return;
      }
      service_id_hash_ =
          ByteArray(socket_control_frame.introduction().service_id_hash());
      break;
    case SocketControlFrame::DISCONNECTION:
      if (!socket_control_frame.has_disconnection()) {
        packet_type_ = BlePacketType::kInvalid;
        return;
      }
      if (!socket_control_frame.disconnection().has_service_id_hash()) {
        packet_type_ = BlePacketType::kInvalid;
        return;
      }
      service_id_hash_ =
          ByteArray(socket_control_frame.disconnection().service_id_hash());
      break;
    case SocketControlFrame::PACKET_ACKNOWLEDGEMENT:
      if (!socket_control_frame.has_packet_acknowledgement()) {
        packet_type_ = BlePacketType::kInvalid;
        return;
      }
      if (!socket_control_frame.packet_acknowledgement()
               .has_service_id_hash()) {
        packet_type_ = BlePacketType::kInvalid;
        return;
      }
      service_id_hash_ = ByteArray(
          socket_control_frame.packet_acknowledgement().service_id_hash());
      packet_acknowledgement_received_size_ = 0;
      if (socket_control_frame.packet_acknowledgement().has_received_size()) {
        packet_acknowledgement_received_size_ =
            socket_control_frame.packet_acknowledgement().received_size();
      }
      break;
    default: {
      packet_type_ = BlePacketType::kInvalid;
    }
  }
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
