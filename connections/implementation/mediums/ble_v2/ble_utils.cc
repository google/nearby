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

#include "connections/implementation/mediums/ble_v2/ble_utils.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/base/attributes.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/optional.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"
#include "connections/implementation/mediums/ble_v2/ble_packet.h"
#include "connections/implementation/mediums/utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/prng.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace bleutils {

namespace {

// These two values make up the base UUID we use when advertising a slot.
// The base is an all zero Version-3 name-based UUID. To turn this into an
// advertisement slot UUID, we simply OR the least significant bits with the
// slot number.
//
// More info about the format can be found here:
// https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based)
constexpr std::int64_t kAdvertisementUuidMsb = 0x0000000000003000;
constexpr std::int64_t kAdvertisementUuidLsb = 0x8000000000000000;

// The most significant bits and the least significant bits for the Copresence
// UUID.
const std::uint64_t kCopresenceServiceUuidMsb = 0x0000FEF300001000;
const std::uint64_t kCopresenceServiceUuidLsb = 0x800000805F9B34FB;

// Creates a string as a space separated listing of hex bytes with [] at the
// beginning and the end.
//
// This is the legacy input for hash in version (kV1). It is for testing only.
std::string StringToPrintableHexString(const std::string& source) {
  // Print out the byte array as a space separated listing of hex bytes.
  std::string out = "[ ";
  for (const char& c : source) {
    absl::StrAppend(&out, absl::StrFormat("%#04x ", c));
  }
  absl::StrAppend(&out, "]");
  return out;
}

}  // namespace

ABSL_CONST_INIT const Uuid kCopresenceServiceUuid(kCopresenceServiceUuidMsb,
                                  kCopresenceServiceUuidLsb);

ByteArray GenerateHash(const std::string& source, size_t size) {
  return Utils::Sha256Hash(source, size);
}

ByteArray GenerateServiceIdHash(const std::string& service_id,
                                BleAdvertisement::Version version) {
  switch (version) {
      // legacy hash for testing only.
    case BleAdvertisement::Version::kV1:
      return Utils::Sha256Hash(StringToPrintableHexString(service_id),
                               BlePacket::kServiceIdHashLength);
    case BleAdvertisement::Version::kV2:
      [[fallthrough]];
    case BleAdvertisement::Version::kUndefined:
      [[fallthrough]];
    default:
      // Use the latest known hashing scheme.
      return Utils::Sha256Hash(service_id, BlePacket::kServiceIdHashLength);
  }
}

ByteArray GenerateDeviceToken() {
  return Utils::Sha256Hash(std::to_string(Prng().NextUint32()),
                           mediums::BleAdvertisement::kDeviceTokenLength);
}

ByteArray GenerateAdvertisementHash(const ByteArray& advertisement_bytes) {
  return Utils::Sha256Hash(
      advertisement_bytes,
      BleAdvertisementHeader::kAdvertisementHashByteLength);
}

// NOLINTNEXTLINE(google3-legacy-absl-backports)
absl::optional<Uuid> GenerateAdvertisementUuid(int slot) {
  if (slot < 0) {
    return absl::nullopt;  // NOLINT
  }
  return Uuid(kAdvertisementUuidMsb, kAdvertisementUuidLsb | slot);
}

}  // namespace bleutils
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
