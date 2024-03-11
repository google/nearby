// Copyright 2022 Google LLC
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

#include "internal/platform/implementation/linux/file_path.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/device_info.h"
#include "internal/platform/implementation/linux/utils.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

const wchar_t* kUpOneLevel = L"/..";
constexpr wchar_t kPathDelimiter = L'/';
constexpr wchar_t kReplacementChar = L'_';
constexpr wchar_t kForwardSlash = L'/';
constexpr wchar_t kBackSlash = L'\\';

std::wstring FilePath::GetCustomSavePath(std::wstring parent_folder,
                                         std::wstring file_name) {
  std::wstring path;
  path += parent_folder + kPathDelimiter + file_name;
  return CreateOutputFileWithRename(path);
}

std::wstring FilePath::GetDownloadPath(std::wstring parent_folder,
                                       std::wstring file_name) {
  return CreateOutputFileWithRename(
      GetDownloadPathInternal(parent_folder, file_name));
}

std::wstring FilePath::GetDownloadPathInternal(std::wstring parent_folder,
                                               std::wstring file_name) {
  DeviceInfo info = DeviceInfo(linux::getSystemBusConnection());

  std::optional<std::filesystem::path> download_path = info.GetDownloadPath();

  std::string base_path;

  std::wstring wide_path(string_to_wstring(base_path));

  if (!download_path) {
    // If grabbing the download path fails then we make a custom one
    base_path = getenv("HOME");
    base_path.append("/Downloads");
  } else {
    base_path = download_path.value();
  }

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
  file.open(wstring_to_string(target), std::fstream::binary | std::fstream::in);

  // While we successfully open the file, keep incrementing the count.
  int count = 0;
  while (!(file.rdstate() & std::ifstream::failbit)) {
    file.close();

    target = (folder + file_name1 + L" (" + std::to_wstring(++count) + L")" +
              file_name2);

    file.clear();
    file.open(wstring_to_string(target),
              std::fstream::binary | std::fstream::in);
  }

  if (count > 0) {
    NEARBY_LOGS(INFO) << "Renamed " << wstring_to_string(path) << " to "
                      << wstring_to_string(target);
  }

  // The above leaves the file open, so close it.
  file.close();

  return target;
}

std::wstring FilePath::MutateForbiddenPathElements(std::wstring& str) {
  // There are no forbidden paths in Linux
  return str;
}

void FilePath::SanitizePath(std::wstring& path) {
  size_t pos = std::wstring::npos;
  // Search for the substring in string in a loop until nothing is found
  while ((pos = path.find(kUpOneLevel)) != std::string::npos) {
    // If found then erase it from string
    path.erase(pos, wcslen(kUpOneLevel));
  }

  ReplaceInvalidCharacters(path);
}

// Legit the only illegal character in Linux
char kIllegalFileCharacters[] = {'/'};

void FilePath::ReplaceInvalidCharacters(std::wstring& path) {
  for (auto& character : path) {
    // If 0 < character < 32, it's illegal, replace it
    if (character > 0 && character < 32) {
      NEARBY_LOGS(INFO) << "In path " << wstring_to_string(path)
                        << " replaced \'" << std::string(1, character)
                        << "\' with \'" << std::string(1, kReplacementChar);
      character = kReplacementChar;
    }
    for (auto illegal_character : kIllegalFileCharacters) {
      if (character == illegal_character) {
        NEARBY_LOGS(INFO) << "In path " << wstring_to_string(path)
                          << " replaced \'" << std::string(1, character)
                          << "\' with \'" << std::string(1, kReplacementChar);
        character = kReplacementChar;
      }
    }
  }
}

}  // namespace linux
}  // namespace nearby
