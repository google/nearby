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

#include "sharing/internal/impl/windows/app_info.h"

#include <windows.h>

#include <optional>
#include <string>

#include "internal/platform/implementation/windows/string_utils.h"

#ifndef QUICK_SHARE_VERSION
#define QUICK_SHARE_VERSION "0.0.0.0"
#endif

namespace nearby::sharing::windows {
namespace {

constexpr wchar_t kAppRegistryKey[] = L"Software\\QuickShareArm64";
constexpr wchar_t kFirstRunDoneValue[] = L"FirstRunDone";

}  // namespace

std::optional<std::string> AppInfo::GetAppVersion() {
  return QUICK_SHARE_VERSION;
}

std::optional<std::string> AppInfo::GetAppLanguage() {
  wchar_t locale[LOCALE_NAME_MAX_LENGTH];
  if (GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH) == 0) {
    return std::nullopt;
  }
  return nearby::windows::string_utils::WideStringToString(locale);
}

bool AppInfo::GetFirstRunDone() {
  DWORD value = 0;
  DWORD size = sizeof(value);
  return RegGetValueW(HKEY_CURRENT_USER, kAppRegistryKey, kFirstRunDoneValue,
                      RRF_RT_REG_DWORD, nullptr, &value,
                      &size) == ERROR_SUCCESS &&
         value != 0;
}

bool AppInfo::SetFirstRunDone(bool value) {
  DWORD data = value ? 1 : 0;
  return RegSetKeyValueW(HKEY_CURRENT_USER, kAppRegistryKey, kFirstRunDoneValue,
                         REG_DWORD, &data, sizeof(data)) == ERROR_SUCCESS;
}

}  // namespace nearby::sharing::windows
