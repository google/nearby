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

#include "connections/implementation/mediums/advertisements/dct_advertisement.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "connections/implementation/mediums/advertisements/data_element.h"
#include "internal/crypto_cros/hkdf.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/crypto.h"
#include "internal/platform/logging.h"
#include "internal/platform/stream_reader.h"
#include "internal/platform/stream_writer.h"

namespace nearby::connections::advertisements::ble {
namespace {
constexpr int kServiceIdHashSize = 2;
constexpr int kMaxDeviceNameSize = 7;
constexpr int kDedupBitSize = 7;
constexpr int kDataTypeServiceIdHash = 0x05;
constexpr int kDataTypeDeviceInformation = 0x07;
constexpr int kDataTypePsm = 0x04;
constexpr char kServiceIdHashSalt[] = "DCT Protocol";
constexpr char kServiceIdHashInfo[] = "Service ID Hash";
constexpr char kEndpointIdChars[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
}  // namespace

DctAdvertisement::DctAdvertisement(const std::string& service_id,
                                   const std::string& device_name, uint16_t psm,
                                   uint8_t dedup) {
  psm_ = psm;
  service_id_hash_ = nearby::crypto::HkdfSha256(
      /*secret=*/service_id, /*salt= */ kServiceIdHashSalt,
      /*info=*/kServiceIdHashInfo,
      /*derived_key_size=*/kServiceIdHashSize);
  if (device_name.size() > kMaxDeviceNameSize) {
    is_device_name_truncated_ = true;
    // Truncate the device name and make sure it is a valid UTF-8 string.
    device_name_ = TrunctateDeviceName(device_name, kMaxDeviceNameSize);
  } else {
    device_name_ = device_name;
  }

  dedup_ = dedup;
}

std::optional<DctAdvertisement> DctAdvertisement::Create(
    const std::string& service_id, const std::string& device_name, uint16_t psm,
    uint8_t dedup) {
  if (service_id.empty() || device_name.empty() || psm == 0 || dedup > 0x7F) {
    LOG(WARNING) << "Invalid arguments for creating a DCT advertisement.";
    return std::nullopt;
  }

  if (!IsValidUtf8String(device_name)) {
    LOG(WARNING) << "Invalid UTF-8 string for device name.";
    return std::nullopt;
  }

  return DctAdvertisement(service_id, device_name, psm, dedup);
}

std::optional<DctAdvertisement> DctAdvertisement::Parse(
    const std::string& advertisement) {
  ByteArray data(advertisement);
  StreamReader reader(data);
  DctAdvertisement dct_advertisement;

  std::optional<uint8_t> header = reader.ReadUint8();
  if (!header.has_value()) {
    LOG(WARNING) << "Failed to read header.";
    return std::nullopt;
  }

  if (*header != (kVersion << 5)) {
    LOG(WARNING) << "Unsupported version: " << *header;
    return std::nullopt;
  }

  // Read service id hash
  std::optional<DataElement> service_id_hash =
      DataElement::FromStreamReader(reader);
  if (!service_id_hash.has_value()) {
    LOG(WARNING) << "Failed to read service id hash.";
    return std::nullopt;
  }
  if (service_id_hash->length() != kServiceIdHashSize ||
      service_id_hash->type() != kDataTypeServiceIdHash) {
    LOG(WARNING) << "Invalid service id hash.";
    return std::nullopt;
  }
  dct_advertisement.service_id_hash_ = service_id_hash->value();

  // Read PSM
  std::optional<DataElement> psm = DataElement::FromStreamReader(reader);
  if (!psm.has_value()) {
    LOG(WARNING) << "Failed to read PSM.";
    return std::nullopt;
  }
  if (psm->length() != 2 || psm->type() != kDataTypePsm) {
    LOG(WARNING) << "Invalid PSM.";
    return std::nullopt;
  }
  std::string psm_str = psm->value().data();
  dct_advertisement.psm_ =
      (static_cast<uint16_t>(psm_str[0]) << 8) | psm_str[1];

  // Read device information
  std::optional<DataElement> device_information =
      DataElement::FromStreamReader(reader);
  if (!device_information.has_value()) {
    LOG(WARNING) << "Failed to read device information.";
    return std::nullopt;
  }
  if (device_information->type() != kDataTypeDeviceInformation) {
    LOG(WARNING) << "Invalid device information.";
    return std::nullopt;
  }

  std::string device_information_str = device_information->value();
  dct_advertisement.is_device_name_truncated_ =
      device_information_str[0] & 0x80;
  dct_advertisement.dedup_ = device_information_str[0] & 0x7F;
  dct_advertisement.device_name_ = device_information_str.substr(1);

  return dct_advertisement;
}

std::optional<std::string> DctAdvertisement::GenerateEndpointId(
    uint8_t dedup, const std::string& device_name) {
  if (device_name.empty() || !IsValidUtf8String(device_name) || dedup > 0x7F) {
    return std::nullopt;
  }

  std::string truncted_device_name =
      TrunctateDeviceName(device_name, kMaxDeviceNameSize);

  truncted_device_name.append(1, dedup);
  ByteArray hash_result(4);
  hash_result.CopyAt(0, Crypto::Sha256(truncted_device_name));
  std::string endpoint_id;
  for (const char c : hash_result) {
    endpoint_id.append(1, kEndpointIdChars[c % sizeof(kEndpointIdChars)]);
  }

  return endpoint_id;
}

std::string DctAdvertisement::ToData() const {
  StreamWriter writer;

  writer.WriteUint8(kVersion << 5);
  writer.WriteBits(service_id_hash_.size(), 4);
  writer.WriteBits(kDataTypeServiceIdHash, 4);
  writer.WriteBytes(service_id_hash_);
  writer.WriteBits(2, 4);
  writer.WriteBits(kDataTypePsm, 4);
  writer.WriteUint16(psm_);
  // 2 bytes DE for device information.
  writer.WriteBits(1, 1);
  writer.WriteBits(device_name_.size() + 1, 7);
  writer.WriteUint8(kDataTypeDeviceInformation);
  writer.WriteBits(is_device_name_truncated_ ? 1 : 0, 1);
  writer.WriteBits(dedup_, kDedupBitSize);
  writer.WriteBytes(device_name_);

  return writer.GetData();
}

std::string DctAdvertisement::TrunctateDeviceName(
    const std::string& device_name, int max_length) {
  size_t byte_index = 0;

  while (byte_index < device_name.length() && byte_index < max_length) {
    unsigned char byte = static_cast<unsigned char>(device_name[byte_index]);

    if ((byte & 0x80) == 0x00) {
      byte_index++;
    } else if ((byte & 0xE0) == 0xC0) {
      if (byte_index + 2 <= max_length &&
          byte_index + 1 < device_name.length() &&
          (static_cast<unsigned char>(device_name[byte_index + 1]) & 0xC0) ==
              0x80) {
        byte_index += 2;
      } else {
        break;  // Invalid or exceeds max length
      }
    } else if ((byte & 0xF0) == 0xE0) {
      if (byte_index + 3 <= max_length &&
          byte_index + 2 < device_name.length() &&
          (static_cast<unsigned char>(device_name[byte_index + 1]) & 0xC0) ==
              0x80 &&
          (static_cast<unsigned char>(device_name[byte_index + 2]) & 0xC0) ==
              0x80) {
        byte_index += 3;
      } else {
        break;  // Invalid or exceeds max length
      }
    } else if ((byte & 0xF8) == 0xF0) {
      if (byte_index + 4 <= max_length &&
          byte_index + 3 < device_name.length() &&
          (static_cast<unsigned char>(device_name[byte_index + 1]) & 0xC0) ==
              0x80 &&
          (static_cast<unsigned char>(device_name[byte_index + 2]) & 0xC0) ==
              0x80 &&
          (static_cast<unsigned char>(device_name[byte_index + 3]) & 0xC0) ==
              0x80) {
        byte_index += 4;
      } else {
        break;  // Invalid or exceeds max length
      }
    } else {
      break;  // Invalid UTF-8 sequence
    }
  }

  return device_name.substr(0, byte_index);
}

bool DctAdvertisement::IsValidUtf8String(const std::string& str) {
  const unsigned char* bytes =
      reinterpret_cast<const unsigned char*>(str.c_str());
  const unsigned char* end = bytes + str.length();

  while (bytes < end) {
    unsigned char byte = *bytes++;

    if ((byte & 0x80) == 0x00) {
      continue;
    } else if ((byte & 0xE0) == 0xC0) {
      if (bytes + 1 > end || (bytes[0] & 0xC0) != 0x80) {
        return false;
      }
      bytes++;
    } else if ((byte & 0xF0) == 0xE0) {
      if (bytes + 2 > end || (bytes[0] & 0xC0) != 0x80 ||
          (bytes[1] & 0xC0) != 0x80) {
        return false;
      }
      bytes += 2;
    } else if ((byte & 0xF8) == 0xF0) {
      if (bytes + 3 > end || (bytes[0] & 0xC0) != 0x80 ||
          (bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80) {
        return false;
      }
      bytes += 3;
    } else {
      return false;
    }
  }

  return true;
}

}  // namespace nearby::connections::advertisements::ble
