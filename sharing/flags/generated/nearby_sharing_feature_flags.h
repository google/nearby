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

#include <cstdint>

#include "absl/container/btree_map.h"
#include "absl/strings/string_view.h"
#include "internal/flags/flag.h"

namespace nearby {
namespace sharing {
namespace config_package_nearby {

constexpr absl::string_view kConfigPackage = "nearby";

namespace nearby_sharing_feature {
// Time to delay the endpoint loss in milliseconds.
constexpr auto kDelayEndpointLossMs =
    flags::Flag<int64_t>(kConfigPackage, "45632386", 500);
// Enable/disable the use of BLE as a connection medium.
constexpr auto kEnableBleForTransfer =
    flags::Flag<bool>(kConfigPackage, "45427466", false);
// Enable/disable certificates dump
constexpr auto kEnableCertificatesDump =
    flags::Flag<bool>(kConfigPackage, "45409184", false);
// Enable/disable WebRTC medium in Nearby Share
constexpr auto kEnableMediumWebRtc =
    flags::Flag<bool>(kConfigPackage, "45418905", false);
// Enable/disable WifiLan medium in Nearby Share
constexpr auto kEnableMediumWifiLan =
    flags::Flag<bool>(kConfigPackage, "45418906", true);
// Enable/disable retry/resume transfer for partial files.
constexpr auto kEnableRetryResumeTransfer =
    flags::Flag<bool>(kConfigPackage, "45411589", false);
// Enable/disable self share UI in Nearby Share
constexpr auto kEnableSelfShareUi =
    flags::Flag<bool>(kConfigPackage, "45418908", false);
// Enable/disable sending desktop events
constexpr auto kEnableSendingDesktopEvents =
    flags::Flag<bool>(kConfigPackage, "45459748", false);
// Disable/enable the WebRTC medium in Nearby Sharing SDK.
constexpr auto kEnableWebrtcMedium =
    flags::Flag<bool>(kConfigPackage, "45411620", false);
// Set the logging level in Nearby Sharing SDK. The default logging level is
// WARNING. The mapping of logging level to number: INFO: 0, WARNING: 1, ERROR:
// 2, FATAL: 3,  negative values are -(verbosity level).
constexpr auto kLoggingLevel =
    flags::Flag<int64_t>(kConfigPackage, "45401358", 1);
// When true, the sender will not require confirming the ukey2 token.
constexpr auto kSenderSkipsConfirmation =
    flags::Flag<bool>(kConfigPackage, "45411353", true);
// Enable/disable auto-update on settings page
constexpr auto kShowAutoUpdateSetting =
    flags::Flag<bool>(kConfigPackage, "45409033", false);
// When true, use gRpc client to access backend.
constexpr auto kUseGrpcClient =
    flags::Flag<bool>(kConfigPackage, "45630055", false);
// When true, delete the file payload which received unexpectedly.
constexpr auto kDeleteUnexpectedReceivedFileFix =
    flags::Flag<bool>(kConfigPackage, "45657036", false);
// The default time in milliseconds a cached entry can be in LOST state.
constexpr auto kDiscoveryCacheLostExpiryMs =
    flags::Flag<int64_t>(kConfigPackage, "45658774", 500);
// When true, enable Nearby Share gRPC async client.
constexpr auto kEnableNearbyShareGrpcAsyncClient =
    flags::Flag<bool>(kConfigPackage, "45726584", false);
// When true, enable wifi hotspot medium for HP Realtek devices.
constexpr auto kEnableWifiHotspotForHpRealtekDevices =
    flags::Flag<bool>(kConfigPackage, "45673628", false);
// The amount of time in milliseconds a share target stays in discovery cache in
// receive disabled state after a transfer.
constexpr auto kUnregisterTargetDiscoveryCacheLostExpiryMs =
    flags::Flag<int64_t>(kConfigPackage, "45663103", 10000);
// When true, enable alternate BLE service UUID for discovery.
constexpr auto kUseAlternateServiceUuidForDiscovery =
    flags::Flag<bool>(kConfigPackage, "45683539", false);
// Enable/disable QR Code UI
constexpr auto kEnableQrCodeUi =
    flags::Flag<bool>(kConfigPackage, "45417647", false);
// Show Admin mode warning message in the app
constexpr auto kShowAdminModeWarning =
    flags::Flag<bool>(kConfigPackage, "45410558", false);
// Update track
constexpr auto kUpdateTrack =
    flags::Flag<absl::string_view>(kConfigPackage, "45409861", "");
// Timeout between displays of the conflict banner.
constexpr auto kConflictBannerTimeout =
    flags::Flag<int64_t>(kConfigPackage, "45668886", 604800);
// Enable a persistent BETA label.
constexpr auto kEnableBetaLabel =
    flags::Flag<bool>(kConfigPackage, "45662570", true);
// Enable the info banner to display duplicate Quick Share apps.
constexpr auto kEnableConflictBanner =
    flags::Flag<bool>(kConfigPackage, "45661130", false);
// When true, enables use of Flutter hooks.
constexpr auto kEnableFlutterHooks =
    flags::Flag<bool>(kConfigPackage, "45720206", false);
// When true, enables the mini pulse animation.
constexpr auto kEnableMiniPulse =
    flags::Flag<bool>(kConfigPackage, "45724244", false);
// When true, enables responsive UI.
constexpr auto kEnableResponsiveUi =
    flags::Flag<bool>(kConfigPackage, "45727212", false);
// When true, enables UI experiments.
constexpr auto kEnableUiExperiments =
    flags::Flag<bool>(kConfigPackage, "45678202", false);
// When true, enables responsive UI.
constexpr auto kEnableResponsiveUi =
    flags::Flag<bool>(kConfigPackage, "45725853", false);

inline absl::btree_map<int, const flags::Flag<bool>&> GetBoolFlags() {
  return {
      {45427466, kEnableBleForTransfer},
      {45409184, kEnableCertificatesDump},
      {45418905, kEnableMediumWebRtc},
      {45418906, kEnableMediumWifiLan},
      {45411589, kEnableRetryResumeTransfer},
      {45418908, kEnableSelfShareUi},
      {45459748, kEnableSendingDesktopEvents},
      {45411620, kEnableWebrtcMedium},
      {45411353, kSenderSkipsConfirmation},
      {45409033, kShowAutoUpdateSetting},
      {45630055, kUseGrpcClient},
      {45657036, kDeleteUnexpectedReceivedFileFix},
      {45726584, kEnableNearbyShareGrpcAsyncClient},
      {45673628, kEnableWifiHotspotForHpRealtekDevices},
      {45683539, kUseAlternateServiceUuidForDiscovery},
      {45417647, kEnableQrCodeUi},
      {45410558, kShowAdminModeWarning},
      {45662570, kEnableBetaLabel},
      {45661130, kEnableConflictBanner},
      {45720206, kEnableFlutterHooks},
      {45724244, kEnableMiniPulse},
      {45727212, kEnableResponsiveUi},
      {45678202, kEnableUiExperiments},
      {45725853, kEnableResponsiveUi},
  };
}

inline absl::btree_map<int, const flags::Flag<int64_t>&> GetInt64Flags() {
  return {
      {45632386, kDelayEndpointLossMs},
      {45401358, kLoggingLevel},
      {45658774, kDiscoveryCacheLostExpiryMs},
      {45663103, kUnregisterTargetDiscoveryCacheLostExpiryMs},
      {45668886, kConflictBannerTimeout},
  };
}

inline absl::btree_map<int, const flags::Flag<double>&> GetDoubleFlags() {
  return {};
}

inline absl::btree_map<int, const flags::Flag<absl::string_view>&>
GetStringFlags() {
  return {
      {45409861, kUpdateTrack},
  };
}

}  // namespace nearby_sharing_feature
}  // namespace config_package_nearby
}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_FLAGS_NEARBY_SHARING_FEATURE_FLAGS_H_
