// Copyright 2021-2023 Google LLC
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

#include "internal/platform/implementation/platform.h"

// clang-format off
#include <windows.h>
#include <winver.h>
#include <PathCch.h>
#include <knownfolders.h>
#include <psapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
// clang-format on

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "absl/base/attributes.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/platform/implementation/app_lifecycle_monitor.h"
#include "internal/platform/implementation/atomic_boolean.h"
#include "internal/platform/implementation/atomic_reference.h"
#include "internal/platform/implementation/awdl.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/condition_variable.h"
#include "internal/platform/implementation/count_down_latch.h"
#include "internal/platform/implementation/credential_storage.h"
#include "internal/platform/implementation/http_loader.h"
#include "internal/platform/implementation/input_file.h"
#include "internal/platform/implementation/mutex.h"
#include "internal/platform/implementation/output_file.h"
#include "internal/platform/implementation/scheduled_executor.h"
#include "internal/platform/implementation/shared/count_down_latch.h"
#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/implementation/wifi.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/implementation/windows/atomic_boolean.h"
#include "internal/platform/implementation/windows/atomic_reference.h"
#include "internal/platform/implementation/windows/ble.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/bluetooth_classic_medium.h"
#include "internal/platform/implementation/windows/condition_variable.h"
#include "internal/platform/implementation/windows/device_info.h"
#include "internal/platform/implementation/windows/device_paths.h"
#include "internal/platform/implementation/windows/file.h"
#include "internal/platform/implementation/windows/file_path.h"
#include "internal/platform/implementation/windows/http_loader.h"
#include "internal/platform/implementation/windows/mutex.h"
#include "internal/platform/implementation/windows/preferences_manager.h"
#include "internal/platform/implementation/windows/scheduled_executor.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/implementation/windows/submittable_executor.h"
#include "internal/platform/implementation/windows/timer.h"
#include "internal/platform/implementation/windows/wifi.h"
#include "internal/platform/implementation/windows/wifi_direct.h"
#include "internal/platform/implementation/windows/wifi_hotspot.h"
#include "internal/platform/implementation/windows/wifi_lan.h"
#include "internal/platform/logging.h"
#include "internal/platform/os_name.h"

namespace nearby::api {

std::string ImplementationPlatform::GetCustomSavePath(
    const std::string& save_path, const std::string& parent_folder,
    const std::string& file_name) {
  FilePath path{save_path};
  if (path.IsEmpty()) {
    path = nearby::platform::windows::GetDownloadsPath();
  }
  FilePath parent_folder_path{parent_folder};
  if (!parent_folder_path.IsEmpty()) {
    path.append(parent_folder_path);
  }
  FilePath file_path{file_name};
  if (!file_path.IsEmpty()) {
    path.append(file_path);
  }

  return windows::FilePath::GetCustomSavePath(path).ToString();
}

OSName ImplementationPlatform::GetCurrentOS() { return OSName::kWindows; }

std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(
    bool initial_value) {
  return std::make_unique<windows::AtomicBoolean>(initial_value);
}

std::unique_ptr<AtomicUint32> ImplementationPlatform::CreateAtomicUint32(
    std::uint32_t value) {
  return std::make_unique<windows::AtomicUint32>(value);
}

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(
    std::int32_t count) {
  return std::make_unique<shared::CountDownLatch>(count);
}

#pragma push_macro("CreateMutex")
#undef CreateMutex
std::unique_ptr<Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  return std::make_unique<windows::Mutex>(mode);
}
#pragma pop_macro("CreateMutex")

std::unique_ptr<ConditionVariable>
ImplementationPlatform::CreateConditionVariable(Mutex* mutex) {
  return std::make_unique<windows::ConditionVariable>(mutex);
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    const std::string& file_path) {
  return windows::IOFile::CreateInputFile(file_path);
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    const std::string& file_path) {
  FilePath path{file_path};
  FilePath folder_path = path.GetParentPath();
  // Verifies that a path is a valid directory.
  if (!Files::DirectoryExists(folder_path)) {
    if (!Files::CreateDirectories(folder_path)) {
      LOG(ERROR) << "Failed to create directory: " << folder_path.ToString();
      return nullptr;
    }
  }
  return windows::IOFile::CreateOutputFile(file_path);
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateSingleThreadExecutor() {
  return std::make_unique<windows::SubmittableExecutor>();
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateMultiThreadExecutor(
    std::int32_t max_concurrency) {
  return std::make_unique<windows::SubmittableExecutor>(max_concurrency);
}

std::unique_ptr<ScheduledExecutor>
ImplementationPlatform::CreateScheduledExecutor() {
  return std::make_unique<windows::ScheduledExecutor>();
}

std::unique_ptr<BluetoothAdapter>
ImplementationPlatform::CreateBluetoothAdapter() {
  return std::make_unique<windows::BluetoothAdapter>();
}

std::unique_ptr<BluetoothClassicMedium>
ImplementationPlatform::CreateBluetoothClassicMedium(
    nearby::api::BluetoothAdapter& adapter) {
  return std::make_unique<windows::BluetoothClassicMedium>(adapter);
}

std::unique_ptr<api::ble::BleMedium> ImplementationPlatform::CreateBleMedium(
    api::BluetoothAdapter& adapter) {
  return std::make_unique<windows::BleMedium>(adapter);
}

std::unique_ptr<api::CredentialStorage>
ImplementationPlatform::CreateCredentialStorage() {
  return nullptr;
}

std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() {
  return std::make_unique<windows::WifiMedium>();
}

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  return std::make_unique<windows::WifiLanMedium>();
}

std::unique_ptr<AwdlMedium> ImplementationPlatform::CreateAwdlMedium() {
  return nullptr;
}

std::unique_ptr<WifiHotspotMedium>
ImplementationPlatform::CreateWifiHotspotMedium() {
  return std::make_unique<windows::WifiHotspotMedium>();
}

std::unique_ptr<WifiDirectMedium>
ImplementationPlatform::CreateWifiDirectMedium() {
  return std::make_unique<windows::WifiDirectMedium>();
}

std::unique_ptr<AppLifecycleMonitor>
ImplementationPlatform::CreateAppLifecycleMonitor(
    std::function<void(AppLifecycleMonitor::AppLifecycleState)>
        state_updated_callback) {
  return nullptr;
}

absl::StatusOr<WebResponse> ImplementationPlatform::SendRequest(
    const WebRequest& request) {
  windows::HttpLoader http_loader{request};
  return http_loader.GetResponse();
}

std::unique_ptr<Timer> ImplementationPlatform::CreateTimer() {
  return std::make_unique<windows::Timer>();
}

std::unique_ptr<DeviceInfo> ImplementationPlatform::CreateDeviceInfo() {
  return std::make_unique<windows::DeviceInfo>();
}

std::unique_ptr<nearby::api::PreferencesManager>
ImplementationPlatform::CreatePreferencesManager(absl::string_view path) {
  return std::make_unique<windows::PreferencesManager>(FilePath{path});
}

}  // namespace nearby::api
