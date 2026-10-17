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
#include "sharing/internal/impl/windows/windows_platform.h"

#include <windows.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "internal/base/file_path.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/windows/device_paths.h"
#include "internal/platform/logging.h"
#include "sharing/internal/impl/common/public_certificate_database.h"
#include "sharing/internal/impl/windows/app_info.h"
#include "sharing/internal/impl/windows/network_monitor.h"
#include "sharing/internal/impl/windows/system_info.h"

namespace nearby::sharing::windows {
namespace {

FilePath AppDataPath() {
  return nearby::platform::windows::GetLocalAppDataPath(
      FilePath(L"QuickShareArm64"));
}

// Internet zone, as browsers record downloads (urlmon URLZONE_INTERNET).
constexpr char kZoneIdentifier[] = "[ZoneTransfer]\r\nZoneId=3\r\n";

bool WriteZoneIdentifier(const FilePath& path) {
  std::wstring stream = path.ToWideString() + L":Zone.Identifier";
  HANDLE file = CreateFileW(stream.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  bool ok = WriteFile(file, kZoneIdentifier, sizeof(kZoneIdentifier) - 1,
                      &written, nullptr) &&
            written == sizeof(kZoneIdentifier) - 1;
  CloseHandle(file);
  return ok;
}

}  // namespace

WindowsPlatform::WindowsPlatform()
    : preference_manager_(
          nearby::api::ImplementationPlatform::CreatePreferencesManager(
              AppDataPath().ToString())),
      device_info_(nearby::api::ImplementationPlatform::CreateDeviceInfo()) {}

std::unique_ptr<nearby::api::NetworkMonitor>
WindowsPlatform::CreateNetworkMonitor(
    std::function<void(bool)> lan_connected_callback,
    std::function<void(bool)> internet_connected_callback) {
  return std::make_unique<NetworkMonitor>(
      std::move(lan_connected_callback),
      std::move(internet_connected_callback));
}

std::unique_ptr<nearby::api::SystemInfo> WindowsPlatform::CreateSystemInfo() {
  return std::make_unique<SystemInfo>();
}

std::unique_ptr<nearby::api::AppInfo> WindowsPlatform::CreateAppInfo() {
  return std::make_unique<AppInfo>();
}

std::unique_ptr<sharing::api::PublicCertificateDatabase>
WindowsPlatform::CreatePublicCertificateDatabase(
    const FilePath& database_path) {
  return std::make_unique<PublicCertificateDatabase>();
}

bool WindowsPlatform::UpdateFileOriginMetadata(
    std::vector<FilePath>& file_paths) {
  bool ok = true;
  for (const FilePath& path : file_paths) {
    if (!WriteZoneIdentifier(path)) {
      LOG(WARNING) << "Failed to mark " << path.ToString()
                   << " as downloaded: " << GetLastError();
      ok = false;
    }
  }
  return ok;
}

}  // namespace nearby::sharing::windows
