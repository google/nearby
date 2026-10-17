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
#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_WINDOWS_PLATFORM_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_WINDOWS_PLATFORM_H_

#include <functional>
#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/device_info.h"
#include "location/nearby/sharing/lib/account/account_manager.h"
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/impl/common/offline_account_manager.h"
#include "sharing/internal/impl/common/preference_manager.h"
#include "sharing/internal/impl/windows/bluetooth_adapter.h"
#include "sharing/internal/impl/windows/fast_initiation_manager.h"

namespace nearby::sharing::windows {

// SharingPlatform for Windows desktop. Preferences are stored under
// %LOCALAPPDATA%\QuickShareArm64; there is no Google account.
class WindowsPlatform : public api::SharingPlatform {
 public:
  WindowsPlatform();

  void InitProductIdGetter(absl::string_view (*product_id_getter)()) override {}
  std::unique_ptr<nearby::api::NetworkMonitor> CreateNetworkMonitor(
      std::function<void(bool)> lan_connected_callback,
      std::function<void(bool)> internet_connected_callback) override;
  sharing::api::BluetoothAdapter& GetBluetoothAdapter() override {
    return bluetooth_adapter_;
  }
  nearby::api::FastInitBleBeacon& GetFastInitBleBeacon() override {
    return fast_init_ble_beacon_;
  }
  nearby::api::FastInitiationManager& GetFastInitiationManager() override {
    return fast_initiation_manager_;
  }
  std::unique_ptr<nearby::api::SystemInfo> CreateSystemInfo() override;
  std::unique_ptr<nearby::api::AppInfo> CreateAppInfo() override;
  sharing::api::PreferenceManager& GetPreferenceManager() override {
    return preference_manager_;
  }
  AccountManager& GetAccountManager() override { return account_manager_; }
  nearby::api::DeviceInfo& GetDeviceInfo() override { return *device_info_; }
  std::unique_ptr<sharing::api::PublicCertificateDatabase>
  CreatePublicCertificateDatabase(const FilePath& database_path) override;
  // Marks received files with Mark-of-the-Web (Internet zone).
  bool UpdateFileOriginMetadata(std::vector<FilePath>& file_paths) override;

 private:
  BluetoothAdapter bluetooth_adapter_;
  FastInitBleBeacon fast_init_ble_beacon_;
  FastInitiationManager fast_initiation_manager_{bluetooth_adapter_};
  PreferenceManager preference_manager_;
  OfflineAccountManager account_manager_;
  std::unique_ptr<nearby::api::DeviceInfo> device_info_;
};

}  // namespace nearby::sharing::windows

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_WINDOWS_PLATFORM_H_
