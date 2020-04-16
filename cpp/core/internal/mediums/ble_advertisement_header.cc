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

#include "core/internal/mediums/ble_advertisement_header.h"

#include <cstring>

#include "platform/base64_utils.h"
#include "platform/byte_array.h"
#include "platform/logging.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// The following IfThisThenThat is for BloomFilter length in
// ble_v2.createAdvertisementHeader
// LINT.IfChange
const std::uint32_t BLEAdvertisementHeader::kServiceIdBloomFilterLength = 10;
// LINT.ThenChange(cpp/core/internal/mediums/ble_v2.h)
const std::uint32_t BLEAdvertisementHeader::kAdvertisementHashLength = 4;

const std::uint32_t BLEAdvertisementHeader::kVersionAndNumSlotsLength = 1;
const std::uint32_t BLEAdvertisementHeader::kMinAdvertisementHeaderLength =
    kVersionAndNumSlotsLength + kServiceIdBloomFilterLength +
    kAdvertisementHashLength;
const std::uint16_t BLEAdvertisementHeader::kVersionBitmask = 0x0E0;
const std::uint16_t BLEAdvertisementHeader::kNumSlotsBitmask = 0x01F;

ConstPtr<BLEAdvertisementHeader> BLEAdvertisementHeader::fromString(
    const std::string &ble_advertisement_header_string) {
  ScopedPtr<Ptr<ByteArray> > scoped_ble_advertisement_header_bytes(
      Base64Utils::decode(ble_advertisement_header_string));
  if (scoped_ble_advertisement_header_bytes.isNull()) {
    NEARBY_LOG(
        INFO,
        "Cannot deserialize BLEAdvertisementHeader: failed Base64 decoding");
    return ConstPtr<BLEAdvertisementHeader>();
  }

  if (scoped_ble_advertisement_header_bytes->size() <
      kMinAdvertisementHeaderLength) {
    NEARBY_LOG(INFO,
               "Cannot deserialize BLEAdvertisementHeader: expecting min %u "
               "raw bytes, got %zu instead",
               kMinAdvertisementHeaderLength,
               scoped_ble_advertisement_header_bytes->size());
    return ConstPtr<BLEAdvertisementHeader>();
  }

  // Now, time to read the bytes!
  const char *ble_advertisement_header_read_ptr =
      scoped_ble_advertisement_header_bytes->getData();

  // 1. Version.
  // The first 3 bits of the first byte represent the version.
  Version::Value version = parseVersionFromByte(
      static_cast<std::uint16_t>(*ble_advertisement_header_read_ptr));
  if (version != Version::V2) {
    NEARBY_LOG(
        INFO,
        "Cannot deserialize BLEAdvertisementHeader, unsupported version %u",
        version);
    return ConstPtr<BLEAdvertisementHeader>();
  }

  // 2. Number of slots.
  // The last 5 bits of the first byte represent the number of slots.
  std::uint32_t num_slots = parseNumSlotsFromByte(
      static_cast<std::uint16_t>(*ble_advertisement_header_read_ptr));
  ble_advertisement_header_read_ptr += kVersionAndNumSlotsLength;

  // 3. Service ID bloom filter.
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_bloom_filter(
      MakeConstPtr(new ByteArray(ble_advertisement_header_read_ptr,
                                 kServiceIdBloomFilterLength)));
  ble_advertisement_header_read_ptr += kServiceIdBloomFilterLength;

  // 4. Advertisement hash.
  ScopedPtr<ConstPtr<ByteArray> > scoped_advertisement_hash(
      MakeConstPtr(new ByteArray(ble_advertisement_header_read_ptr,
                                 kAdvertisementHashLength)));
  ble_advertisement_header_read_ptr += kAdvertisementHashLength;

  return MakeRefCountedConstPtr(new BLEAdvertisementHeader(
      version, num_slots, scoped_service_id_bloom_filter.release(),
      scoped_advertisement_hash.release()));
}

std::string BLEAdvertisementHeader::asString(
    Version::Value version, std::uint32_t num_slots,
    ConstPtr<ByteArray> service_id_bloom_filter,
    ConstPtr<ByteArray> advertisement_hash) {
  // Check that the given input is valid.
  if (version != Version::V2) {
    NEARBY_LOG(
        INFO, "Cannot serialize BLEAdvertisementHeader: unsupported Version %u",
        version);
    return "";
  }

  if (service_id_bloom_filter->size() != kServiceIdBloomFilterLength) {
    NEARBY_LOG(INFO,
               "Cannot serialize BLEAdvertisementHeader: expected "
               "service_id_bloom_filter of %u bytes, but got %zu",
               kServiceIdBloomFilterLength, service_id_bloom_filter->size());
    return "";
  }

  if (advertisement_hash->size() != kAdvertisementHashLength) {
    NEARBY_LOG(INFO,
               "Cannot serialize BLEAdvertisementHeader: expected "
               "advertisement_hash of %u bytes, but got %zu",
               kAdvertisementHashLength, advertisement_hash->size());
    return "";
  }

  // Initialize the bytes.
  ByteArray advertisement_header_bytes{kMinAdvertisementHeaderLength};
  char *advertisement_header_bytes_write_ptr =
      advertisement_header_bytes.getData();

  // 1. Version.
  serializeVersionByte(advertisement_header_bytes_write_ptr, version);

  // 2. Number of slots.
  serializeNumSlots(advertisement_header_bytes_write_ptr, num_slots);
  advertisement_header_bytes_write_ptr += kVersionAndNumSlotsLength;

  // 3. Service ID bloom filter.
  memcpy(advertisement_header_bytes_write_ptr,
         service_id_bloom_filter->getData(), kServiceIdBloomFilterLength);
  advertisement_header_bytes_write_ptr += kServiceIdBloomFilterLength;

  // 4. Advertisement hash.
  memcpy(advertisement_header_bytes_write_ptr, advertisement_hash->getData(),
         kAdvertisementHashLength);
  advertisement_header_bytes_write_ptr += kAdvertisementHashLength;

  // Header needs to be binary safe, so apply a Base64 encoding.
  return Base64Utils::encode(advertisement_header_bytes);
}

BLEAdvertisementHeader::Version::Value
BLEAdvertisementHeader::parseVersionFromByte(std::uint16_t byte) {
  return static_cast<Version::Value>((byte & kVersionBitmask) >> 5);
}

std::uint32_t BLEAdvertisementHeader::parseNumSlotsFromByte(
    std::uint16_t byte) {
  return static_cast<std::uint32_t>((byte & kNumSlotsBitmask));
}

void BLEAdvertisementHeader::serializeVersionByte(char *version_byte_write_ptr,
                                                  Version::Value version) {
  *version_byte_write_ptr |=
      static_cast<char>((version << 5) & kVersionBitmask);
}

void BLEAdvertisementHeader::serializeNumSlots(char *num_slots_byte_write_ptr,
                                               std::uint32_t num_slots) {
  *num_slots_byte_write_ptr |= static_cast<char>(num_slots & kNumSlotsBitmask);
}

BLEAdvertisementHeader::BLEAdvertisementHeader(
    BLEAdvertisementHeader::Version::Value version, std::uint32_t num_slots,
    ConstPtr<ByteArray> service_id_bloom_filter,
    ConstPtr<ByteArray> advertisement_hash)
    : version_(version),
      num_slots_(num_slots),
      service_id_bloom_filter_(service_id_bloom_filter),
      advertisement_hash_(advertisement_hash) {}

BLEAdvertisementHeader::~BLEAdvertisementHeader() {
  // Nothing to do.
}

BLEAdvertisementHeader::Version::Value BLEAdvertisementHeader::getVersion()
    const {
  return version_;
}

std::uint32_t BLEAdvertisementHeader::getNumSlots() const { return num_slots_; }

ConstPtr<ByteArray> BLEAdvertisementHeader::getServiceIdBloomFilter() const {
  return service_id_bloom_filter_.get();
}

ConstPtr<ByteArray> BLEAdvertisementHeader::getAdvertisementHash() const {
  return advertisement_hash_.get();
}

bool BLEAdvertisementHeader::operator<(
    const BLEAdvertisementHeader &rhs) const {
  if (this->getVersion() != rhs.getVersion()) {
    return this->getVersion() < rhs.getVersion();
  }
  if (this->getNumSlots() != rhs.getNumSlots()) {
    return this->getNumSlots() < rhs.getNumSlots();
  }
  if (*(this->getServiceIdBloomFilter()) != *(rhs.getServiceIdBloomFilter())) {
    return *(this->getServiceIdBloomFilter()) <
           *(rhs.getServiceIdBloomFilter());
  }
  return *(this->getAdvertisementHash()) < *(rhs.getAdvertisementHash());
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
