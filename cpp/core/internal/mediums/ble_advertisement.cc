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

#include "core/internal/mediums/ble_advertisement.h"

#include <cstring>

#include "platform/logging.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

const std::uint32_t BLEAdvertisement::kServiceIdHashLength = 3;

const std::uint32_t BLEAdvertisement::kVersionLength = 1;
// Length of one int. Be sure to re-evaluate how we compute data size in this
// class if this constant ever changes!
const std::uint32_t BLEAdvertisement::kDataSizeLength = 4;
const std::uint32_t BLEAdvertisement::kMinAdvertisementLength =
    kVersionLength + kServiceIdHashLength + kDataSizeLength;
// The maximum length for a GATT characteristic value is 512 bytes, so make sure
// the entire advertisement is less than that. The data can take up whatever
// space is remaining after the bytes preceding it.
const std::uint32_t BLEAdvertisement::kMaxDataSize =
    512 - kMinAdvertisementLength;
const std::uint16_t BLEAdvertisement::kVersionBitmask = 0x0E0;
const std::uint16_t BLEAdvertisement::kSocketVersionBitmask = 0x01C;

ConstPtr<BLEAdvertisement> BLEAdvertisement::fromBytes(
    ConstPtr<ByteArray> ble_advertisement_bytes) {
  if (ble_advertisement_bytes.isNull()) {
    NEARBY_LOG(INFO,
               "Cannot deserialize BLEAdvertisement: null bytes passed in");
    return ConstPtr<BLEAdvertisement>();
  }

  if (ble_advertisement_bytes->size() < kMinAdvertisementLength) {
    NEARBY_LOG(INFO,
               "Cannot deserialize BLEAdvertisement: expecting min %u raw "
               "bytes, got %zu",
               kMinAdvertisementLength, ble_advertisement_bytes->size());
    return ConstPtr<BLEAdvertisement>();
  }

  // Now, time to read the bytes!
  const char *ble_advertisement_bytes_read_ptr =
      ble_advertisement_bytes->getData();

  // 1. Version.
  Version::Value version = parseVersionFromByte(
      static_cast<std::uint16_t>(*ble_advertisement_bytes_read_ptr));
  if (!isSupportedVersion(version)) {
    NEARBY_LOG(INFO,
               "Cannot deserialize BLEAdvertisement: unsupported Version %u",
               version);
    return ConstPtr<BLEAdvertisement>();
  }

  // 2. Socket Version.
  SocketVersion::Value socket_version = parseSocketVersionFromByte(
      static_cast<std::uint16_t>(*ble_advertisement_bytes_read_ptr));
  if (!isSupportedSocketVersion(socket_version)) {
    NEARBY_LOG(
        INFO,
        "Cannot deserialize BLEAdvertisement: unsupported SocketVersion %u",
        socket_version);
    return ConstPtr<BLEAdvertisement>();
  }
  ble_advertisement_bytes_read_ptr += kVersionLength;

  // 3. Service ID hash.
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(MakeConstPtr(
      new ByteArray(ble_advertisement_bytes_read_ptr, kServiceIdHashLength)));
  ble_advertisement_bytes_read_ptr += kServiceIdHashLength;

  // 4.1. Data size.
  size_t expected_data_size =
      deserializeDataSize(ble_advertisement_bytes_read_ptr);
  if (expected_data_size < 0) {
    NEARBY_LOG(INFO,
               "Cannot deserialize BLEAdvertisement: negative data size %zu",
               expected_data_size);
    return ConstPtr<BLEAdvertisement>();
  }
  ble_advertisement_bytes_read_ptr += kDataSizeLength;

  // Check that the stated data size is the same as what we received.
  size_t actual_data_size = computeDataSize(ble_advertisement_bytes);
  if (actual_data_size < expected_data_size) {
    NEARBY_LOG(INFO,
               "Cannot deserialize BLEAdvertisement: expected data to be %zu "
               "bytes, got %zu bytes",
               expected_data_size, actual_data_size);
    return ConstPtr<BLEAdvertisement>();
  }

  // 4.2. Data.
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(MakeConstPtr(
      new ByteArray(ble_advertisement_bytes_read_ptr, expected_data_size)));
  ble_advertisement_bytes_read_ptr += expected_data_size;

  return MakeRefCountedConstPtr(new BLEAdvertisement(
      version, socket_version, scoped_service_id_hash.release(),
      scoped_data.release()));
}

ConstPtr<ByteArray> BLEAdvertisement::toBytes(
    Version::Value version, SocketVersion::Value socket_version,
    ConstPtr<ByteArray> service_id_hash, ConstPtr<ByteArray> data) {
  // Check that the given input is valid.
  if (!isSupportedVersion(version)) {
    NEARBY_LOG(INFO,
               "Cannot serialize BLEAdvertisement: unsupported Version %u",
               version);
    return ConstPtr<ByteArray>();
  }

  if (!isSupportedSocketVersion(socket_version)) {
    NEARBY_LOG(
        INFO, "Cannot serialize BLEAdvertisement: unsupported SocketVersion %u",
        socket_version);
    return ConstPtr<ByteArray>();
  }

  if (service_id_hash->size() != kServiceIdHashLength) {
    NEARBY_LOG(INFO,
               "Cannot serialize BLEAdvertisement: expected a service_id_hash "
               "of %u bytes, but got %zu",
               kServiceIdHashLength, service_id_hash->size());
    return ConstPtr<ByteArray>();
  }

  if (data->size() > kMaxDataSize) {
    NEARBY_LOG(INFO,
               "Cannot serialize BLEAdvertisement: expected data of at most %u "
               "bytes, but got %zu",
               kMaxDataSize, data->size());
    return ConstPtr<ByteArray>();
  }

  // Initialize the bytes.
  size_t advertisement_length = computeAdvertisementLength(data);
  Ptr<ByteArray> advertisement_bytes{new ByteArray{advertisement_length}};
  char *advertisement_bytes_write_ptr = advertisement_bytes->getData();

  // 1. Version.
  serializeVersionByte(advertisement_bytes_write_ptr, version);

  // 2. SocketVersion.
  serializeSocketVersionByte(advertisement_bytes_write_ptr, socket_version);
  advertisement_bytes_write_ptr += kVersionLength;

  // 3. Service ID hash.
  memcpy(advertisement_bytes_write_ptr, service_id_hash->getData(),
         kServiceIdHashLength);
  advertisement_bytes_write_ptr += kServiceIdHashLength;

  // 4.1. Data length.
  serializeDataSize(advertisement_bytes_write_ptr, data->size());
  advertisement_bytes_write_ptr += kDataSizeLength;

  // 4.2. Data.
  memcpy(advertisement_bytes_write_ptr, data->getData(), data->size());
  advertisement_bytes_write_ptr += data->size();

  return ConstifyPtr(advertisement_bytes);
}

bool BLEAdvertisement::isSupportedVersion(Version::Value version) {
  return version >= Version::V1 && version <= Version::V2;
}

bool BLEAdvertisement::isSupportedSocketVersion(
    SocketVersion::Value socket_version) {
  return socket_version >= SocketVersion::V1 &&
         socket_version <= SocketVersion::V2;
}

BLEAdvertisement::Version::Value BLEAdvertisement::parseVersionFromByte(
    std::uint16_t byte) {
  return static_cast<BLEAdvertisement::Version::Value>(
      (byte & kVersionBitmask) >> 5);
}

BLEAdvertisement::SocketVersion::Value
BLEAdvertisement::parseSocketVersionFromByte(std::uint16_t byte) {
  return static_cast<SocketVersion::Value>((byte & kSocketVersionBitmask) >> 2);
}

size_t BLEAdvertisement::deserializeDataSize(
    const char *data_size_bytes_read_ptr) {
  // Allocate a chunk of memory to store our deserialized size.
  char data_size_bytes[kDataSizeLength];

  // Assign the bits of our size from the given raw bytes, keeping in mind that
  // we need to convert from Big Endian to Little Endian in the process.
  for (int i = 0; i < kDataSizeLength; ++i) {
    data_size_bytes[i] = data_size_bytes_read_ptr[kDataSizeLength - i - 1];
  }

  // Interpret the char array as a single int.
  return static_cast<size_t>(
      *(reinterpret_cast<std::uint32_t *>(&data_size_bytes)));
}

size_t BLEAdvertisement::computeDataSize(
    ConstPtr<ByteArray> ble_advertisement_bytes) {
  return ble_advertisement_bytes->size() - kMinAdvertisementLength;
}

size_t BLEAdvertisement::computeAdvertisementLength(ConstPtr<ByteArray> data) {
  // The advertisement length is the minimum length + the length of the data.
  return kMinAdvertisementLength + data->size();
}

void BLEAdvertisement::serializeVersionByte(char *version_byte_write_ptr,
                                            Version::Value version) {
  *version_byte_write_ptr |=
      static_cast<char>((version << 5) & kVersionBitmask);
}

void BLEAdvertisement::serializeSocketVersionByte(
    char *socket_version_byte_write_ptr, SocketVersion::Value socket_version) {
  *socket_version_byte_write_ptr |=
      static_cast<char>((socket_version << 2) & kSocketVersionBitmask);
}

void BLEAdvertisement::serializeDataSize(char *data_size_bytes_write_ptr,
                                         size_t data_size) {
  // Get a raw representation of the data size bytes in memory.
  char *data_size_bytes = reinterpret_cast<char *>(&data_size);

  // Append these raw bytes to advertisement bytes, keeping in mind that we need
  // to convert from Little Endian to Big Endian in the process.
  for (int i = 0; i < kDataSizeLength; ++i) {
    data_size_bytes_write_ptr[i] = data_size_bytes[kDataSizeLength - i - 1];
  }
}

BLEAdvertisement::BLEAdvertisement(Version::Value version,
                                   SocketVersion::Value socket_version,
                                   ConstPtr<ByteArray> service_id_hash,
                                   ConstPtr<ByteArray> data)
    : version_(version),
      socket_version_(socket_version),
      service_id_hash_(service_id_hash),
      data_(data) {}

BLEAdvertisement::~BLEAdvertisement() {
  // Nothing to do.
}

BLEAdvertisement::Version::Value BLEAdvertisement::getVersion() const {
  return version_;
}

BLEAdvertisement::SocketVersion::Value BLEAdvertisement::getSocketVersion()
    const {
  return socket_version_;
}

ConstPtr<ByteArray> BLEAdvertisement::getServiceIdHash() const {
  return service_id_hash_.get();
}

ConstPtr<ByteArray> BLEAdvertisement::getData() const { return data_.get(); }

bool BLEAdvertisement::operator==(const BLEAdvertisement &rhs) const {
  return this->getVersion() == rhs.getVersion() &&
         this->getSocketVersion() == rhs.getSocketVersion() &&
         *(this->getServiceIdHash()) == *(rhs.getServiceIdHash()) &&
         *(this->getData()) == *(rhs.getData());
}

bool BLEAdvertisement::operator<(const BLEAdvertisement &rhs) const {
  if (this->getVersion() != rhs.getVersion()) {
    return this->getVersion() < rhs.getVersion();
  }
  if (this->getSocketVersion() != rhs.getSocketVersion()) {
    return this->getSocketVersion() < rhs.getSocketVersion();
  }
  if (*(this->getServiceIdHash()) != *(rhs.getServiceIdHash())) {
    return *(this->getServiceIdHash()) < *(rhs.getServiceIdHash());
  }
  return *(this->getData()) < *(rhs.getData());
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
