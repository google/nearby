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

#include "connections/implementation/mediums/ble/ble_l2cap_packet.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "connections/implementation/mediums/ble/ble_advertisement.h"
#include "connections/implementation/mediums/utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/stream_reader.h"

namespace nearby {
namespace connections {
namespace mediums {

BleL2capPacket::BleL2capPacket(Command command,
                               const ByteArray* service_id_hash,
                               const ByteArray* data)
    : command_(command) {
  if (service_id_hash) {
    service_id_hash_ = *service_id_hash;
  }
  if (data) {
    advertisement_ = *data;
  }
}

absl::StatusOr<BleL2capPacket> BleL2capPacket::CreateFromStream(
    InputStream& input_stream) {
  // The first 1 byte is the command.
  ExceptionOr<ByteArray> command_byte = input_stream.Read(kCommandLength);
  if (!command_byte.ok()) {
    LOG(WARNING) << "Cannot read BleL2capPacket: command byte not available.";
    return absl::InvalidArgumentError(
        "Cannot read BleL2capPacket: command byte not available.");
  }
  Command command_data =
      static_cast<Command>((int)command_byte.result().data()[0]);
  VLOG(1) << "command_data: " << static_cast<int>(command_data);

  int data_length = 0;
  if (command_data == Command::kRequestAdvertisement ||
      command_data == Command::kResponseAdvertisement) {
    ExceptionOr<ByteArray> length_byte = input_stream.Read(2);
    if (!length_byte.ok()) {
      LOG(WARNING) << "Cannot read BleL2capPacket: length byte not available.";
      return absl::InvalidArgumentError(
          "Cannot read BleL2capPacket: length byte not available.");
    }
    data_length = (static_cast<uint8_t>(length_byte.result().data()[0]) << 8) |
                  (static_cast<uint8_t>(length_byte.result().data()[1]));
    if (data_length == 0) {
      LOG(WARNING) << "Cannot read BleL2capPacket: data length incorrect.";
      return absl::InvalidArgumentError(
          "Cannot read BleL2capPacket: data length incorrect.");
    }
  }

  switch (command_data) {
    case Command::kRequestAdvertisementFinish:
    case Command::kRequestDataConnection:
    case Command::kResponseServiceIdNotFound:
    case Command::kResponseDataConnectionReady:
    case Command::kResponseDataConnectionFailure:
      return BleL2capPacket(command_data, nullptr, nullptr);
    case Command::kRequestAdvertisement: {
      if (data_length < BleAdvertisement::kServiceIdHashLength) {
        LOG(WARNING)
            << "Cannot read BleL2capPacket: service id hash length, got "
            << data_length;
        return absl::InvalidArgumentError(
            "Cannot read BleL2capPacket: service id hash length "
            "incorrect.");
      }
      ExceptionOr<ByteArray> service_id_hash_byte =
          input_stream.Read(data_length);
      if (!service_id_hash_byte.ok() ||
          service_id_hash_byte.result().size() != data_length) {
        LOG(WARNING) << "Cannot read BleL2capPacket: service id hash byte "
                        "not available.";
        return absl::InvalidArgumentError(
            "Cannot read BleL2capPacket: service id hash byte not "
            "available.");
      }
      return BleL2capPacket(command_data, &service_id_hash_byte.result(),
                            nullptr);
    }
    case Command::kResponseAdvertisement: {
      if (data_length > BleAdvertisement::kMaxAdvertisementLength) {
        LOG(INFO) << "Cannot read BleL2capPacket: advertisement length, got "
                  << data_length;
        return absl::InvalidArgumentError(
            "Cannot read BleL2capPacket: advertisement length incorrect.");
      }
      ExceptionOr<ByteArray> advertisement_data =
          input_stream.Read(data_length);
      if (!advertisement_data.ok() ||
          advertisement_data.result().size() != data_length) {
        LOG(WARNING)
            << "Cannot read BleL2capPacket: advertisement not available.";
        return absl::InvalidArgumentError(
            "Cannot read BleL2capPacket: advertisement not available.");
      }
      return BleL2capPacket(command_data, nullptr,
                            &advertisement_data.result());
    }
    default:
      // fall through
      break;
  }
  LOG(WARNING) << "Cannot read BleL2capPacket: unsupported command "
               << static_cast<int>(command_data);
  return absl::InvalidArgumentError(
      "Cannot read BleL2capPacket: unsupported command.");
}

absl::StatusOr<BleL2capPacket> BleL2capPacket::CreateFromBytes(
    const ByteArray& bytes) {
  if (bytes.size() < kCommandLength) {
    LOG(INFO) << "Cannot deserialize BleL2capPacket: expecting min "
              << kCommandLength << " bytes, got " << bytes.size();
    return absl::InvalidArgumentError(
        "Cannot deserialize BleL2capPacket: input bytes too short.");
  }

  StreamReader stream_reader(&bytes);
  // The first 1 byte is the command.
  auto command_byte = stream_reader.ReadUint8();
  if (!command_byte.has_value()) {
    LOG(INFO)
        << "Cannot deserialize BleL2capPacket: command byte not available.";
    return absl::InvalidArgumentError(
        "Cannot deserialize BleL2capPacket: command byte not available.");
  }
  Command command_data = static_cast<Command>(*command_byte);

  if (command_data == Command::kRequestAdvertisementFinish ||
      command_data == Command::kRequestDataConnection ||
      command_data == Command::kResponseServiceIdNotFound ||
      command_data == Command::kResponseDataConnectionReady ||
      command_data == Command::kResponseDataConnectionFailure) {
    return BleL2capPacket(command_data, nullptr, nullptr);
  }

  if (!stream_reader.IsAvailable(kDataLength)) {
    LOG(INFO) << "Cannot deserialize BleL2capPacket: data length bytes not "
                 "available.";
    return absl::InvalidArgumentError(
        "Cannot deserialize BleL2capPacket: data length bytes not available.");
  }

  int data_length = stream_reader.ReadUint16().value_or(0);
  if (data_length == 0) {
    LOG(INFO) << "Cannot deserialize BleL2capPacket: data length incorrect.";
    return absl::InvalidArgumentError(
        "Cannot deserialize BleL2capPacket: data length incorrect.");
  }

  if (command_data == Command::kRequestAdvertisement) {
    if (data_length < BleAdvertisement::kServiceIdHashLength) {
      LOG(INFO)
          << "Cannot deserialize BleL2capPacket: service id hash length, got "
          << data_length;
      return absl::InvalidArgumentError(
          "Cannot deserialize BleL2capPacket: service id hash length "
          "incorrect.");
    }
    auto service_id_hash_data = stream_reader.ReadBytes(data_length);
    if (!service_id_hash_data.has_value()) {
      LOG(INFO) << "Cannot deserialize BleL2capPacket: service id hash not "
                   "available.";
      return absl::InvalidArgumentError(
          "Cannot deserialize BleL2capPacket: service id hash not available.");
    }
    return BleL2capPacket(command_data, &service_id_hash_data.value(), nullptr);
  } else if (command_data == Command::kResponseAdvertisement) {
    if (data_length > BleAdvertisement::kMaxAdvertisementLength) {
      LOG(INFO) << "Cannot deserialize BleL2capPacket: advertisement length, "
                   "got "
                << data_length;
      return absl::InvalidArgumentError(
          "Cannot deserialize BleL2capPacket: advertisement length incorrect.");
    }
    auto advertisement_data = stream_reader.ReadBytes(data_length);
    if (!advertisement_data.has_value()) {
      LOG(INFO)
          << "Cannot deserialize BleL2capPacket: advertisement not available.";
      return absl::InvalidArgumentError(
          "Cannot deserialize BleL2capPacket: advertisement not available.");
    }
    return BleL2capPacket(command_data, nullptr, &advertisement_data.value());
  } else {
    LOG(INFO) << "Cannot deserialize BleL2capPacket: unsupported command "
              << static_cast<int>(command_data);
    return absl::InvalidArgumentError(
        "Cannot deserialize BleL2capPacket: unsupported command.");
  }
}

absl::StatusOr<ByteArray> BleL2capPacket::ByteArrayForRequestAdvertisement(
    const std::string& service_id) {
  if (service_id.empty()) {
    return absl::InvalidArgumentError(
        "Cannot deserialize BleL2capPacket: service id is empty.");
  }
  ByteArray service_id_hash = GenerateServiceIdHash(service_id);
  return ByteArrayForCommand(BleL2capPacket::Command::kRequestAdvertisement,
                             &service_id_hash);
}

absl::StatusOr<ByteArray> BleL2capPacket::ByteArrayForResponseAdvertisement(
    const ByteArray& advertisement) {
  if (advertisement.size() < BleAdvertisement::kMinAdvertisementLength) {
    return absl::InvalidArgumentError(
        "Cannot deserialize BleL2capPacket: advertisement size is too small.");
  }
  return ByteArrayForCommand(BleL2capPacket::Command::kResponseAdvertisement,
                             &advertisement);
}

ByteArray BleL2capPacket::ByteArrayForRequestAdvertisementFinish() {
  return ByteArrayForCommand(
      BleL2capPacket::Command::kRequestAdvertisementFinish, {});
}

ByteArray BleL2capPacket::ByteArrayForServiceIdNotFound() {
  return ByteArrayForCommand(Command::kResponseServiceIdNotFound, {});
}

ByteArray BleL2capPacket::ByteArrayForRequestDataConnection() {
  return ByteArrayForCommand(Command::kRequestDataConnection, {});
}

ByteArray BleL2capPacket::ByteArrayForDataConnectionReady() {
  return ByteArrayForCommand(Command::kResponseDataConnectionReady, {});
}

ByteArray BleL2capPacket::ByteArrayForDataConnectionFailure() {
  return ByteArrayForCommand(Command::kResponseDataConnectionFailure, {});
}

ByteArray BleL2capPacket::GenerateServiceIdHash(const std::string& service_id) {
  return Utils::Sha256Hash(service_id, BleAdvertisement::kServiceIdHashLength);
}

ByteArray BleL2capPacket::ByteArrayForCommand(BleL2capPacket::Command command,
                                              const ByteArray* data) {
  std::string out;
  if (data) {
    ByteArray length_bytes{kDataLength};
    char* length_bytes_data = length_bytes.data();
    length_bytes_data[0] = (data->size() & 0xFF00) >> 8;
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
