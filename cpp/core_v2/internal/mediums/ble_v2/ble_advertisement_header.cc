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

#include "core_v2/internal/mediums/ble_v2/ble_advertisement_header.h"

#include <inttypes.h>

#include "platform_v2/base/base64_utils.h"
#include "platform_v2/base/base_input_stream.h"
#include "platform_v2/public/logging.h"
#include "absl/strings/str_cat.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

BleAdvertisementHeader::BleAdvertisementHeader(
    Version version, int num_slots, const ByteArray &service_id_bloom_filter,
    const ByteArray &advertisement_hash) {
  if (version != Version::kV2 || num_slots <= 0 ||
      service_id_bloom_filter.size() != kServiceIdBloomFilterLength ||
      advertisement_hash.size() != kAdvertisementHashLength) {
    return;
  }

  version_ = version;
  num_slots_ = num_slots;
  service_id_bloom_filter_ = service_id_bloom_filter;
  advertisement_hash_ = advertisement_hash;
}

BleAdvertisementHeader::BleAdvertisementHeader(
    const std::string &ble_advertisement_header_string) {
  ByteArray ble_advertisement_header_bytes =
      Base64Utils::Decode(ble_advertisement_header_string);

  if (ble_advertisement_header_bytes.Empty()) {
    NEARBY_LOG(
        ERROR,
        "Cannot deserialize BLEAdvertisementHeader: failed Base64 decoding");
    return;
  }

  if (ble_advertisement_header_bytes.size() < kMinAdvertisementHeaderLength) {
    NEARBY_LOG(ERROR,
               "Cannot deserialize BleAdvertisementHeader: expecting min %u "
               "raw bytes, got %" PRIu64 " instead",
               kMinAdvertisementHeaderLength,
               ble_advertisement_header_bytes.size());
    return;
  }

  BaseInputStream base_input_stream{ble_advertisement_header_bytes};
  // The first 1 byte is supposed to be the version and number of slots.
  auto version_and_pcp_byte = static_cast<char>(base_input_stream.ReadUint8());
  // The upper 3 bits are supposed to be the version.
  version_ =
      static_cast<Version>((version_and_pcp_byte & kVersionBitmask) >> 5);
  if (version_ != Version::kV2) {
    NEARBY_LOG(
        ERROR,
        "Cannot deserialize BleAdvertisementHeader: unsupported Version %d",
        version_);
    return;
  }
  // The lower 5 bits are supposed to be the number of slots.
  num_slots_ = static_cast<int>(version_and_pcp_byte & kNumSlotsBitmask);
  if (num_slots_ <= 0) {
    version_ = Version::kUndefined;
    return;
  }

  // The next 10 bytes are supposed to be the service_id_bloom_filter.
  service_id_bloom_filter_ =
      base_input_stream.ReadBytes(kServiceIdBloomFilterLength);

  // The next 4 bytes are supposed to be the advertisement_hash.
  advertisement_hash_ = base_input_stream.ReadBytes(kAdvertisementHashLength);
}

BleAdvertisementHeader::operator std::string() const {
  if (!IsValid()) {
    return "";
  }

  // The first 3 bits are the Version.
  char version_and_num_slots_byte =
      (static_cast<char>(version_) << 5) & kVersionBitmask;
  // The next 5 bits are the number of slots.
  version_and_num_slots_byte |=
      static_cast<char>(num_slots_) & kNumSlotsBitmask;

  // clang-format off
  std::string out = absl::StrCat(std::string(1, version_and_num_slots_byte),
                                 std::string(service_id_bloom_filter_),
                                 std::string(advertisement_hash_));
  // clang-format on

  return Base64Utils::Encode(ByteArray(std::move(out)));
}

bool BleAdvertisementHeader::operator<(
    const BleAdvertisementHeader &rhs) const {
  if (this->GetVersion() != rhs.GetVersion()) {
    return this->GetVersion() < rhs.GetVersion();
  }
  if (this->GetNumSlots() != rhs.GetNumSlots()) {
    return this->GetNumSlots() < rhs.GetNumSlots();
  }
  if (this->GetServiceIdBloomFilter() != rhs.GetServiceIdBloomFilter()) {
    return this->GetServiceIdBloomFilter() < rhs.GetServiceIdBloomFilter();
  }
  return this->GetAdvertisementHash() < rhs.GetAdvertisementHash();
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
