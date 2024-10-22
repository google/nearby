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
#include <list>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {
namespace mediums {

namespace {
constexpr int kAdvertisementType = 0b0001;
constexpr int kVersion = 1;
// Type + Version + Hash Count
constexpr int kMetadataLength = 2;
constexpr int kAdvertisementHashLength =
    BleAdvertisementHeader::kAdvertisementHashByteLength;

constexpr int kTypeBitmask = 0x0F0;
constexpr int kVersionBitmask = 0x007;
constexpr int kHashCountBitmask = 0x007;

bool IsSupportedVersion(int version) { return version == kVersion; }

}  // namespace

absl::StatusOr<InstantOnLostAdvertisement>
InstantOnLostAdvertisement::CreateFromHashes(
    const std::list<std::string>& hashes) {
  for (auto& hash : hashes) {
    if (hash.length() != kAdvertisementHashLength) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Cannot create instant on loss advertisement from "
                          "invalid hashes"));
    }
  }

  return InstantOnLostAdvertisement(hashes);
}

std::string InstantOnLostAdvertisement::ToBytes() const {
  if (hashes_.empty() || hashes_.size() > kMaxHashCount) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to convert hashes due to hash "
                          "size is not valid, size = "
                       << hashes_.size();
    return "";
  }

  // 1. Header.
  uint8_t header = ((kAdvertisementType << 4) & kTypeBitmask);
  header |= (kVersion & kVersionBitmask);

  // 2. Hash counts.
  uint8_t count = static_cast<uint8_t>(hashes_.size() & kHashCountBitmask);
  std::string result = absl::StrFormat("%c%c", header, count);
  for (const auto& hash : hashes_) {
    if (hash.length() != kAdvertisementHashLength) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Failed to convert hashes to advertisement due "
                            "to invalid hash : "
                         << absl::BytesToHexString(hash);
      return "";
    }
    absl::StrAppend(&result, hash);
  }
  return result;
}

absl::StatusOr<InstantOnLostAdvertisement>
InstantOnLostAdvertisement::CreateFromBytes(absl::string_view bytes) {
  if (bytes.length() < kMetadataLength) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Cannot create instant on loss advertisement due to invalid length %d",
        bytes.length()));
  }

  // 1. Parse header.
  uint8_t header = bytes[0];
  int type = (header & kTypeBitmask) >> 4;
  if (type != kAdvertisementType) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse due to header type %d.", type));
  }

  // 2. Parse version.
  int version = header & kVersionBitmask;
  if (!IsSupportedVersion(version)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse due to unknown version %d.", version));
  }

  // 3. Parse hash counts.
  uint8_t count = (bytes[1] & kHashCountBitmask);
  if (count * kAdvertisementHashLength + 2 != bytes.length()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse due to incorrect count %d.", count));
  }

  // 4. Parse hashes.
  std::list<std::string> hashes;
  for (int i = 2; i < bytes.length(); i += kAdvertisementHashLength) {
    hashes.push_back(std::string(bytes.substr(i, kAdvertisementHashLength)));
  }

  return InstantOnLostAdvertisement(hashes);
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
