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

#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"

#include <inttypes.h>

#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/base_input_stream.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {
namespace mediums {

// These definitions are necessary before C++17.
constexpr int BleAdvertisementHeader::kAdvertisementHashByteLength;
constexpr int BleAdvertisementHeader::kServiceIdBloomFilterByteLength;
constexpr int BleAdvertisementHeader::kDefaultPsmValue;
constexpr int BleAdvertisementHeader::kPsmValueByteLength;

BleAdvertisementHeader::BleAdvertisementHeader(
    Version version, bool support_extended_advertisement, int num_slots,
    const ByteArray &service_id_bloom_filter,
    const ByteArray &advertisement_hash, int psm) {
  if (version != Version::kV2 || num_slots < 0 ||
      service_id_bloom_filter.size() != kServiceIdBloomFilterByteLength ||
      advertisement_hash.size() != kAdvertisementHashByteLength) {
    return;
  }

  version_ = version;
  support_extended_advertisement_ = support_extended_advertisement;
  num_slots_ = num_slots;
  service_id_bloom_filter_ = service_id_bloom_filter;
  advertisement_hash_ = advertisement_hash;
  psm_ = psm;
}

BleAdvertisementHeader::BleAdvertisementHeader(
    const ByteArray &ble_advertisement_header_bytes) {
  ByteArray advertisement_header_bytes =
      Base64Utils::Decode(ble_advertisement_header_bytes.AsStringView());
  if (advertisement_header_bytes.Empty()) {
    NEARBY_LOG(
        ERROR,
        "Cannot deserialize BLEAdvertisementHeader: failed Base64 decoding");
    return;
  }

  if (advertisement_header_bytes.size() < kMinAdvertisementHeaderLength) {
    NEARBY_LOG(ERROR,
               "Cannot deserialize BleAdvertisementHeader: expecting min %u "
               "raw bytes, got %" PRIu64 " instead",
               kMinAdvertisementHeaderLength,
               advertisement_header_bytes.size());
    return;
  }

  BaseInputStream base_input_stream(advertisement_header_bytes);
  // The first 1 byte is supposed to be the version and number of slots.
  auto version_and_num_slots_byte =
      static_cast<char>(base_input_stream.ReadUint8());
  // The upper 3 bits are supposed to be the version.
  version_ =
      static_cast<Version>((version_and_num_slots_byte & kVersionBitmask) >> 5);
  if (version_ != Version::kV2) {
    NEARBY_LOG(
        ERROR,
        "Cannot deserialize BleAdvertisementHeader: unsupported Version %d",
        version_);
    return;
  }
  // The next 1 bit is supposed to be the extended advertisement flag.
  support_extended_advertisement_ =
      ((version_and_num_slots_byte & kExtendedAdvertismentBitMask) >> 4) == 1;
  // The lower 4 bits are supposed to be the number of slots.
  num_slots_ = static_cast<int>(version_and_num_slots_byte & kNumSlotsBitmask);
  if (num_slots_ < 0) {
    version_ = Version::kUndefined;
    return;
  }

  // The next 10 bytes are supposed to be the service_id_bloom_filter.
  service_id_bloom_filter_ =
      base_input_stream.ReadBytes(kServiceIdBloomFilterByteLength);

  // The next 4 bytes are supposed to be the advertisement_hash.
  advertisement_hash_ =
      base_input_stream.ReadBytes(kAdvertisementHashByteLength);

  // The next 2 bytes are PSM value.
  if (base_input_stream.IsAvailable(kPsmValueByteLength)) {
    psm_ = static_cast<int>(base_input_stream.ReadUint16());
  }
}

BleAdvertisementHeader::operator ByteArray() const {
  if (!IsValid()) {
    return ByteArray();
  }

  // The first 3 bits are the Version.
  char version_and_num_slots_byte =
      (static_cast<char>(version_) << 5) & kVersionBitmask;
  // The next 1 bit is extended advertisement flag.
  version_and_num_slots_byte |=
      (static_cast<char>(support_extended_advertisement_) << 4) &
      kExtendedAdvertismentBitMask;
  // The next 5 bits are the number of slots.
  version_and_num_slots_byte |=
      static_cast<char>(num_slots_) & kNumSlotsBitmask;

  // Convert psm_ value to 2-bytes.
  ByteArray psm_bytes{kPsmValueByteLength};
  char *data = psm_bytes.data();
  data[0] = psm_ & 0xFF00;
  data[1] = psm_ & 0x00FF;

  // clang-format off
  std::string out = absl::StrCat(std::string(1, version_and_num_slots_byte),
                                 std::string(service_id_bloom_filter_),
                                 std::string(advertisement_hash_),
                                 std::string(psm_bytes));
  // clang-format on

  return ByteArray(Base64Utils::Encode(ByteArray(std::move(out))));
}

bool BleAdvertisementHeader::operator==(
    const BleAdvertisementHeader &rhs) const {
  return GetVersion() == rhs.GetVersion() &&
         IsSupportExtendedAdvertisement() ==
             rhs.IsSupportExtendedAdvertisement() &&
         GetNumSlots() == rhs.GetNumSlots() &&
         GetServiceIdBloomFilter() == rhs.GetServiceIdBloomFilter() &&
         GetAdvertisementHash() == rhs.GetAdvertisementHash() &&
         GetPsm() == rhs.GetPsm();
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
