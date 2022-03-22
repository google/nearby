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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_V2_UTILS_H_
#define CORE_INTERNAL_MEDIUMS_BLE_V2_UTILS_H_

#include "absl/strings/str_format.h"
#include "connections/implementation/mediums/ble_v2//ble_advertisement.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"
#include "connections/implementation/mediums/ble_v2/ble_packet.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/implementation/mediums/uuid.h"
#include "internal/platform/prng.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

class BleUtils {
 public:
  static constexpr absl::string_view kCopresenceServiceUuid =
      "0000FEF3-0000-1000-8000-00805F9B34FB";

  static ByteArray GenerateHash(const std::string& source, size_t size) {
    return Utils::Sha256Hash(source, size);
  }

  static ByteArray GenerateServiceIdHash(
      const std::string& service_id,
      BleAdvertisement::Version version = BleAdvertisement::Version::kV2) {
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

  static std::string StringToPrintableHexString(const std::string& source) {
    // Print out the byte array as a space separated listing of hex bytes.
    std::string out = "[ ";
    for (const char& c : source) {
      absl::StrAppend(&out, absl::StrFormat("%#04x ", c));
    }
    absl::StrAppend(&out, "]");
    return out;
  }

  static ByteArray GenerateDeviceToken() {
    return Utils::Sha256Hash(std::to_string(Prng().NextUint32()),
                             mediums::BleAdvertisement::kDeviceTokenLength);
  }

  static ByteArray GenerateAdvertisementHash(
      const ByteArray& advertisement_bytes) {
    return Utils::Sha256Hash(advertisement_bytes,
                             BleAdvertisementHeader::kAdvertisementHashLength);
  }

  static std::string GenerateAdvertisementUuid(int slot) {
    return std::string(
        Uuid(kAdvertisementUuidMsb, kAdvertisementUuidLsb | slot));
  }

 private:
  // These two values make up the base UUID we use when advertising a slot.
  // The base is an all zero Version-3 name-based UUID. To turn this into an
  // advertisement slot UUID, we simply OR the least significant bits with the
  // slot number.
  //
  // More info about the format can be found here:
  // https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based)
  static constexpr std::int64_t kAdvertisementUuidMsb = 0x0000000000003000;
  static constexpr std::int64_t kAdvertisementUuidLsb = 0x8000000000000000;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_UTILS_H_
