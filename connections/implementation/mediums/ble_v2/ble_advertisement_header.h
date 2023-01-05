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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_V2_BLE_ADVERTISEMENT_HEADER_H_
#define CORE_INTERNAL_MEDIUMS_BLE_V2_BLE_ADVERTISEMENT_HEADER_H_

#include <string>

#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace mediums {

// Represents the format of the Mediums BLE Advertisement Header used in
// Advertising + Discovery.
//
// [VERSION][NUM_SLOTS][SERVICE_ID_BLOOM_FILTER][ADVERTISEMENT_HASH][L2_CAP_PSM]
//
// See go/nearby-ble-design for more information.
//
// Note. The object constructed by default constructor or the parameterized
// constructor with invalid value(s) is treated as invalid instance. Caller
// should be responsible to call IsValid() to check the instance is invalid in
// advance before continue on.
class BleAdvertisementHeader {
 public:
  // Versions of the BleAdvertisementHeader.
  enum class Version {
    kUndefined = 0,
    kV1 = 1,
    kV2 = 2,
    // Version is only allocated 3 bits in the BleAdvertisementHeader, so this
    // can never go beyond V7.
    //
    // V1 is not present because it's an old format used in Nearby Connections
    // before this logic was pushed down into Nearby Mediums. V1 put
    // everything in the service data, while V2 puts the data inside a GATT
    // characteristic so the two are not compatible.
  };

  static constexpr int kAdvertisementHashByteLength = 4;
  static constexpr int kServiceIdBloomFilterByteLength = 10;
  static constexpr int kDefaultPsmValue = 0;
  static constexpr int kPsmValueByteLength = 2;

  // Hashable
  bool operator==(const BleAdvertisementHeader &rhs) const;
  template <typename H>
  friend H AbslHashValue(H h, const BleAdvertisementHeader &b) {
    return H::combine(std::move(h), b.version_,
                      b.support_extended_advertisement_, b.num_slots_,
                      b.service_id_bloom_filter_, b.advertisement_hash_,
                      b.psm_);
  }

  BleAdvertisementHeader() = default;
  BleAdvertisementHeader(Version version, bool support_extended_advertisement,
                         int num_slots,
                         const ByteArray &service_id_bloom_filter,
                         const ByteArray &advertisement_hash, int psm);
  explicit BleAdvertisementHeader(
      const ByteArray &ble_advertisement_header_bytes);
  BleAdvertisementHeader(const BleAdvertisementHeader &) = default;
  BleAdvertisementHeader &operator=(const BleAdvertisementHeader &) = default;
  BleAdvertisementHeader(BleAdvertisementHeader &&) = default;
  BleAdvertisementHeader &operator=(BleAdvertisementHeader &&) = default;
  ~BleAdvertisementHeader() = default;

  explicit operator ByteArray() const;

  bool IsValid() const { return version_ == Version::kV2; }
  Version GetVersion() const { return version_; }
  bool IsSupportExtendedAdvertisement() const {
    return support_extended_advertisement_;
  }
  int GetNumSlots() const { return num_slots_; }
  ByteArray GetServiceIdBloomFilter() const { return service_id_bloom_filter_; }
  ByteArray GetAdvertisementHash() const { return advertisement_hash_; }
  int GetPsm() const { return psm_; }
  void SetPsm(int psm) { psm_ = psm; }

 private:
  static constexpr int kVersionAndNumSlotsLength = 1;
  static constexpr int kMinAdvertisementHeaderLength =
      kVersionAndNumSlotsLength + kServiceIdBloomFilterByteLength +
      kAdvertisementHashByteLength;
  static constexpr int kVersionBitmask = 0x0E0;
  static constexpr int kExtendedAdvertismentBitMask = 0x010;
  static constexpr int kNumSlotsBitmask = 0x00F;

  Version version_ = Version::kUndefined;
  bool support_extended_advertisement_ = false;
  int num_slots_ = 0;
  ByteArray service_id_bloom_filter_;
  ByteArray advertisement_hash_;
  int psm_ = kDefaultPsmValue;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_BLE_ADVERTISEMENT_HEADER_H_
