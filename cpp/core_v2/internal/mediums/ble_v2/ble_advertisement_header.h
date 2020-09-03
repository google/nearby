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

#ifndef CORE_V2_INTERNAL_MEDIUMS_BLE_V2_BLE_ADVERTISEMENT_HEADER_H_
#define CORE_V2_INTERNAL_MEDIUMS_BLE_V2_BLE_ADVERTISEMENT_HEADER_H_

#include <string>

#include "platform_v2/base/byte_array.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// Represents the format of the Mediums BLE Advertisement Header used in
// Advertising + Discovery.
//
// [VERSION][NUM_SLOTS][SERVICE_ID_BLOOM_FILTER][ADVERTISEMENT_HASH]
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

  BleAdvertisementHeader() = default;
  BleAdvertisementHeader(Version version, int num_slots,
                         const ByteArray &service_id_bloom_filter,
                         const ByteArray &advertisement_hash);
  explicit BleAdvertisementHeader(
      const std::string &ble_advertisement_header_string);
  BleAdvertisementHeader(const BleAdvertisementHeader &) = default;
  BleAdvertisementHeader &operator=(const BleAdvertisementHeader &) = default;
  BleAdvertisementHeader(BleAdvertisementHeader &&) = default;
  BleAdvertisementHeader &operator=(BleAdvertisementHeader &&) = default;
  ~BleAdvertisementHeader() = default;

  // Produces an encoded binary string which can be decoded by the explicit
  // constructor. The returned string is empty if BleAdvertisementHeader is not
  // valid - false on IsValid().
  explicit operator std::string() const;
  bool operator<(const BleAdvertisementHeader &rhs) const;

  bool IsValid() const { return version_ == Version::kV2; }
  Version GetVersion() const { return version_; }
  int GetNumSlots() const { return num_slots_; }
  ByteArray GetServiceIdBloomFilter() const { return service_id_bloom_filter_; }
  ByteArray GetAdvertisementHash() const { return advertisement_hash_; }

 private:
  static constexpr int kServiceIdBloomFilterLength = 10;
  static constexpr int kAdvertisementHashLength = 4;
  static constexpr int kMinAdvertisementHeaderLength =
      1 + kServiceIdBloomFilterLength + kAdvertisementHashLength;
  static constexpr int kVersionBitmask = 0x0E0;
  static constexpr int kNumSlotsBitmask = 0x01F;

  Version version_ = Version::kUndefined;
  int num_slots_;
  ByteArray service_id_bloom_filter_;
  ByteArray advertisement_hash_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_BLE_V2_BLE_ADVERTISEMENT_HEADER_H_
