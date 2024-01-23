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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SHARING_PLATFORM_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SHARING_PLATFORM_H_

#include <functional>
#include <memory>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/analytics/event_logger.h"
#include "internal/platform/device_info.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/task_runner.h"
#include "sharing/internal/api/app_info.h"
#include "sharing/internal/api/bluetooth_adapter.h"
#include "sharing/internal/api/fast_init_ble_beacon.h"
#include "sharing/internal/api/fast_initiation_manager.h"
#include "sharing/internal/api/network_monitor.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/api/public_certificate_database.h"
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/api/shell.h"
#include "sharing/internal/api/system_info.h"
#include "sharing/internal/api/wifi_adapter.h"

namespace nearby::sharing::api {

constexpr char kSharingPreferencesFilePath[] = "Google/Nearby/Sharing";

// Platform abstraction interface for NearbyShare cross-platform compatibility.
class SharingPlatform {
 public:
  virtual ~SharingPlatform() = default;

  // This function should only be called once.
  virtual void InitLogging() = 0;

  // Platform specific implementation to set default logging levels.
  virtual void UpdateLoggingLevel() = 0;

  virtual std::unique_ptr<nearby::api::NetworkMonitor> CreateNetworkMonitor(
      std::function<void(nearby::api::NetworkMonitor::ConnectionType, bool)>
          callback) = 0;

  virtual BluetoothAdapter& GetBluetoothAdapter() = 0;

  virtual WifiAdapter& GetWifiAdapter() = 0;

  virtual void LaunchDefaultBrowserFromURL(
      absl::string_view url, std::function<void(absl::Status)> callback) = 0;

  virtual nearby::api::Shell& GetShell() = 0;

  virtual nearby::api::FastInitBleBeacon& GetFastInitBleBeacon() = 0;

  virtual nearby::api::FastInitiationManager& GetFastInitiationManager() = 0;

  // Make calls to OS to copy text to clipboard
  //
  // @param text is a text to copy to clipboard.
  // @param callback
  //
  // If it is successfully copied, callback provided is executed with
  // absl::OkStatus. Otherwise, absl::InternalError.
  virtual void CopyText(absl::string_view text,
                        std::function<void(absl::Status)> callback) = 0;

  // Creates system information class. SystemInfo provides APIs to access
  // system information.
  virtual std::unique_ptr<nearby::api::SystemInfo> CreateSystemInfo() = 0;

  // Creates app information class. AppInfo provides APIs to access app
  // information, such as app version.
  virtual std::unique_ptr<nearby::api::AppInfo> CreateAppInfo() = 0;

  virtual PreferenceManager& GetPreferenceManager() = 0;
  virtual AccountManager& GetAccountManager() = 0;
  virtual TaskRunner& GetDefaultTaskRunner() = 0;
  virtual nearby::DeviceInfo& GetDeviceInfo() = 0;
  virtual std::unique_ptr<PublicCertificateDatabase>
  CreatePublicCertificateDatabase(absl::string_view database_path) = 0;

  virtual std::unique_ptr<SharingRpcClientFactory>
  CreateSharingRpcClientFactory(
      nearby::analytics::EventLogger* event_logger) = 0;
};
}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SHARING_PLATFORM_H_
