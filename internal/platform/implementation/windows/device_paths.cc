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

#include "internal/platform/implementation/windows/device_paths.h"

// clang-format off
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#include <windows.h>
#include <fileapi.h>
#include <shlobj_core.h>
// clang-format on

#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"

namespace nearby::platform::windows {
namespace {

constexpr absl::string_view kLogsRelativePath = "Google\\Nearby\\Sharing\\Logs";
constexpr absl::string_view kCrashDumpsRelativePath =
    "Google\\Nearby\\Sharing\\CrashDumps";

// Returns the absolute path of the given path.
// If the given path is already absolute, returns the path itself.
// On error, returns an empty path.
FilePath GetAbsolutePath(const FilePath& path) {
  if (path.IsAbsolute()) {
    return path;
  }
  std::wstring path_name = path.ToWideString();
  DWORD size = GetFullPathNameW(path_name.c_str(), 0, /*lpBuffer=*/nullptr,
                                /*lpFilePart=*/nullptr);
  if (size == 0) {
    return FilePath();
  }
  std::wstring absolute_path(size, L'\0');
  size = GetFullPathNameW(path_name.c_str(), size, absolute_path.data(),
                          /*lpFilePart=*/nullptr);
  if (size == 0) {
    return FilePath();
  }
  absolute_path.resize(size);
  return FilePath(absolute_path);
}

FilePath GetSystemPath(GUID folder_id, const FilePath& relative_path) {
  FilePath result_path;
  PWSTR path = nullptr;
  HRESULT result = SHGetKnownFolderPath(folder_id, KF_FLAG_DEFAULT,
                                        /*hToken=*/nullptr, &path);
  // On some systems, the system folder have been set incorrectly to a relative
  // path, e.g. default downloads folder may be set to "X:Downloads".  In those
  // cases, we need to resolve the absolute path as this function is expected to
  // return an absolute path.
  if (result == S_OK) {
    FilePath system_path = FilePath(path);
    if (system_path.IsAbsolute()) {
      result_path = std::move(system_path);
    } else {
      // resolve to absolute path using GetFullPathNameW
      result_path = GetAbsolutePath(system_path);
    }
  }
  if (result_path.IsEmpty()) {
    FilePath tmp_path = Files::GetTemporaryDirectory();
    if (tmp_path.IsAbsolute()) {
      result_path = std::move(tmp_path);
    } else {
      result_path = GetAbsolutePath(tmp_path);
    }
  }
  if (result_path.IsEmpty()) {
    // All attempts failed to get an absolute path.  Just return C:/
    result_path = FilePath("C:\\");
  }
  result_path.append(relative_path);
  CoTaskMemFree(path);
  return result_path;
}
}  // namespace

FilePath GetLocalAppDataPath(const FilePath& app_path) {
  return GetSystemPath(FOLDERID_LocalAppData, app_path);
}

FilePath GetLogPath() {
  return GetLocalAppDataPath(FilePath(kLogsRelativePath));
}

FilePath GetCrashDumpPath() {
  return GetLocalAppDataPath(FilePath(kCrashDumpsRelativePath));
}

FilePath GetDownloadsPath() {
  return GetSystemPath(FOLDERID_Downloads, FilePath(""));
}

}  // namespace nearby::platform::windows
