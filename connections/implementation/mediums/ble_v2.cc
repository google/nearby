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

#include "connections/implementation/mediums/ble_v2.h"

#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"
#include "connections/implementation/mediums/ble_v2/bloom_filter.h"
#include "connections/implementation/mediums/utils.h"
#include "internal/platform/prng.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

constexpr int kDummyServiceIdLength = 128;

ByteArray GenerateAdvertisementHash(const ByteArray& advertisement_bytes) {
  return Utils::Sha256Hash(
      advertisement_bytes,
      mediums::BleAdvertisementHeader::kAdvertisementHashLength);
}

}  // namespace

BleV2::BleV2(BluetoothRadio& radio)
    : radio_(radio), adapter_(radio_.GetBluetoothAdapter()) {}

ByteArray BleV2::CreateAdvertisementHeader() {
  // Create a randomized dummy service id to anonymize the header with.
  ByteArray dummy_service_id_bytes =
      Utils::GenerateRandomBytes(kDummyServiceIdLength);
  std::string dummy_service_id{dummy_service_id_bytes};

  mediums::BloomFilter<
      mediums::BleAdvertisementHeader::kServiceIdBloomFilterLength>
      bloom_filter;
  bloom_filter.Add(dummy_service_id);

  ByteArray advertisement_hash =
      GenerateAdvertisementHash(dummy_service_id_bytes);
  for (const auto& item : gatt_advertisements_) {
    const std::string& service_id = item.second.first;
    const ByteArray& gatt_advertisement = item.second.second;
    bloom_filter.Add(service_id);

    // Compute the next hash according to the algorithm in
    // https://source.corp.google.com/piper///depot/google3/java/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/mediums/bluetooth/BluetoothLowEnergy.java;rcl=428397891;l=1043
    ByteArray previous_advertisement_hash = advertisement_hash;
    std::string advertisement_bodies =
        absl::StrCat(previous_advertisement_hash.AsStringView(),
                     gatt_advertisement.AsStringView());

    advertisement_hash =
        GenerateAdvertisementHash(ByteArray(std::move(advertisement_bodies)));
  }

  return ByteArray(mediums::BleAdvertisementHeader(
      mediums::BleAdvertisementHeader::Version::kV2,
      /*extended_advertisement=*/false,
      /*num_slots=*/gatt_advertisements_.size(), ByteArray(bloom_filter),
      advertisement_hash, /*psm=*/0));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
