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
// Disable instant on lost on BLE without extended feature.
constexpr auto kDisableInstantOnLostOnBleWithoutExtended =
    flags::Flag<bool>(kConfigPackage, "45687098", true);
// When true, enable advertising for instant on lost feature.
constexpr auto kEnableAdvertisingForInstantOnLost =
    flags::Flag<bool>(kConfigPackage, "45708614", false);
// Enable/Disable auto_reconnect feature.
constexpr auto kEnableAutoReconnect =
    flags::Flag<bool>(kConfigPackage, "45427690", false);
// Enable/Disable AWDL in Nearby connections SDK.
constexpr auto kEnableAwdl =
    flags::Flag<bool>(kConfigPackage, "45690762", false);
// Disable/Enable BLE L2CAP in Nearby Connections SDK.
constexpr auto kEnableBleL2cap =
    flags::Flag<bool>(kConfigPackage, "45685706", false);
// Enable/Disable BLE medium injection.
constexpr auto kEnableBleMediumInjection =
    flags::Flag<bool>(kConfigPackage, "45743128", false);
// Disable/Enable BLE v2 in Nearby Connections SDK.
constexpr auto kEnableBleV2 =
    flags::Flag<bool>(kConfigPackage, "45401515", true);
// Enable/Disable DCT advertising/scanning specification.
constexpr auto kEnableDct =
    flags::Flag<bool>(kConfigPackage, "45697202", false);
// Disable/Enable dynamic role switch in Nearby Connections SDK.
constexpr auto kEnableDynamicRoleSwitch =
    flags::Flag<bool>(kConfigPackage, "45696452", true);
// Enable/Disable GATT client disconnection.
constexpr auto kEnableGattClientDisconnection =
    flags::Flag<bool>(kConfigPackage, "45698964", false);
// Disable/Enable GATT query in thread in BLE V2.
constexpr auto kEnableGattQueryInThread =
    flags::Flag<bool>(kConfigPackage, "45415261", true);
// When true, enable instant on lost feature.
constexpr auto kEnableInstantOnLost =
    flags::Flag<bool>(kConfigPackage, "45642180", false);
// When true, enable multiplexing in NC.
constexpr auto kEnableMultiplex =
    flags::Flag<bool>(kConfigPackage, "45647946", false);
// Enable/disable multiplex in NC for AWDL.
constexpr auto kEnableMultiplexAwdl =
    flags::Flag<bool>(kConfigPackage, "45690761", false);
// When true, enable multiplexing in NC for Bluetooth.
constexpr auto kEnableMultiplexBluetooth =
    flags::Flag<bool>(kConfigPackage, "45676646", false);
// When true, enable multiplexing in NC for Wifi.
constexpr auto kEnableMultiplexWifiLan =
    flags::Flag<bool>(kConfigPackage, "45676647", false);
// Enable/Disable preferences for Nearby Connections.
constexpr auto kEnableNearbyConnectionsPreferences =
    flags::Flag<bool>(kConfigPackage, "45732423", false);
// Enable/Disable payload manager to skip chunk update.
constexpr auto kEnablePayloadManagerToSkipChunkUpdate =
    flags::Flag<bool>(kConfigPackage, "45415729", true);
// Enable/Disable payload-received-ack feature.
constexpr auto kEnablePayloadReceivedAck =
    flags::Flag<bool>(kConfigPackage, "45425840", false);
// Enable/Disable GATT query for extended advertisement.
constexpr auto kEnableReadGattForExtendedAdvertisement =
    flags::Flag<bool>(kConfigPackage, "45718229", false);
// Enable/Disable safe-to-disconnect feature.
constexpr auto kEnableSafeToDisconnect =
    flags::Flag<bool>(kConfigPackage, "45425789", false);
// When true, enable scanning for instant on lost feature.
constexpr auto kEnableScanningForInstantOnLost =
    flags::Flag<bool>(kConfigPackage, "45708613", false);
// Stop BLE_V2 scanning when upgrading to WIFI Hotspot or WFD.
constexpr auto kEnableStopBleScanningOnWifiUpgrade =
    flags::Flag<bool>(kConfigPackage, "45687902", false);
// Enable/Disable Wi-Fi Direct in Nearby connections SDK.
constexpr auto kEnableWifiDirect =
    flags::Flag<bool>(kConfigPackage, "45741157", false);
// by default, enable Wi-Fi Hotspot client.
constexpr auto kEnableWifiHotspotClient =
    flags::Flag<bool>(kConfigPackage, "45648734", true);
// Default max transmit packet size for medium.
constexpr auto kMediumDefaultMaxTransmitPacketSize =
    flags::Flag<int64_t>(kConfigPackage, "45669529", 65536);
// Default max allowed read bytes for medium.
constexpr auto kMediumMaxAllowedReadBytes =
    flags::Flag<int64_t>(kConfigPackage, "45669530", 1048576);
// Disable/Enable refactor of BLE/L2CAP in Nearby Connections SDK.
constexpr auto kRefactorBleL2cap =
    flags::Flag<bool>(kConfigPackage, "45737079", false);
// Set the safe-to-disconnect version.
// 0. Disabled all. 1. safe-to-disconnect 2. reserved 3. auto-reconnect
// 4. auto-resume  5. non-distance-constraint-recovery 6. payload_ack
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
