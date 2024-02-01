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
#ifndef THIRD_PARTY_NEARBY_SHARING_FLAGS_NEARBY_SHARING_FEATURE_FLAGS_H_
#define THIRD_PARTY_NEARBY_SHARING_FLAGS_NEARBY_SHARING_FEATURE_FLAGS_H_

#include "absl/strings/string_view.h"
#include "internal/flags/flag.h"

namespace nearby {
namespace sharing {
namespace config_package_nearby {

constexpr absl::string_view kConfigPackage = "nearby";

namespace nearby_sharing_feature {
// Enable/disable background scanning
constexpr auto kEnableBackgroundScanning = flags::Flag<bool>(kConfigPackage, "45418904", true);
// Enable/disable the use of BLE as a connection medium.
constexpr auto kEnableBleForTransfer = flags::Flag<bool>(kConfigPackage, "45427466", false);
// Disable/Enable BLE v2 in Nearby Sharing SDK.
constexpr auto kEnableBleV2 = flags::Flag<bool>(kConfigPackage, "45401516", false);
// Enable/disable certificates dump
constexpr auto kEnableCertificatesDump = flags::Flag<bool>(kConfigPackage, "45409184", false);
// Enable/disable components refactor.
constexpr auto kEnableComponentsRefactor = flags::Flag<bool>(kConfigPackage, "45412090", true);
// Enable/disable dumping feature flags
constexpr auto kEnableDumpingFeatureFlags = flags::Flag<bool>(kConfigPackage, "45415713", true);
// Disable/Enable estimated time remaining UI in Nearby Share app.
constexpr auto kEnableEstimatedTimeRemainingUi = flags::Flag<bool>(kConfigPackage, "45408998", true);
// Enable/disable NL_LOG(FATAL) from crashing the app. If set to false then fatal logs do not crash and only result in a regular log.
constexpr auto kEnableFatalLog = flags::Flag<bool>(kConfigPackage, "45411907", false);
// Enable/disable logging battery usage
constexpr auto kEnableLoggingBatteryUsage = flags::Flag<bool>(kConfigPackage, "45409353", false);
// Enable/disable logging for foreground receiving
constexpr auto kEnableLoggingForegroundReceiving = flags::Flag<bool>(kConfigPackage, "45409411", false);
// Enable/disable logging additional system info metrics
constexpr auto kEnableLoggingSystemInfoMetrics = flags::Flag<bool>(kConfigPackage, "45412418", true);
// Enable/disable WebRTC medium in Nearby Share
constexpr auto kEnableMediumWebRtc = flags::Flag<bool>(kConfigPackage, "45418905", false);
// Enable/disable WifiLan medium in Nearby Share
constexpr auto kEnableMediumWifiLan = flags::Flag<bool>(kConfigPackage, "45418906", true);
// Enable/disable Nearby Sharing functionality
constexpr auto kEnableNearbySharing = flags::Flag<bool>(kConfigPackage, "45418903", true);
// Replace std::async with platform thread
constexpr auto kEnablePlatformThreadToNearbyClient = flags::Flag<bool>(kConfigPackage, "45411213", true);
// Enable/disable QR Code UI
constexpr auto kEnableQrCodeUi = flags::Flag<bool>(kConfigPackage, "45417647", false);
// Enable/disable retry/resume transfer for partial files.
constexpr auto kEnableRetryResumeTransfer = flags::Flag<bool>(kConfigPackage, "45411589", false);
// Enable/disable self share feature in Nearby Share
constexpr auto kEnableSelfShare = flags::Flag<bool>(kConfigPackage, "45418907", true);
// Enable/disable self share UI in Nearby Share
constexpr auto kEnableSelfShareUi = flags::Flag<bool>(kConfigPackage, "45418908", false);
// Enable/disable sending desktop events
constexpr auto kEnableSendingDesktopEvents = flags::Flag<bool>(kConfigPackage, "45459748", false);
// Disable/Enable tips carousel UI in Nearby Share app.
constexpr auto kEnableTipsCarouselUi = flags::Flag<bool>(kConfigPackage, "45411567", false);
// Enable/disable optimization for transfer cancellation.
constexpr auto kEnableTransferCancellationOptimization = flags::Flag<bool>(kConfigPackage, "45429881", false);
// Disable/enable the WebRTC medium in Nearby Sharing SDK.
constexpr auto kEnableWebrtcMedium = flags::Flag<bool>(kConfigPackage, "45411620", false);
// Set the logging level in Nearby Sharing SDK. The default logging level is WARNING. The mapping of logging level to number:VERBOSE: -1, INFO: 0, WARNING: 1, ERROR: 2, FATAL: 3
constexpr auto kLoggingLevel = flags::Flag<int64_t>(kConfigPackage, "45401358", 1);
// When true, the sender will not require confirming the ukey2 token.
constexpr auto kSenderSkipsConfirmation = flags::Flag<bool>(kConfigPackage, "45411353", true);
// Share Zwieback ID between Phenotype and Clearcut client in Nearby Share for Rasta metrics
constexpr auto kShareZwiebackBtwPhenotypeAndClearcut = flags::Flag<bool>(kConfigPackage, "45419546", false);
// Show Admin mode warning message in the app
constexpr auto kShowAdminModeWarning = flags::Flag<bool>(kConfigPackage, "45410558", false);
// Show/hide auto app start setting.
constexpr auto kShowAutoAppStartSetting = flags::Flag<bool>(kConfigPackage, "45411601", false);
// Enable/disable auto-update on settings page
constexpr auto kShowAutoUpdateSetting = flags::Flag<bool>(kConfigPackage, "45409033", false);
// Suppress Fast Initiation HUN by switching from kNotify to kSilent
constexpr auto kSuppressFastInitHun = flags::Flag<bool>(kConfigPackage, "45409586", true);
// Update track
constexpr auto kUpdateTrack = flags::Flag<absl::string_view>(kConfigPackage, "45409861", "");

}  // namespace nearby_sharing_feature
}  // namespace config_package_nearby
}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_FLAGS_NEARBY_SHARING_FEATURE_FLAGS_H_
