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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_FLAGS_NEARBY_CONNECTIONS_FEATURE_FLAGS_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_FLAGS_NEARBY_CONNECTIONS_FEATURE_FLAGS_H_

#include "absl/strings/string_view.h"
#include "internal/flags/flag.h"

namespace nearby {
namespace connections {
namespace config_package_nearby {

constexpr absl::string_view kConfigPackage = "nearby";

// The Nearby Connections features.
namespace nearby_connections_feature {

// LINT.IfChanged
// Disable/Enable BLE v2 in Nearby Connections SDK.
constexpr auto kEnableBleV2 =
    flags::Flag<bool>(kConfigPackage, "45401515", false);

// The timeout in millis to report peripheral device lost.
constexpr auto kBlePeripheralLostTimeoutMillis =
    flags::Flag<int64_t>(kConfigPackage, "45411439", 12000);

// Enable/Disable GATT query during scanning.
constexpr auto kEnableGattQueryInThread =
    flags::Flag<bool>(kConfigPackage, "45415261", false);

// Enable/Disable payload manager to skip chunk update.
constexpr auto kEnablePayloadManagerToSkipChunkUpdate =
    flags::Flag<bool>(kConfigPackage, "45415729", false);

// LINT.ThenChange(
//   //depot/google3/location/nearby/cpp/sharing/clients/cpp/nearby_sharing_service_adapter_dart.h,
//   //depot/google3/location/nearby/cpp/sharing/clients/cpp/nearby_sharing_service_adapter_dart.cc,
//   //depot/google3/location/nearby/cpp/sharing/clients/dart/platform/lib/ffi_types.dart,
//   //depot/google3/location/nearby/cpp/sharing/clients/dart/platform/lib/types/models.dart
// )

}  // namespace nearby_connections_feature
}  // namespace config_package_nearby
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_FLAGS_NEARBY_CONNECTIONS_FEATURE_FLAGS_H_
