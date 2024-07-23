// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_BLE_V2_INSTANT_ON_LOST_ADVERTISEMENT_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_BLE_V2_INSTANT_ON_LOST_ADVERTISEMENT_H_

#include <list>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace nearby {
namespace connections {
namespace mediums {

// Represents the format of the BLE instant on lost Advertisement used in
// Advertising + Discovery.
//
// <p>[ADVERTISEMENT TYPE 4 bits][1 bit unused][VERSION 3 bits] [5 bits
// unused][HASH COUNT 3 bits] [ADVERTISEMENT_HASH 4 Bytes]x1~6
class InstantOnLostAdvertisement {
 public:
  // The BLE advertisement available size is 27 - 3 (BT header) - 2 (Our Header)
  // = 22 bytes so we can put at most 5 hashes in one advertisement.
  static constexpr int kMaxHashCount = 5;

  // Creates an on lost advertisement from an advertisement hash.
  static absl::StatusOr<InstantOnLostAdvertisement> CreateFromHashes(
      const std::list<std::string>& hashes);

  // Creates an InstantOnLostAdvertisement from raw bytes received over-the-air.
  static absl::StatusOr<InstantOnLostAdvertisement> CreateFromBytes(
      absl::string_view bytes);

  // Returns this instant-on-lost-advertisement in raw string
  // (non human-readable) format.
  // NOTE: Even though this function returns a string, this is not a UTF-8
  // string, as it contains raw bytes.
  std::string ToBytes() const;

  std::list<std::string> hashes() const { return hashes_; }

 private:
  explicit InstantOnLostAdvertisement(const std::list<std::string>& hashes)
      : hashes_(hashes) {}

  std::list<std::string> hashes_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_BLE_V2_INSTANT_ON_LOST_ADVERTISEMENT_H_
