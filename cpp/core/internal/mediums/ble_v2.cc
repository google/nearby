// Copyright 2022 Google LLC
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

#include "third_party/nearby/cpp/core/internal/mediums/ble_v2.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "third_party/nearby/cpp/core/internal/mediums/ble_v2/ble_advertisement.h"
#include "third_party/nearby/cpp/core/internal/mediums/ble_v2/ble_advertisement_header.h"
#include "third_party/nearby/cpp/core/internal/mediums/ble_v2/bloom_filter.h"
#include "third_party/nearby/cpp/core/internal/mediums/utils.h"
#include "third_party/nearby/cpp/platform/base/prng.h"

namespace location {
namespace nearby {
namespace connections {

ByteArray BleV2::GenerateAdvertisementHash(
    const ByteArray& advertisement_bytes) {
  return Utils::Sha256Hash(
      advertisement_bytes,
      mediums::BleAdvertisementHeader::kAdvertisementHashLength);
}

ByteArray BleV2::GenerateHash(const std::string& source, size_t size) {
  return Utils::Sha256Hash(source, size);
}

ByteArray BleV2::GenerateDeviceToken() {
  return Utils::Sha256Hash(std::to_string(Prng().NextUint32()),
                           mediums::BleAdvertisement::kDeviceTokenLength);
}

BleV2::BleV2(BluetoothRadio& radio) : radio_(radio) {}

ByteArray BleV2::CreateAdvertisementHeader() {
  // Create a randomized dummy service id to anonymize the header with.
  ByteArray dummy_service_id_bytes =
      Utils::GenerateRandomBytes(kDummyServiceIdLength);
  std::string dummy_service_id{dummy_service_id_bytes};

  // Create a bloom filter with the dummy service id.
  mediums::BloomFilter<
      mediums::BleAdvertisementHeader::kServiceIdBloomFilterLength>
      bloom_filter;
  bloom_filter.Add(dummy_service_id);

  // Add the service id for each GATT advertisement into the bloom filter, while
  // also creating a hash of dummyServiceIdBytes + all GATT advertisements.
  ByteArray advertisement_hash =
      GenerateAdvertisementHash(dummy_service_id_bytes);
  int num_slots = 0;
  for (const auto& item : gatt_advertisement_info.gatt_advertisements) {
    std::string service_id = item.second.first;
    ByteArray gatt_advertisment = item.second.second;
    bloom_filter.Add(service_id);

    // Compute the next hash by taking the hash of [previous hash] + [next
    // advertisement data].
    ByteArray previous_advertisement_hash = advertisement_hash;
    std::string advertisement_bodies =
        absl::StrCat(std::string(previous_advertisement_hash),
                     std::string(gatt_advertisment));

    advertisement_hash =
        GenerateAdvertisementHash(ByteArray{std::move(advertisement_bodies)});
    num_slots++;
  }

  return ByteArray(mediums::BleAdvertisementHeader(
      mediums::BleAdvertisementHeader::Version::kV2,
      /*extended_advertisement=*/false, num_slots, ByteArray(bloom_filter),
      advertisement_hash, /*psm=*/0));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
