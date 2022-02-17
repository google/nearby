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

#include <string>

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
namespace bleutils {

ABSL_CONST_INIT extern const absl::string_view kCopresenceServiceUuid;

ByteArray GenerateHash(const std::string& source, size_t size);
ByteArray GenerateServiceIdHash(const std::string& service_id);
ByteArray GenerateDeviceToken();
ByteArray GenerateAdvertisementHash(const ByteArray& advertisement_bytes);
std::string GenerateAdvertisementUuid(int slot);

}  // namespace bleutils
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_UTILS_H_
