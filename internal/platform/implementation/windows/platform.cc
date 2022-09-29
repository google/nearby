// Copyright 2021 Google LLC
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

#include <windows.h>
#include <winver.h>
#include <PathCch.h>
#include <knownfolders.h>
#include <psapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>

#include <memory>
#include <sstream>
#include <string>

#include "internal/platform/implementation/shared/count_down_latch.h"
#include "internal/platform/implementation/windows/atomic_boolean.h"
#include "internal/platform/implementation/windows/atomic_reference.h"
#include "internal/platform/implementation/windows/ble.h"
#include "internal/platform/implementation/windows/ble_v2.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/bluetooth_classic_medium.h"
#include "internal/platform/implementation/windows/cancelable.h"
#include "internal/platform/implementation/windows/condition_variable.h"
#include "internal/platform/implementation/windows/executor.h"
#include "internal/platform/implementation/windows/file.h"
#include "internal/platform/implementation/windows/future.h"
#include "internal/platform/implementation/windows/listenable_future.h"
#include "internal/platform/implementation/windows/log_message.h"
#include "internal/platform/implementation/windows/mutex.h"
#include "internal/platform/implementation/windows/scheduled_executor.h"
#include "internal/platform/implementation/windows/server_sync.h"
#include "internal/platform/implementation/windows/settable_future.h"
#include "internal/platform/implementation/windows/submittable_executor.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/webrtc.h"
#include "internal/platform/implementation/windows/wifi.h"
#include "internal/platform/implementation/windows/wifi_hotspot.h"
#include "internal/platform/implementation/windows/wifi_lan.h"

namespace location {
namespace nearby {
namespace api {

namespace {
constexpr absl::string_view kUpOneLevel("/..");

std::string GetDownloadPathInternal(absl::string_view parent_folder,
                                    absl::string_view file_name) {
  PWSTR basePath;

  // Retrieves the full path of a known folder identified by the folder's
  // KNOWNFOLDERID.
  // https://docs.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
  SHGetKnownFolderPath(
      FOLDERID_Downloads,  //  rfid: A reference to the KNOWNFOLDERID that
                           //  identifies the folder.
      0,           // dwFlags: Flags that specify special retrieval options.
      NULL,        // hToken: An access token that represents a particular user.
      &basePath);  // ppszPath: When this method returns, contains the address
                   // of a pointer to a null-terminated Unicode string that
                   // specifies the path of the known folder. The calling
                   // process is responsible for freeing this resource once it
                   // is no longer needed by calling CoTaskMemFree, whether
                   // SHGetKnownFolderPath succeeds or not.
  size_t bufferSize;
  wcstombs_s(&bufferSize, NULL, 0, basePath, 0);
  std::string fullpathUTF8(bufferSize - 1, '\0');
  wcstombs_s(&bufferSize, fullpathUTF8.data(), bufferSize, basePath, _TRUNCATE);

  std::string parent_folder_path(parent_folder);

  std::replace(fullpathUTF8.begin(), fullpathUTF8.end(), '\\', '/');

  // If parent_folder starts with a \\ or /, then strip it
  while (!parent_folder_path.empty() && (*parent_folder_path.begin() == '\\' ||
                                         *parent_folder_path.begin() == '/')) {
    parent_folder_path.erase(0, 1);
  }

  // If parent_folder ends with a \\ or /, then strip it
  while (!parent_folder_path.empty() && (*parent_folder_path.rbegin() == '\\' ||
                                         *parent_folder_path.rbegin() == '/')) {
    parent_folder_path.erase(parent_folder_path.size() - 1, 1);
  }

  std::string file_name_path(file_name);

  // If file_name starts with a \\, then strip it
  while (!file_name_path.empty() &&
         (*file_name_path.begin() == '\\' || *file_name_path.begin() == '/')) {
    file_name_path.erase(0, 1);
  }

  // If file_name ends with a \\, then strip it
  while (!file_name_path.empty() && (*file_name_path.rbegin() == '\\' ||
                                     *file_name_path.rbegin() == '/')) {
    file_name_path.erase(file_name_path.size() - 1, 1);
  }

  CoTaskMemFree(basePath);

  std::string path("");

  if (parent_folder_path.empty() && file_name_path.empty()) {
    return fullpathUTF8;
  }
  if (parent_folder_path.empty()) {
    path += fullpathUTF8 + "/" + file_name_path;
    return path;
  }
  if (file_name_path.empty()) {
    path += fullpathUTF8 + "/" + parent_folder_path;
    return path;
  }

  path += fullpathUTF8 + "/" + parent_folder_path + "/" + file_name_path;
  return path;
}

void SanitizePath(std::string& path) {
  size_t pos = std::string::npos;
  // Search for the substring in string in a loop until nothing is found
  while ((pos = path.find(kUpOneLevel.data())) != std::string::npos) {
    // If found then erase it from string
    path.erase(pos, kUpOneLevel.size());
  }
}

// If the file already exists we add " (x)", where x is an incrementing number,
// starting at 1, using the next non-existing number, to the file name, just
// before the first dot, or at the end if no dot. The absolute path is returned.
std::string CreateOutputFileWithRename(absl::string_view path) {
  // Remove any /..
  std::string sanitized_path(path);
  std::replace(sanitized_path.begin(), sanitized_path.end(), '\\', '/');
  SanitizePath(sanitized_path);

  auto last_separator = sanitized_path.find_last_of('/');
  std::string folder(sanitized_path.substr(0, last_separator));
  std::string file_name(sanitized_path.substr(last_separator));

  int count = 0;

  // Locate the last dot
  auto first = file_name.find_last_of('.');

  if (first == std::string::npos) {
    first = file_name.size();
  }

  // Break the string at the dot.
  auto file_name1 = file_name.substr(0, first);
  auto file_name2 = file_name.substr(first);

  // Construct the target file name
  std::string target(sanitized_path);

  std::fstream file;

  // Open file as std::wstring
  file.open(windows::string_to_wstring(target),
            std::fstream::binary | std::fstream::in);

  // While we successfully open the file, keep incrementing the count.
  while (!(file.rdstate() & std::ifstream::failbit)) {
    file.close();
#undef StrCat
    target = absl::StrCat(folder, file_name1, " (", ++count, ")", file_name2);
    file.clear();
    file.open(windows::string_to_wstring(target),
              std::fstream::binary | std::fstream::in);
  }

  // The above leaves the file open, so close it.
  file.close();

  return target;
}

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

bool FolderExists(const std::string& folder_name) {
  DWORD ftyp = GetFileAttributesA(folder_name.c_str());

  if (ftyp == INVALID_FILE_ATTRIBUTES) {
    return false;  // something is wrong with your path!
  }

  if (ftyp & FILE_ATTRIBUTE_DIRECTORY) {
    return true;
  }  // this is a directory!

  return false;  // this is not a directory!
}

}  // namespace

std::string ImplementationPlatform::GetDownloadPath(
    absl::string_view parent_folder, absl::string_view file_name) {
  return CreateOutputFileWithRename(
      GetDownloadPathInternal(parent_folder, file_name));
}

std::string ImplementationPlatform::GetDownloadPath(
    absl::string_view file_name) {
  std::string fake_parent_path;
  return GetDownloadPathInternal(fake_parent_path, file_name);
}

std::string ImplementationPlatform::GetAppDataPath(
    absl::string_view file_name) {
  PWSTR basePath;

  // Retrieves the full path of a known folder identified by the folder's
  // KNOWNFOLDERID.
  // https://docs.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
  SHGetKnownFolderPath(
      FOLDERID_ProgramData,  //  rfid: A reference to the KNOWNFOLDERID that
                             //  identifies the folder.
      0,           // dwFlags: Flags that specify special retrieval options.
      NULL,        // hToken: An access token that represents a particular user.
      &basePath);  // ppszPath: When this method returns, contains the address
                   // of a pointer to a null-terminated Unicode string that
                   // specifies the path of the known folder. The calling
                   // process is responsible for freeing this resource once it
                   // is no longer needed by calling CoTaskMemFree, whether
                   // SHGetKnownFolderPath succeeds or not.
  size_t bufferSize;
  wcstombs_s(&bufferSize, NULL, 0, basePath, 0);
  std::string fullpathUTF8(bufferSize - 1, '\0');
  wcstombs_s(&bufferSize, fullpathUTF8.data(), bufferSize, basePath, _TRUNCATE);
  CoTaskMemFree(basePath);

  // Get the application image name
  auto app_name = GetApplicationName(GetCurrentProcessId());

  // Check if our folder exists
  std::replace(fullpathUTF8.begin(), fullpathUTF8.end(), '\\', '/');

  std::stringstream path("");

  path << fullpathUTF8.c_str() << "/" << app_name.c_str() << "/"
       << file_name.data();

  return path.str();
}

OSName ImplementationPlatform::GetCurrentOS() { return OSName::kWindows; }

std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(
    bool initial_value) {
  return absl::make_unique<windows::AtomicBoolean>();
}

std::unique_ptr<AtomicUint32> ImplementationPlatform::CreateAtomicUint32(
    std::uint32_t value) {
  return absl::make_unique<windows::AtomicUint32>();
}

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(
    std::int32_t count) {
  return absl::make_unique<shared::CountDownLatch>(count);
}

std::unique_ptr<Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  return absl::make_unique<windows::Mutex>(mode);
}

std::unique_ptr<ConditionVariable>
ImplementationPlatform::CreateConditionVariable(Mutex* mutex) {
  return absl::make_unique<windows::ConditionVariable>(mutex);
}

ABSL_DEPRECATED("This interface will be deleted in the near future.")
std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    PayloadId payload_id, std::int64_t total_size) {
  std::string parent_folder("");
  std::string file_name(std::to_string(payload_id));
  return windows::IOFile::CreateInputFile(GetDownloadPath(file_name),
                                          total_size);
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    absl::string_view file_path, size_t size) {
  return windows::IOFile::CreateInputFile(file_path, size);
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
    absl::string_view file_path) {
  std::string path(file_path);

  std::string folder_path = path.substr(0, path.find_last_of('/'));
  // Verifies that a path is a valid directory.
  // https://docs.microsoft.com/en-us/windows/win32/api/shlwapi/nf-shlwapi-pathisdirectorya
  if (!PathIsDirectoryA(folder_path.data())) {
    // This function creates a file system folder whose fully qualified path is
    // given by pszPath. If one or more of the intermediate folders do not
    // exist, they are created as well.
    // https://docs.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shcreatedirectoryexa
    int result = SHCreateDirectoryExA(0, folder_path.data(), nullptr);
  }

  return windows::IOFile::CreateOutputFile(file_path);
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<LogMessage> ImplementationPlatform::CreateLogMessage(
    const char* file, int line, LogMessage::Severity severity) {
  return absl::make_unique<windows::LogMessage>(file, line, severity);
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateSingleThreadExecutor() {
  return absl::make_unique<windows::SubmittableExecutor>();
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateMultiThreadExecutor(
    std::int32_t max_concurrency) {
  return absl::make_unique<windows::SubmittableExecutor>(max_concurrency);
}

std::unique_ptr<ScheduledExecutor>
ImplementationPlatform::CreateScheduledExecutor() {
  return absl::make_unique<windows::ScheduledExecutor>();
}

std::unique_ptr<BluetoothAdapter>
ImplementationPlatform::CreateBluetoothAdapter() {
  return absl::make_unique<windows::BluetoothAdapter>();
}

std::unique_ptr<BluetoothClassicMedium>
ImplementationPlatform::CreateBluetoothClassicMedium(
    nearby::api::BluetoothAdapter& adapter) {
  return absl::make_unique<windows::BluetoothClassicMedium>(adapter);
}

std::unique_ptr<BleMedium> ImplementationPlatform::CreateBleMedium(
    BluetoothAdapter& adapter) {
  return absl::make_unique<windows::BleMedium>(adapter);
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<api::ble_v2::BleMedium>
ImplementationPlatform::CreateBleV2Medium(api::BluetoothAdapter& adapter) {
  return std::make_unique<windows::BleV2Medium>(adapter);
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<ServerSyncMedium>
ImplementationPlatform::CreateServerSyncMedium() {
  return std::unique_ptr<windows::ServerSyncMedium>();
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() {
  return std::make_unique<windows::WifiMedium>();
}

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  return absl::make_unique<windows::WifiLanMedium>();
}

std::unique_ptr<WifiHotspotMedium>
ImplementationPlatform::CreateWifiHotspotMedium() {
  return std::make_unique<windows::WifiHotspotMedium>();
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<WebRtcMedium> ImplementationPlatform::CreateWebRtcMedium() {
  return absl::make_unique<windows::WebRtcMedium>();
}

}  // namespace api
}  // namespace nearby
}  // namespace location
