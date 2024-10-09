// Copyright 2022-2024 Google LLC
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

#include "internal/platform/implementation/windows/file_path.h"

// clang-format off
#include <windows.h>
#include <winver.h>
#include <PathCch.h>
#include <knownfolders.h>
#include <psapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <wchar.h>
// clang-format on

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {

const wchar_t* kUpOneLevel = L"..";
constexpr wchar_t kDot = L'.';
constexpr wchar_t kPathDelimiter = L'/';
constexpr wchar_t kReplacementChar = L'_';
constexpr wchar_t kForwardSlash = L'/';
constexpr wchar_t kBackSlash = L'\\';

constexpr std::wstring_view kForbiddenPathNames[] = {
    L"CON",  L"PRN",  L"AUX",  L"NUL",  L"COM1", L"COM2", L"COM3", L"COM4",
    L"COM5", L"COM6", L"COM7", L"COM8", L"COM9", L"LPT1", L"LPT2", L"LPT3",
    L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9"};

std::wstring FilePath::GetCustomSavePath(std::wstring parent_folder,
                                         std::wstring file_name) {
  std::wstring path;
  SanitizeFileName(file_name);
  path += parent_folder + kPathDelimiter + file_name;
  return CreateOutputFileWithRename(path);
}

std::wstring FilePath::GetDownloadPath(std::wstring parent_folder,
                                       std::wstring file_name) {
  SanitizeFileName(file_name);
  return CreateOutputFileWithRename(
      GetDownloadPathInternal(parent_folder, file_name));
}

std::wstring FilePath::GetDownloadPathInternal(std::wstring parent_folder,
                                               std::wstring file_name) {
  PWSTR basePath;

  // Retrieves the full path of a known folder identified by the folder's
  // KNOWNFOLDERID.
  // https://docs.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
  SHGetKnownFolderPath(
      /*rfid=*/FOLDERID_Downloads,
      /*dwFlags=*/0,
      /*hToken=*/nullptr,
      /*ppszPath=*/&basePath);

  std::wstring wide_path(basePath);
  std::replace(wide_path.begin(), wide_path.end(), kBackSlash, kForwardSlash);

  // If parent_folder starts with a \\ or /, then strip it
  while (!parent_folder.empty() && (*parent_folder.begin() == kBackSlash ||
                                    *parent_folder.begin() == kForwardSlash)) {
    parent_folder.erase(0, 1);
  }

  // If parent_folder ends with a \\ or /, then strip it
  while (!parent_folder.empty() && (*parent_folder.rbegin() == kBackSlash ||
                                    *parent_folder.rbegin() == kForwardSlash)) {
    parent_folder.erase(parent_folder.size() - 1, 1);
  }

  // If file_name starts with a \\, then strip it
  while (!file_name.empty() && (*file_name.begin() == kBackSlash ||
                                *file_name.begin() == kForwardSlash)) {
    file_name.erase(0, 1);
  }

  // If file_name ends with a \\, then strip it
  while (!file_name.empty() && (*file_name.rbegin() == kBackSlash ||
                                *file_name.rbegin() == kForwardSlash)) {
    file_name.erase(file_name.size() - 1, 1);
  }

  CoTaskMemFree(basePath);

  std::wstring path;

  if (parent_folder.empty()) {
    path =
        file_name.empty() ? wide_path : wide_path + kForwardSlash + file_name;
  } else {
    path = file_name.empty() ? wide_path + kForwardSlash + parent_folder
                             : wide_path + kForwardSlash + parent_folder +
                                   kForwardSlash + file_name;
  }

  // Convert to UTF8 format.
  return path;
}

// If the file already exists we add " (x)", where x is an incrementing number,
// starting at 1, using the next non-existing number, to the file name, just
// before the first dot, or at the end if no dot. The absolute path is returned.
std::wstring FilePath::CreateOutputFileWithRename(std::wstring path) {
  std::wstring sanitized_path(path);

  // Replace any \\ with /
  std::replace(sanitized_path.begin(), sanitized_path.end(), kBackSlash,
               kForwardSlash);

  // Remove any /..'s
  SanitizePath(sanitized_path);

  auto last_delimiter = sanitized_path.find_last_of(kPathDelimiter);
  std::wstring folder(sanitized_path.substr(0, last_delimiter));
  std::wstring file_name(sanitized_path.substr(last_delimiter));

  // Locate the last dot
  auto first = file_name.find_last_of('.');

  if (first == std::string::npos) {
    first = file_name.size();
  }

  // Break the string at the dot.
  auto file_name1 = file_name.substr(0, first);
  auto file_name2 = file_name.substr(first);

  // Construct the target file name
  std::wstring target(sanitized_path);

  std::fstream file;

  // Open file as std::wstring
  file.open(target, std::fstream::binary | std::fstream::in);

  // While we successfully open the file, keep incrementing the count.
  int count = 0;
  while (!(file.rdstate() & std::ifstream::failbit)) {
    file.close();

    target = (folder + file_name1 + L" (" + std::to_wstring(++count) + L")" +
              file_name2);

    file.clear();
    file.open(target, std::fstream::binary | std::fstream::in);
  }

  if (count > 0) {
    LOG(INFO) << "Renamed " << string_utils::WideStringToString(path) << " to "
              << string_utils::WideStringToString(target);
  }

  // The above leaves the file open, so close it.
  file.close();

  return target;
}

std::wstring FilePath::MutateForbiddenPathElements(std::wstring& str) {
  std::vector<std::wstring> path_elements;
  std::wstring::iterator pos = str.begin();
  std::wstring::iterator last = str.begin();

  while (pos != str.end()) {
    last = pos;
    pos = std::find(pos, str.end(), kPathDelimiter);

    if (pos != str.end()) {
      std::wstring path_element = std::wstring(last, pos);
      if (path_element.length() > 0) path_elements.push_back(path_element);

      last = ++pos;
    }
  }

  std::wstring lastToken = std::wstring(last, pos);
  if (lastToken.length() > 0) path_elements.push_back(lastToken);

  std::wstring processed_path;
  absl::Span<const std::wstring_view> forbidden(kForbiddenPathNames);

  for (auto& path_element : path_elements) {
    auto tmp_path_element = path_element;

    if (tmp_path_element.size() == 1 && tmp_path_element[0] == kDot) {
      // Change the dot path name to an underscore.
      tmp_path_element[0] = kReplacementChar;
      LOG(INFO) << "Renamed path element "
                << string_utils::WideStringToString(path_element) << " to "
                << string_utils::WideStringToString(tmp_path_element);
      path_element[0] = kReplacementChar;
    }

    std::transform(tmp_path_element.begin(), tmp_path_element.end(),
                   tmp_path_element.begin(),
                   [](wchar_t c) { return std::toupper(c); });

    while (std::find(forbidden.begin(), forbidden.end(), tmp_path_element) !=
           forbidden.end()) {
      tmp_path_element.insert(tmp_path_element.begin(), kReplacementChar);
      LOG(INFO) << "Renamed path element "
                << string_utils::WideStringToString(path_element) << " to "
                << string_utils::WideStringToString(tmp_path_element);
      path_element.insert(path_element.begin(), kReplacementChar);
    }

    processed_path += path_element;
    if (&path_element != &path_elements.back()) {
      processed_path += kPathDelimiter;
    }
  }

  return processed_path;
}

void FilePath::SanitizeFileName(std::wstring& file_name) {
  if (!file_name.empty() && file_name[file_name.size() - 1] == kDot) {
    // Change the last dot to an underscore.
    file_name[file_name.size() - 1] = kReplacementChar;
  }
}

void FilePath::SanitizePath(std::wstring& path) {
  path = MutateForbiddenPathElements(path);
  ReplaceInvalidCharacters(path);
}

char kIllegalFileCharacters[] = {'?', '*', '\'', '<', '>', '|', ':'};

void FilePath::ReplaceInvalidCharacters(std::wstring& path) {
  auto it = path.begin();
  it += 2;  // Skip the 'C:' or any other drive specifier

  for (; it != path.end(); it++) {
    // If 0 < character < 32, it's illegal, replace it
    if (*it > 0 && *it < 32) {
      LOG(INFO) << "In path " << string_utils::WideStringToString(path)
                << " replaced \'" << std::string(1, *it) << "\' with \'"
                << std::string(1, kReplacementChar);
      *it = kReplacementChar;
    }
    if (*it == 0) {  // character is null
      LOG(INFO) << "In path " << string_utils::WideStringToString(path)
                << " replaced \'NULL\' with \'"
                << std::string(1, kReplacementChar) << "\'";
      *it = kReplacementChar;
    }
    for (auto illegal_character : kIllegalFileCharacters) {
      if (*it == illegal_character) {
        LOG(INFO) << "In path " << string_utils::WideStringToString(path)
                  << " replaced \'" << std::string(1, *it) << "\' with \'"
                  << std::string(1, kReplacementChar);
        *it = kReplacementChar;
      }
    }
  }
}

}  // namespace windows
}  // namespace nearby
