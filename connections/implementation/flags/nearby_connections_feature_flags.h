// Copyright 2024 Google LLC
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

// Mendel flags, auto-generated. DO NOT EDIT.
#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_FLAGS_NEARBY_CONNECTIONS_FEATURE_FLAGS_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_FLAGS_NEARBY_CONNECTIONS_FEATURE_FLAGS_H_

#include <cstdint>

#include "absl/strings/string_view.h"
#include "internal/flags/flag.h"

namespace nearby {
namespace connections {
namespace config_package_nearby {

constexpr absl::string_view kConfigPackage = "nearby";

namespace nearby_connections_feature {
// The timeout in millis to report peripheral device lost.
constexpr auto kBlePeripheralLostTimeoutMillis =
    flags::Flag<int64_t>(kConfigPackage, "45411439", 12000);
// When true, disable Bluetooth classic scanning.
constexpr auto kDisableBluetoothClassicScanning =
    flags::Flag<bool>(kConfigPackage, "45639961", false);
// Enable/Disable auto_reconnect feature.
constexpr auto kEnableAutoReconnect =
    flags::Flag<bool>(kConfigPackage, "45427690", false);
// Disable/Enable BLE v2 in Nearby Connections SDK.
constexpr auto kEnableBleV2 =
    flags::Flag<bool>(kConfigPackage, "45401515", false);
// Disable/Enable GATT query in thread in BLE V2.
// Manual edit: setting this to false for ChromeOS rollout as well.
constexpr auto kEnableGattQueryInThread =
    flags::Flag<bool>(kConfigPackage, "45415261", false);
// When true, enable instant on lost feature.
constexpr auto kEnableInstantOnLost =
    flags::Flag<bool>(kConfigPackage, "45642180", false);
// When true, enable multiplexing in NC.
constexpr auto kEnableMultiplex =
    flags::Flag<bool>(kConfigPackage, "45647946", false);
// Enable/Disable payload manager to skip chunk update.
constexpr auto kEnablePayloadManagerToSkipChunkUpdate =
    flags::Flag<bool>(kConfigPackage, "45415729", true);
// Enable/Disable payload-received-ack feature.
constexpr auto kEnablePayloadReceivedAck =
    flags::Flag<bool>(kConfigPackage, "45425840", false);
// Enable/Disable safe-to-disconnect feature.
constexpr auto kEnableSafeToDisconnect =
    flags::Flag<bool>(kConfigPackage, "45425789", false);
// by default, enable Wi-Fi Hotspot client.
constexpr auto kEnableWifiHotspotClient =
    flags::Flag<bool>(kConfigPackage, "45648734", true);
// Set the safe-to-disconnect version.
// Enable 1. safe-to-disconnect check 2. reserved 3. auto-reconnect 4.
// auto-resume 5. non-distance-constraint-recovery 6. payload_ack
constexpr auto kSafeToDisconnectVersion =
    flags::Flag<int64_t>(kConfigPackage, "45425841", 0);
// When true, use stable endpoint ID.
constexpr auto kUseStableEndpointId =
    flags::Flag<bool>(kConfigPackage, "45639298", false);

}  // namespace nearby_connections_feature
}  // namespace config_package_nearby
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_FLAGS_NEARBY_CONNECTIONS_FEATURE_FLAGS_H_
