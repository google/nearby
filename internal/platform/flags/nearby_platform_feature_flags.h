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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_FLAGS_NEARBY_PLATFORM_FEATURE_FLAGS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_FLAGS_NEARBY_PLATFORM_FEATURE_FLAGS_H_

#include <cstdint>

#include "absl/strings/string_view.h"
#include "internal/flags/flag.h"

namespace nearby {
namespace platform {
namespace config_package_nearby {

constexpr absl::string_view kConfigPackage = "nearby";

// The Nearby Platform features.
namespace nearby_platform_feature {

// Disable/Enable win32 socket implementation for Wi-Fi hotspot.
constexpr auto kEnableHotspotWin32Socket =
    flags::Flag<bool>(kConfigPackage, "45401992", true);

// Disable/Enable GATT feature in BLE V2.
constexpr auto kEnableBleV2Gatt =
    flags::Flag<bool>(kConfigPackage, "45415180", false);

// Disable/Enable GATT feature in BLE V2.
constexpr auto kEnableBleV2GattOnNonExtendedDevice =
    flags::Flag<bool>(kConfigPackage, "45415267", false);

// The maximum scanning times for available hotspots.
constexpr auto kWifiHotspotScanMaxRetries =
    flags::Flag<int64_t>(kConfigPackage, "45415883", 3);

// The maximum IP check times during Wi-Fi hotspot connection.
constexpr auto kWifiHotspotCheckIpMaxRetries =
    flags::Flag<int64_t>(kConfigPackage, "45415884", 10);

// The interval between 2 IP check attempts.
constexpr auto kWifiHotspotCheckIpIntervalMillis =
    flags::Flag<int64_t>(kConfigPackage, "45415885", 500);

// The maximum connection times to remote Wi-Fi hotspot.
constexpr auto kWifiHotspotConnectionMaxRetries =
    flags::Flag<int64_t>(kConfigPackage, "45415886", 3);

// The interval between 2 connectin attempts.
constexpr auto kWifiHotspotConnectionIntervalMillis =
    flags::Flag<int64_t>(kConfigPackage, "45415887", 2000);

// The connection timeout to remote Wi-Fi hotspot.
constexpr auto kWifiHotspotConnectionTimeoutMillis =
    flags::Flag<int64_t>(kConfigPackage, "45415888", 10000);

// Enable/Disable Intel PIe SDK to query/set WIFI feature.
constexpr auto kEnableIntelPieSdk =
    flags::Flag<bool>(kConfigPackage, "45428547", false);

// Enable/Disable new Bluetooth refactor
constexpr auto kEnableNewBluetoothRefactor =
    flags::Flag<bool>(kConfigPackage, "45615156", false);

// Enable/Disable task scheduler for ScheduledExecutor and timer
constexpr auto kEnableTaskScheduler =
    flags::Flag<bool>(kConfigPackage, "45643835", false);

}  // namespace nearby_platform_feature
}  // namespace config_package_nearby
}  // namespace platform
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_FLAGS_NEARBY_PLATFORM_FEATURE_FLAGS_H_
