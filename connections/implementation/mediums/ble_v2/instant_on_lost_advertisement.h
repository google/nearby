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

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace nearby {
namespace connections {
namespace mediums {

// Represents an advertisement indicating that a previous BleAdvertisement is no
// longer valid.
//
// Format:
// [VERSION][5 bits reserved][ADVERTISEMENT HASH]
class InstantOnLostAdvertisement {
 public:
  // Creates an on lost advertisement from an advertisement hash.
  static absl::StatusOr<InstantOnLostAdvertisement> CreateFromHash(
      absl::string_view advertisement_hash);

  // Creates an InstantOnLostAdvertisement from raw bytes received over-the-air.
  static absl::StatusOr<InstantOnLostAdvertisement> CreateFromBytes(
      absl::string_view bytes);

  // Returns this instant-on-lost-advertisement in raw string
  // (non human-readable) format.
  // NOTE: Even though this function returns a string, this is not a UTF-8
  // string, as it contains raw bytes.
  std::string ToBytes() const;

  std::string GetHash() const { return advertisement_hash_; }

 private:
  explicit InstantOnLostAdvertisement(absl::string_view hash)
      : advertisement_hash_(std::string(hash)) {}

  const std::string advertisement_hash_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_BLE_V2_INSTANT_ON_LOST_ADVERTISEMENT_H_
