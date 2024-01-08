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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_SHARING_PLATFORM_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_SHARING_PLATFORM_H_

#include <functional>
#include <memory>

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
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
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/api/shell.h"
#include "sharing/internal/api/system_info.h"
#include "sharing/internal/api/wifi_adapter.h"

namespace nearby::sharing::api {

class MockSharingPlatform : public SharingPlatform {
 public:
  MockSharingPlatform() = default;
  MockSharingPlatform(const MockSharingPlatform&) = delete;
  MockSharingPlatform& operator=(const MockSharingPlatform&) = delete;
  ~MockSharingPlatform() override = default;

  MOCK_METHOD(void, InitLogging, (), (override));

  MOCK_METHOD(void, UpdateLoggingLevel, (), (override));

  MOCK_METHOD(
      std::unique_ptr<nearby::api::NetworkMonitor>, CreateNetworkMonitor,
      (std::function<void(nearby::api::NetworkMonitor::ConnectionType, bool)>
           callback),
      (override));

  MOCK_METHOD(nearby::sharing::api::BluetoothAdapter&, GetBluetoothAdapter, (),
              (override));

  MOCK_METHOD(nearby::sharing::api::WifiAdapter&, GetWifiAdapter, (),
              (override));

  MOCK_METHOD(void, LaunchDefaultBrowserFromURL,
              (absl::string_view url,
               std::function<void(absl::Status)> callback),
              (override));

  MOCK_METHOD(nearby::api::Shell&, GetShell, (), (override));

  MOCK_METHOD(nearby::api::FastInitBleBeacon&, GetFastInitBleBeacon, (),
              (override));

  MOCK_METHOD(nearby::api::FastInitiationManager&, GetFastInitiationManager, (),
              (override));

  MOCK_METHOD(void, CopyText,
              (absl::string_view text,
               std::function<void(absl::Status)> callback),
              (override));

  MOCK_METHOD(std::unique_ptr<nearby::api::SystemInfo>, CreateSystemInfo, (),
              (override));

  MOCK_METHOD(std::unique_ptr<nearby::api::AppInfo>, CreateAppInfo, (),
              (override));

  MOCK_METHOD(PreferenceManager&, GetPreferenceManager, (), (override));

  MOCK_METHOD(AccountManager&, GetAccountManager, (), (override));
  MOCK_METHOD(TaskRunner&, GetDefaultTaskRunner, (), (override));
  MOCK_METHOD(nearby::DeviceInfo&, GetDeviceInfo, (), (override));
  MOCK_METHOD(std::unique_ptr<PublicCertificateDatabase>,
              CreatePublicCertificateDatabase,
              (absl::string_view database_path), (override));
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_SHARING_PLATFORM_H_
