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

#include "connections/implementation/mediums/ble_v2/instant_on_lost_advertisement.h"

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"

namespace nearby {
namespace connections {
namespace mediums {

namespace {
constexpr int kVersion = 0b1;
constexpr int kHashLength =
    BleAdvertisementHeader::kAdvertisementHashByteLength;
constexpr int kVersionMask = 0x0e0;
// The header length for the instant on lost advertisement is 1 byte.
constexpr int kTotalLength = 1 + kHashLength;
}  // namespace

absl::StatusOr<InstantOnLostAdvertisement>
InstantOnLostAdvertisement::CreateFromHash(
    absl::string_view advertisement_hash) {
  if (advertisement_hash.length() != kHashLength) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot create instant on loss advertisement from "
                        "invalid hash length %d.",
                        advertisement_hash.length()));
  }
  return InstantOnLostAdvertisement(advertisement_hash);
}

std::string InstantOnLostAdvertisement::ToBytes() const {
  // 1. Header
  uint8_t header_byte = ((kVersion << 5) & kVersionMask);
  // 2. Hash
  return absl::StrFormat("%c%s", header_byte, advertisement_hash_);
}

absl::StatusOr<InstantOnLostAdvertisement>
InstantOnLostAdvertisement::CreateFromBytes(absl::string_view bytes) {
  if (bytes.length() != kTotalLength) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Cannot create instant on loss advertisement due to invalid length %d",
        bytes.length()));
  }
  // 1. Check header.
  uint8_t header_byte = bytes[0];
  int version = (header_byte & kVersionMask) >> 5;
  if (version != kVersion) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Cannot create instant on loss advertisement from invalid version %d.",
        version));
  }
  // 2. Get hash, skipping the header (size 1).
  return InstantOnLostAdvertisement(bytes.substr(1, kHashLength));
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
