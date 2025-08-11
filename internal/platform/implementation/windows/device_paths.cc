// Copyright 2025 Google LLC
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

#define _WIN32_WINNT _WIN32_WINNT_WIN10
#include "internal/platform/implementation/windows/device_paths.h"

#include <shlobj_core.h>
#include <windows.h>

#include "absl/strings/string_view.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"

namespace nearby::platform::windows {
namespace {

constexpr absl::string_view kLogsRelativePath = "Google\\Nearby\\Sharing\\Logs";
constexpr absl::string_view kCrashDumpsRelativePath =
    "Google\\Nearby\\Sharing\\CrashDumps";

FilePath GetAppDataPath(GUID folder_id, const FilePath& app_path) {
  FilePath app_data_path;
  PWSTR path = nullptr;
  HRESULT result = SHGetKnownFolderPath(folder_id, KF_FLAG_DEFAULT,
                                        /*hToken=*/nullptr, &path);
  if (result == S_OK) {
    app_data_path = FilePath(path);
    app_data_path.append(app_path);
  } else {
    app_data_path = Files::GetTemporaryDirectory();
  }
  CoTaskMemFree(path);
  return app_data_path;
}
}  // namespace

FilePath GetLocalAppDataPath(const FilePath& app_path) {
  return GetAppDataPath(FOLDERID_LocalAppData, app_path);
}

FilePath GetLogPath() {
  return GetLocalAppDataPath(FilePath(kLogsRelativePath));
}

FilePath GetCrashDumpPath() {
  return GetLocalAppDataPath(FilePath(kCrashDumpsRelativePath));
}

}  // namespace nearby::platform::windows
