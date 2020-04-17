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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_ADVERTISEMENT_HEADER_H_
#define CORE_INTERNAL_MEDIUMS_BLE_ADVERTISEMENT_HEADER_H_

#include "platform/byte_array.h"
#include "platform/ptr.h"

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
class BLEAdvertisementHeader {
 public:
  // Versions of the BLEAdvertisementHeader.
  struct Version {
    enum Value {
      V2 = 2,
      // Version is only allocated 3 bits in the BLEAdvertisementHeader, so this
      // can never go beyond V7.
      //
      // V1 is not present because it's an old format used in Nearby Connections
      // before this logic was pushed down into Nearby Mediums. V1 put
      // everything in the service data, while V2 puts the data inside a GATT
      // characteristic so the two are not compatible.
    };
  };

  static ConstPtr<BLEAdvertisementHeader> fromString(
      const std::string &ble_advertisement_header_string);

  static std::string asString(Version::Value version, std::uint32_t num_slots,
                              ConstPtr<ByteArray> service_id_bloom_filter,
                              ConstPtr<ByteArray> advertisement_hash);

  static const std::uint32_t kServiceIdBloomFilterLength;
  static const std::uint32_t kAdvertisementHashLength;

  ~BLEAdvertisementHeader();

  Version::Value getVersion() const;
  std::uint32_t getNumSlots() const;
  ConstPtr<ByteArray> getServiceIdBloomFilter() const;
  ConstPtr<ByteArray> getAdvertisementHash() const;

  // Operator overloads when comparing ConstPtr<BLEAdvertisementHeader>.
  bool operator<(const BLEAdvertisementHeader &rhs) const;

 private:
  // DiscoveredPeripheralTracker needs to be a friend of this class because it
  // directly calls the constructor (the Java code keeps the constructor package
  // private).
  // Calling the constuctor directly allows us to avoid the unnessary extra
  // calls to parse and decode to get the BLEAdvertisementHeader.
  template <typename>
  friend class DiscoveredPeripheralTracker;

  static Version::Value parseVersionFromByte(std::uint16_t byte);
  static std::uint32_t parseNumSlotsFromByte(std::uint16_t byte);

  static const std::uint32_t kVersionAndNumSlotsLength;
  static const std::uint32_t kMinAdvertisementHeaderLength;
  static const std::uint16_t kVersionBitmask;
  static const std::uint16_t kNumSlotsBitmask;

  BLEAdvertisementHeader(Version::Value version, std::uint32_t num_slots,
                         ConstPtr<ByteArray> service_id_bloom_filter,
                         ConstPtr<ByteArray> advertisement_hash);

  static void serializeVersionByte(char *version_byte_write_ptr,
                                   Version::Value version);
  static void serializeNumSlots(char *num_slots_byte_write_ptr,
                                std::uint32_t num_slots);

  const Version::Value version_;
  const uint32_t num_slots_;
  ScopedPtr<ConstPtr<ByteArray> > service_id_bloom_filter_;
  ScopedPtr<ConstPtr<ByteArray> > advertisement_hash_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_BLE_ADVERTISEMENT_HEADER_H_
