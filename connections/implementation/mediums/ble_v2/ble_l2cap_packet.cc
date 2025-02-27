// Copyright 2025 Google LLC
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

#include "connections/implementation/mediums/ble_v2/ble_l2cap_packet.h"

#include <optional>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement.h"
#include "connections/implementation/mediums/utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"
#include "internal/platform/stream_reader.h"

namespace nearby {
namespace connections {
namespace mediums {

BleL2capPacket::BleL2capPacket(Command command,
                               std::optional<ByteArray> service_id_hash,
                               std::optional<ByteArray> data)
    : command_(command),
      service_id_hash_(std::move(service_id_hash)),
      advertisement_(std::move(data)) {}

BleL2capPacket::BleL2capPacket(Command command) : command_(command) {}

absl::optional<BleL2capPacket> BleL2capPacket::CreateFromBytes(
    const ByteArray &bytes) {
  if (bytes.size() < kCommandLength) {
    LOG(INFO) << "Cannot deserialize BleL2capPacket: expecting min "
              << kCommandLength << " bytes, got " << bytes.size();
    return std::nullopt;
  }

  StreamReader stream_reader(bytes);
  // The first 1 byte is the command.
  auto command_byte = stream_reader.ReadUint8();
  if (!command_byte.has_value()) {
    LOG(INFO) << "Cannot deserialize BleL2capPacket: command byte.";
    return std::nullopt;
  }
  Command command_data = static_cast<Command>(*command_byte);

  if (command_data == Command::kRequestAdvertisementFinish ||
      command_data == Command::kRequestDataConnection ||
      command_data == Command::kResponseServiceIdNotFound ||
      command_data == Command::kResponseDataConnectionReady ||
      command_data == Command::kResponseDataConnectionFailure) {
    return BleL2capPacket(command_data);
  }

  if (!stream_reader.IsAvailable(kDataLength)) {
    LOG(INFO) << "Cannot deserialize BleL2capPacket: data length.";
    return std::nullopt;
  }

  int data_length = stream_reader.ReadUint16().value_or(0);
  if (data_length == 0) {
    LOG(INFO) << "Cannot deserialize BleL2capPacket: data length.";
    return std::nullopt;
  }

  if (command_data == Command::kRequestAdvertisement) {
    if (data_length < BleAdvertisement::kServiceIdHashLength) {
      LOG(INFO)
          << "Cannot deserialize BleL2capPacket: service id hash length, got "
          << data_length;
      return std::nullopt;
    }
    auto service_id_hash_data = stream_reader.ReadBytes(data_length);
    if (!service_id_hash_data.has_value()) {
      LOG(INFO) << "Cannot deserialize BleL2capPacket: service id hash.";
      return std::nullopt;
    }
    return BleL2capPacket(command_data, service_id_hash_data, std::nullopt);
  } else if (command_data == Command::kResponseAdvertisement) {
    if (data_length > BleAdvertisement::kMaxAdvertisementLength) {
      LOG(INFO) << "Cannot deserialize BleL2capPacket: advertisement length, "
                   "got "
                << data_length;
      return std::nullopt;
    }
    auto advertisement_data = stream_reader.ReadBytes(data_length);
    if (!advertisement_data.has_value()) {
      LOG(INFO) << "Cannot deserialize BleL2capPacket: advertisement.";
      return std::nullopt;
    }
    return BleL2capPacket(command_data, std::nullopt, advertisement_data);
  } else {
    LOG(INFO) << "Cannot deserialize BleL2capPacket: unsupported command "
              << static_cast<int>(command_data);
    return std::nullopt;
  }

  return absl::nullopt;
}

absl::optional<ByteArray> BleL2capPacket::ByteArrayForRequestAdvertisement(
    std::string service_id) {
  if (service_id.empty()) {
    return std::nullopt;
  }
  return ByteArrayForCommand(BleL2capPacket::Command::kRequestAdvertisement,
                             BleL2capPacket::GenerateServiceIdHash(service_id));
}

std::optional<ByteArray> BleL2capPacket::ByteArrayForResponseAdvertisement(
    const ByteArray &advertisement) {
  if (advertisement.size() < BleAdvertisement::kMinAdvertisementLength) {
    return std::nullopt;
  }
  return ByteArrayForCommand(BleL2capPacket::Command::kResponseAdvertisement,
                             advertisement);
}

ByteArray BleL2capPacket::ByteArrayForRequestAdvertisementFinish() {
  return ByteArrayForCommand(
      BleL2capPacket::Command::kRequestAdvertisementFinish, std::nullopt);
}

ByteArray BleL2capPacket::ByteArrayForServiceIdNotFound() {
  return ByteArrayForCommand(Command::kResponseServiceIdNotFound, std::nullopt);
}

ByteArray BleL2capPacket::ByteArrayForRequestDataConnection() {
  return ByteArrayForCommand(Command::kRequestDataConnection, std::nullopt);
}

ByteArray BleL2capPacket::ByteArrayForDataConnectionReady() {
  return ByteArrayForCommand(Command::kResponseDataConnectionReady,
                             std::nullopt);
}

ByteArray BleL2capPacket::ByteArrayForDataConnectionFailure() {
  return ByteArrayForCommand(Command::kResponseDataConnectionFailure,
                             std::nullopt);
}

ByteArray BleL2capPacket::GenerateServiceIdHash(std::string service_id) {
  return Utils::Sha256Hash(service_id, BleAdvertisement::kServiceIdHashLength);
}

ByteArray BleL2capPacket::ByteArrayForCommand(
    BleL2capPacket::Command command, const std::optional<ByteArray> &data) {
  std::string out;
  if (data.has_value()) {
    ByteArray length_bytes{kDataLength};
    char *length_bytes_data = length_bytes.data();
    length_bytes_data[0] = data->size() & 0xFF00;
    length_bytes_data[1] = data->size() & 0x00FF;
    out = absl::StrCat(std::string(1, static_cast<char>(command)),
                       std::string(length_bytes), std::string(*data));
  } else {
    out = std::string(1, static_cast<char>(command));
  }
  return ByteArray{std::move(out)};
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
