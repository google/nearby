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

#include "connections/implementation/mediums/ble_v2/ble_advertisement.h"

#include <inttypes.h>

#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"
#include "internal/platform/base_input_stream.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {
namespace mediums {

namespace {

constexpr uint8_t kPsmBitmask = 0x01;

bool HasField(uint8_t field_mask, uint8_t psm_bit) {
  return (field_mask & psm_bit) > 0;
}

}  // namespace

BleAdvertisement::BleAdvertisement(Version version,
                                   SocketVersion socket_version,
                                   const ByteArray &service_id_hash,
                                   const ByteArray &data,
                                   const ByteArray &device_token, int psm) {
  DoInitialize(/*fast_advertisement=*/service_id_hash.Empty(), version,
               socket_version, service_id_hash, data, device_token, psm);
}

void BleAdvertisement::DoInitialize(bool fast_advertisement, Version version,
                                    SocketVersion socket_version,
                                    const ByteArray &service_id_hash,
                                    const ByteArray &data,
                                    const ByteArray &device_token, int psm) {
  // Check that the given input is valid.
  fast_advertisement_ = fast_advertisement;
  if (!fast_advertisement_) {
    if (service_id_hash.size() != kServiceIdHashLength) return;
  }
  if (!IsSupportedVersion(version) ||
      !IsSupportedSocketVersion(socket_version) ||
      (!device_token.Empty() && device_token.size() != kDeviceTokenLength)) {
    return;
  }

  int advertisement_Length = ComputeAdvertisementLength(
      data.size(), device_token.size(), fast_advertisement_);
  int max_advertisement_length = fast_advertisement
                                     ? kMaxFastAdvertisementLength
                                     : kMaxAdvertisementLength;
  if (advertisement_Length > max_advertisement_length) {
    return;
  }

  version_ = version;
  socket_version_ = socket_version;
  if (!fast_advertisement_) service_id_hash_ = service_id_hash;
  data_ = data;
  device_token_ = device_token;
  psm_ = psm;
}

absl::StatusOr<BleAdvertisement> BleAdvertisement::CreateBleAdvertisement(
    const ByteArray &ble_advertisement_bytes) {
  if (ble_advertisement_bytes.Empty()) {
    return absl::InvalidArgumentError(
        "Cannot deserialize BleAdvertisement: null bytes passed in.");
  }

  if (ble_advertisement_bytes.size() < kVersionLength) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Cannot deserialize BleAdvertisement: expecting min ", kVersionLength,
        " bytes, got ", ble_advertisement_bytes.size()));
  }

  ByteArray advertisement_bytes(ble_advertisement_bytes);
  BaseInputStream base_input_stream(advertisement_bytes);
  // The first 1 byte is supposed to be the version, socket version and the fast
  // advertisement flag.
  auto version_byte = static_cast<char>(base_input_stream.ReadUint8());

  Version version = static_cast<Version>((version_byte & kVersionBitmask) >> 5);
  if (!IsSupportedVersion(version)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Cannot deserialize BleAdvertisement: unsupported Version ", version));
  }

  SocketVersion socket_version =
      static_cast<SocketVersion>((version_byte & kSocketVersionBitmask) >> 2);
  if (!IsSupportedSocketVersion(socket_version)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Cannot deserialize BleAdvertisement: unsupported SocketVersion ",
        socket_version));
  }

  bool fast_advertisement =
      static_cast<bool>((version_byte & kFastAdvertisementFlagBitmask) >> 1);

  // The next 3 bytes are supposed to be the service_id_hash if not fast
  // advertisement.
  ByteArray service_id_hash;
  if (!fast_advertisement) {
    service_id_hash = base_input_stream.ReadBytes(kServiceIdHashLength);
  }

  // Data length.
  int expected_data_size =
      fast_advertisement
          ? static_cast<int>(
                base_input_stream.ReadBytes(kFastDataSizeLength).data()[0])
          : static_cast<int>(base_input_stream.ReadUint32());
  if (expected_data_size < 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Cannot deserialize BleAdvertisement: negative data size ",
                     expected_data_size));
  }

  // Data.
  // Check that the stated data size is the same as what we received.
  auto data = base_input_stream.ReadBytes(expected_data_size);
  if (data.size() != expected_data_size) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Cannot deserialize BleAdvertisement: expected data to be ",
        expected_data_size, " bytes, got ", data.size()));
  }

  BleAdvertisement ble_advertisement;
  ble_advertisement.version_ = version;
  ble_advertisement.socket_version_ = socket_version;
  ble_advertisement.fast_advertisement_ = fast_advertisement;
  ble_advertisement.service_id_hash_ = service_id_hash;
  ble_advertisement.data_ = data;

  // Device token. If the number of remaining bytes are valid for device token,
  // then read it.
  if (base_input_stream.IsAvailable(kDeviceTokenLength)) {
    ble_advertisement.device_token_ =
        base_input_stream.ReadBytes(kDeviceTokenLength);
  } else {
    // No device token no more optional field.
    return ble_advertisement;
  }

  // Extra fields, for backward compatible reason, put this field in the end of
  // this advertisement. That means it must support device token if there's any
  // extra field. E.g. If iOS or other platform wants to use extra fields, need
  // to put a random or empty device token in the advertisement.
  int extra_fields_byte_number =
      kExtraFieldsMaskLength + BleAdvertisementHeader::kPsmValueByteLength;
  if (base_input_stream.IsAvailable(extra_fields_byte_number)) {
    BleExtraFields extra_fields{
        base_input_stream.ReadBytes(extra_fields_byte_number)};
    ble_advertisement.psm_ = extra_fields.GetPsm();
  }
  return ble_advertisement;
}

BleAdvertisement::operator ByteArray() const {
  if (!IsValid()) {
    return ByteArray{};
  }

  // The first 3 bits are the Version.
  char version_byte = (static_cast<char>(version_) << 5) & kVersionBitmask;
  // The next 3 bits are the Socket version. 2 bits left are reserved.
  version_byte |=
      (static_cast<char>(socket_version_) << 2) & kSocketVersionBitmask;
  // The next 1 bit is the fast advertisement flag. 1 bit left is reserved.
  version_byte |= (static_cast<char>(fast_advertisement_ ? 1 : 0) << 1) &
                  kFastAdvertisementFlagBitmask;

  // Serialize Data size bytes
  ByteArray data_size_bytes{static_cast<size_t>(
      fast_advertisement_ ? kFastDataSizeLength : kDataSizeLength)};
  auto *data_size_bytes_write_ptr = data_size_bytes.data();
  SerializeDataSize(fast_advertisement_, data_size_bytes_write_ptr,
                    data_.size());

  // For Extra fields, there's no space for legacy fast advertisement, use
  // ByteArrayWithExtraField() to get the new advertisement bytes for extended
  // advertising.

  // clang-format on
  if (fast_advertisement_) {
    std::string out =
        absl::StrCat(std::string(1, version_byte), std::string(data_size_bytes),
                     std::string(data_), std::string(device_token_));
    return ByteArray{std::move(out)};
  } else {
    std::string out = absl::StrCat(
        std::string(1, version_byte), std::string(service_id_hash_),
        std::string(data_size_bytes), std::string(data_),
        std::string(device_token_));
    return ByteArray{std::move(out)};
  }
  // clang-format on
}

ByteArray BleAdvertisement::ByteArrayWithExtraField() const {
  ByteArray advertisement_bytes = ByteArray(*this);
  ByteArray extra_fields_bytes = ByteArray(BleExtraFields(psm_));

  std::string advertisement_with_extra_fields_bytes = absl::StrCat(
      std::string(advertisement_bytes), std::string(extra_fields_bytes));

  return ByteArray(std::move(advertisement_with_extra_fields_bytes));
}

bool BleAdvertisement::operator==(const BleAdvertisement &rhs) const {
  return this->GetVersion() == rhs.GetVersion() &&
         this->GetSocketVersion() == rhs.GetSocketVersion() &&
         this->IsFastAdvertisement() == rhs.IsFastAdvertisement() &&
         this->GetServiceIdHash() == rhs.GetServiceIdHash() &&
         this->GetData() == rhs.GetData() &&
         this->GetDeviceToken() == rhs.GetDeviceToken() &&
         this->GetPsm() == rhs.GetPsm();
}

bool BleAdvertisement::IsSupportedVersion(Version version) {
  return version >= Version::kV1 && version <= Version::kV2;
}

bool BleAdvertisement::IsSupportedSocketVersion(SocketVersion socket_version) {
  return socket_version >= SocketVersion::kV1 &&
         socket_version <= SocketVersion::kV2;
}

void BleAdvertisement::SerializeDataSize(bool fast_advertisement,
                                         char *data_size_bytes_write_ptr,
                                         size_t data_size) const {
  // Get a raw representation of the data size bytes in memory.
  char *data_size_bytes = reinterpret_cast<char *>(&data_size);

  const int data_size_length =
      fast_advertisement ? kFastDataSizeLength : kDataSizeLength;

  // Append these raw bytes to advertisement bytes, keeping in mind that we need
  // to convert from Little Endian to Big Endian in the process.
  for (int i = 0; i < data_size_length; ++i) {
    data_size_bytes_write_ptr[i] = data_size_bytes[data_size_length - i - 1];
  }
}

BleAdvertisement::BleExtraFields::BleExtraFields(int psm) : psm_(psm) {}

BleAdvertisement::BleExtraFields::BleExtraFields(
    const ByteArray &ble_extra_fields_bytes) {
  if (ble_extra_fields_bytes.Empty()) {
    return;
  }

  ByteArray mutated_extra_fields_bytes = {ble_extra_fields_bytes};
  BaseInputStream base_input_stream{mutated_extra_fields_bytes};
  // The first 1 byte is field mask.
  auto mask_byte = static_cast<uint8_t>(base_input_stream.ReadUint8());
  if (!mask_byte) {
    return;
  }

  // The next 2 bytes are supposed to be the psm value.
  if (HasField(mask_byte, kPsmBitmask) &&
      base_input_stream.IsAvailable(
          BleAdvertisementHeader::kPsmValueByteLength)) {
    psm_ = static_cast<int>(base_input_stream.ReadUint16());
  }
}

BleAdvertisement::BleExtraFields::operator ByteArray() const {
  if (psm_ == BleAdvertisementHeader::kDefaultPsmValue) {
    return ByteArray{};
  }

  ByteArray psm_byte{BleAdvertisementHeader::kPsmValueByteLength};
  char *data = psm_byte.data();
  data[0] = psm_ & 0xFF00;
  data[1] = psm_ & 0x00FF;

  std::string out =
      absl::StrCat(std::string(1, kPsmBitmask), std::string(psm_byte));
  return ByteArray{std::move(out)};
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
