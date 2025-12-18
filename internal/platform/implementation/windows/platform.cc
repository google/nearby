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
#include "internal/platform/implementation/app_lifecycle_monitor.h"
#undef StrCat  // Remove the Windows macro definition
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
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
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/implementation/windows/atomic_boolean.h"
#include "internal/platform/implementation/windows/atomic_reference.h"
#include "internal/platform/implementation/windows/ble.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/bluetooth_classic_medium.h"
#include "internal/platform/implementation/windows/condition_variable.h"
#include "internal/platform/implementation/windows/device_info.h"
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
#include "internal/platform/payload_id.h"

namespace nearby {
namespace api {

namespace {

constexpr char kNCRelativePath[] = "Google/Nearby/Connections";

std::string GetApplicationName(DWORD pid) {
  HANDLE handle =
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                  pid);  // Modify pid to the pid of your application
  if (!handle) {
    return "";
  }

  std::string szProcessName("", MAX_PATH);
  DWORD len = MAX_PATH;

  if (NULL != handle) {
    GetModuleFileNameExA(handle, nullptr, szProcessName.data(), len);
  }

  szProcessName.resize(szProcessName.find_first_of('\0') + 1);

  auto just_the_file_name_and_ext = szProcessName.substr(
      szProcessName.find_last_of('\\') + 1,
      szProcessName.length() - szProcessName.find_last_of('\\') + 1);

  return just_the_file_name_and_ext.substr(
      0, just_the_file_name_and_ext.find_last_of('.'));
}

}  // namespace

std::string ImplementationPlatform::GetCustomSavePath(
    const std::string& parent_folder, const std::string& file_name) {
  auto parent = windows::string_utils::StringToWideString(parent_folder);
  auto file = windows::string_utils::StringToWideString(file_name);

  return windows::string_utils::WideStringToString(
      windows::FilePath::GetCustomSavePath(parent, file));
}

std::string ImplementationPlatform::GetDownloadPath(
    const std::string& parent_folder, const std::string& file_name) {
  auto parent = windows::string_utils::StringToWideString(parent_folder);
  auto file = windows::string_utils::StringToWideString(file_name);

  return windows::string_utils::WideStringToString(
      windows::FilePath::GetDownloadPath(parent, file));
}

std::string ImplementationPlatform::GetDownloadPath(
    const std::string& file_name) {
  std::wstring fake_parent_path;
  auto file = windows::string_utils::StringToWideString(file_name);

  return windows::string_utils::WideStringToString(
      windows::FilePath::GetDownloadPath(fake_parent_path, file));
}

std::string ImplementationPlatform::GetAppDataPath(
    const std::string& file_name) {
  PWSTR basePath;

  HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT,
                                        nullptr, &basePath);
  if (result != S_OK) {
    return file_name;
  }

  std::wstring app_data_path{basePath};
  CoTaskMemFree(basePath);
  std::string app_data_path_utf8 =
      windows::string_utils::WideStringToString(app_data_path);
  std::replace(app_data_path_utf8.begin(), app_data_path_utf8.end(), '\\', '/');
  return absl::StrCat(app_data_path_utf8, "/", kNCRelativePath, "/", file_name);
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

ABSL_DEPRECATED("This interface will be deleted in the near future.")
std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    PayloadId payload_id) {
  std::string file_name(std::to_string(payload_id));
  return windows::IOFile::CreateInputFile(GetDownloadPath(file_name));
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    const std::string& file_path) {
  return windows::IOFile::CreateInputFile(file_path);
}

ABSL_DEPRECATED("This interface will be deleted in the near future.")
std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    PayloadId payload_id) {
  std::string parent_folder("");
  std::string file_name(std::to_string(payload_id));
  return windows::IOFile::CreateOutputFile(
      GetDownloadPath(parent_folder, file_name));
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

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<api::ble::BleMedium> ImplementationPlatform::CreateBleMedium(
    api::BluetoothAdapter& adapter) {
  return std::make_unique<windows::BleMedium>(adapter);
}

std::unique_ptr<api::CredentialStorage>
ImplementationPlatform::CreateCredentialStorage() {
  return nullptr;
}

// TODO(b/184975123): replace with real implementation.
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

// TODO(b/261663238) replace with real implementation.
std::unique_ptr<WebRtcMedium> ImplementationPlatform::CreateWebRtcMedium() {
  return nullptr;
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

}  // namespace api
}  // namespace nearby
