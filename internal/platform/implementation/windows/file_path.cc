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
#include <string>
#include <string_view>
#include <vector>

#include "absl/types/span.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

namespace {
constexpr wchar_t kDot = L'.';
constexpr wchar_t kPathDelimiter = L'/';
constexpr wchar_t kReplacementChar = L'_';
constexpr wchar_t kBackSlash = L'\\';

constexpr std::wstring_view kForbiddenPathNames[] = {
    L"CON",  L"PRN",  L"AUX",  L"NUL",  L"COM1", L"COM2", L"COM3", L"COM4",
    L"COM5", L"COM6", L"COM7", L"COM8", L"COM9", L"LPT1", L"LPT2", L"LPT3",
    L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9"};

constexpr char kIllegalFileCharacters[] = {':', '*', '?', '\"', '<', '>', '|'};

void ReplaceInvalidCharacters(std::wstring& path) {
  auto it = path.begin();
  if (path.size() >= 2 && path[1] == L':') {
    it += 2;  // Skip the 'C:' or any other drive specifier
  }

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

std::wstring MutateForbiddenPathElements(std::wstring& str) {
  std::vector<std::wstring> path_elements;
  std::wstring::iterator pos = str.begin();
  std::wstring::iterator last = str.begin();

  while (pos != str.end()) {
    last = pos;
    pos = std::find(pos, str.end(), kPathDelimiter);

    if (pos != str.end()) {
      std::wstring path_element = std::wstring(last, pos);
      if (!path_element.empty()) path_elements.push_back(path_element);

      last = ++pos;
    }
  }

  std::wstring lastToken = std::wstring(last, pos);
  if (!lastToken.empty()) path_elements.push_back(lastToken);

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

void SanitizePath(std::wstring& path) {
  path = MutateForbiddenPathElements(path);
  ReplaceInvalidCharacters(path);
}

}  // namespace

nearby::FilePath FilePath::GetCustomSavePath(nearby::FilePath path) {
  std::wstring sanitized_path(path.ToWideString());

  // Replace any \\ with /
  std::replace(sanitized_path.begin(), sanitized_path.end(), kBackSlash,
               kPathDelimiter);

  // Remove any /..'s
  SanitizePath(sanitized_path);

  return Files::CreateUniqueFileName(nearby::FilePath(sanitized_path));
}

}  // namespace nearby::windows
