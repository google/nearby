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

#include <string>

namespace location {
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

}  // namespace

const absl::string_view kCopresenceServiceUuid =
    "0000FEF3-0000-1000-8000-00805F9B34FB";

ByteArray GenerateHash(const std::string& source, size_t size) {
  return Utils::Sha256Hash(source, size);
}

ByteArray GenerateServiceIdHash(const std::string& service_id) {
  return Utils::Sha256Hash(service_id, BlePacket::kServiceIdHashLength);
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

std::string GenerateAdvertisementUuid(int slot) {
  if (slot < 0) {
    return {};
  }
  return std::string(Uuid(kAdvertisementUuidMsb, kAdvertisementUuidLsb | slot));
}

}  // namespace bleutils
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
