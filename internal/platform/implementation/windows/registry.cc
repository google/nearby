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

#include "internal/platform/implementation/windows/registry.h"

#define _WIN32_WINNT _WIN32_WINNT_WIN10

#include <windows.h>  // NOLINT
#include <winreg.h>  // NOLINT

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/windows/registry_accessor.h"
#include "internal/platform/logging.h"

namespace nearby::platform::windows {

namespace {
constexpr int kMaxRegistryStringValueBufferSize = 1024;
constexpr absl::string_view kGoogleUpdateClientsKey =
    R"(Google\Update\Clients\)";
constexpr absl::string_view kGoogleUpdateClientStateKey =
    R"(Google\Update\ClientState\)";
constexpr absl::string_view kGoogleUpdateClientStateMediumKey =
    R"(Google\Update\ClientStateMedium\)";
constexpr absl::string_view kGoogleNearbyShareKey = R"(Google\NearbyShare)";
constexpr absl::string_view kGoogleNearbyShareFlagsKey =
    R"(Google\NearbyShare\Flags)";
constexpr absl::string_view kWindowsKey =
    R"(Microsoft\Windows NT\CurrentVersion)";

constexpr absl::string_view kHiveRootSoftware = "SOFTWARE";
constexpr absl::string_view kHiveRootSoftwareWow6432Node =
    "SOFTWARE\\WOW6432Node";
constexpr absl::string_view kHiveRootSystem = "SYSTEM";

constexpr absl::string_view kQuickShareAppProductId =
    "{232066FE-FF4D-4C25-83B4-3F8747CF7E3A}";

// RegistryAccessor for actual Win API implementation.
class WindowsRegistryAccessor : public RegistryAccessor {
 public:
  WindowsRegistryAccessor() = default;
  ~WindowsRegistryAccessor() override = default;
  WindowsRegistryAccessor(const WindowsRegistryAccessor&) = delete;
  WindowsRegistryAccessor& operator=(const WindowsRegistryAccessor&) = delete;

  LSTATUS ReadDWordValue(HKEY key, const std::string& sub_key,
                         const std::string& value_name,
                         DWORD& value_data) override {
    DWORD value_size = sizeof(DWORD);
    return RegGetValueA(
        key, sub_key.data(), value_name.data(),
        RRF_RT_DWORD | RRF_ZEROONFAILURE | RRF_SUBKEY_WOW6432KEY,
        /*pdwType=*/nullptr, static_cast<void*>(&value_data), &value_size);
  };

  LSTATUS ReadStringValue(HKEY key, const std::string& sub_key,
                          const std::string& value_name,
                          std::string& value_data) override {
    char value_buffer[kMaxRegistryStringValueBufferSize];
    DWORD value_size = kMaxRegistryStringValueBufferSize;
    LSTATUS status = RegGetValueA(
        key, sub_key.data(), value_name.data(),
        RRF_RT_REG_SZ | RRF_ZEROONFAILURE | RRF_SUBKEY_WOW6432KEY,
        /*pdwType=*/nullptr, static_cast<void*>(&value_buffer), &value_size);
    if (status == ERROR_SUCCESS) {
      value_data = value_buffer;
    }
    return status;
  };

  LSTATUS WriteDWordValue(HKEY key, const std::string& sub_key,
                          const std::string& value_name, DWORD value_data,
                          bool create_sub_key) override {
    HKEY h_sub_key;
    LSTATUS status = RegOpenKeyExA(key, sub_key.data(), /*ulOptions=*/0,
                                   KEY_WRITE | KEY_WOW64_32KEY, &h_sub_key);
    if (create_sub_key && status == ERROR_FILE_NOT_FOUND) {
      status = RegCreateKeyExA(key, sub_key.data(), /*reserved*/ 0,
                               /*lpClass=*/nullptr, REG_OPTION_NON_VOLATILE,
                               KEY_WRITE | KEY_WOW64_32KEY,
                               /*lpSecurityAttributes=*/nullptr, &h_sub_key,
                               /*lpdwDisposition=*/nullptr);
    }
    if (status != ERROR_SUCCESS) {
      return status;
    }
    return RegSetKeyValueA(h_sub_key, /*lpSubKey=*/nullptr, value_name.data(),
                           REG_DWORD, static_cast<const void*>(&value_data),
                           sizeof(DWORD));
  }

  LSTATUS WriteStringValue(HKEY key, const std::string& sub_key,
                           const std::string& value_name,
                           const std::string& value_data,
                           bool create_sub_key) override {
    HKEY h_sub_key;
    LSTATUS status = RegOpenKeyExA(key, sub_key.data(), /*ulOptions=*/0,
                                   KEY_WRITE | KEY_WOW64_32KEY, &h_sub_key);
    if (create_sub_key && status == ERROR_FILE_NOT_FOUND) {
      status = RegCreateKeyExA(key, sub_key.data(), /*reserved*/ 0,
                               /*lpClass=*/nullptr, REG_OPTION_NON_VOLATILE,
                               KEY_WRITE | KEY_WOW64_32KEY,
                               /*lpSecurityAttributes=*/nullptr, &h_sub_key,
                               /*lpdwDisposition=*/nullptr);
    }
    if (status != ERROR_SUCCESS) {
      return status;
    }
    return RegSetKeyValueA(h_sub_key, /*lpSubKey=*/nullptr, value_name.data(),
                           REG_SZ, static_cast<const void*>(value_data.data()),
                           value_data.size() + 1);
  }

  LSTATUS EnumValues(
      HKEY key, const std::string& sub_key,
      absl::flat_hash_map<std::string, std::string>& string_values,
      absl::flat_hash_map<std::string, DWORD>& dword_values) override {
    HKEY h_sub_key;
    LSTATUS status =
        RegOpenKeyExA(key, sub_key.data(), /*ulOptions=*/0,
                      KEY_QUERY_VALUE | KEY_WOW64_32KEY, &h_sub_key);
    if (status != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to open key: " << sub_key << ", status: " << status;
      return status;
    }
    DWORD index = 0;
    std::string value_name;
    value_name.reserve(500);
    do {
      DWORD type = 0;
      DWORD name_size = value_name.capacity();
      DWORD value_size = 0;
      status = RegEnumValueA(h_sub_key, index, value_name.data(), &name_size,
                             /*lpReserved=*/nullptr, &type, /*lpData=*/nullptr,
                             &value_size);
      if (status != ERROR_SUCCESS && status != ERROR_MORE_DATA) {
        break;
      }
      if (name_size >= value_name.capacity()) {
        value_name.reserve(name_size + 1);
      }
      name_size = value_name.capacity();
      if (type == REG_SZ) {
        std::string value(value_size, '\0');
        value_size = value.size();
        status =
            RegEnumValueA(h_sub_key, index, value_name.data(), &name_size,
                          /*lpReserved=*/nullptr, &type,
                          reinterpret_cast<LPBYTE>(value.data()), &value_size);
        if (status != ERROR_SUCCESS) {
          break;
        }
        // value_size may or may not include null terminator.  If it does we
        // need to remove it from the string.
        if (value_size > 0 && value[value_size - 1] == '\0') {
          value.resize(value_size - 1);
        }
        string_values.emplace(std::string(value_name.data(), name_size),
                              std::move(value));
      } else if (type == REG_DWORD) {
        DWORD value = 0;
        value_size = sizeof(DWORD);
        status = RegEnumValueA(h_sub_key, index, value_name.data(), &name_size,
                               /*lpReserved=*/nullptr, &type,
                               reinterpret_cast<LPBYTE>(&value), &value_size);
        if (status != ERROR_SUCCESS) {
          break;
        }
        dword_values.emplace(std::string(value_name.data(), name_size), value);
      }
      ++index;
    } while (status == ERROR_SUCCESS);
    RegCloseKey(h_sub_key);
    if (status == ERROR_NO_MORE_ITEMS) {
      return ERROR_SUCCESS;
    }
    return status;
  }
};

std::optional<std::string> GetKey(Registry::Key key,
                                  absl::string_view product_id) {
  switch (key) {
    case Registry::Key::kClients:
      return absl::StrCat(kGoogleUpdateClientsKey, product_id);
    case Registry::Key::kClientState:
      return absl::StrCat(kGoogleUpdateClientStateKey, product_id);
    case Registry::Key::kClientStateMedium:
      return absl::StrCat(kGoogleUpdateClientStateMediumKey, product_id);
    case Registry::Key::kNearbyShare:
      return std::string(kGoogleNearbyShareKey);
    case Registry::Key::kNearbyShareFlags:
      return std::string(kGoogleNearbyShareFlagsKey);
    case Registry::Key::kWindows:
      return std::string(kWindowsKey);
  }

  return std::nullopt;
}

std::optional<HKEY> GetHiveKey(Registry::Hive hive) {
  switch (hive) {
    case Registry::Hive::kCurrentConfig:
      return HKEY_CURRENT_CONFIG;
    case Registry::Hive::kCurrentUser:
      return HKEY_CURRENT_USER;
    case Registry::Hive::kSoftware:
    case Registry::Hive::kSystem:
      return HKEY_LOCAL_MACHINE;
  }

  return std::nullopt;
}

absl::string_view GetHiveRoot(Registry::Hive hive) {
  switch (hive) {
    case Registry::Hive::kCurrentConfig:
    case Registry::Hive::kCurrentUser:
      return kHiveRootSoftware;
    case Registry::Hive::kSoftware:
#ifndef _WIN64
      return kHiveRootSoftware;
#else
      return kHiveRootSoftwareWow6432Node;
#endif
    case Registry::Hive::kSystem:
      return kHiveRootSystem;
    default:
      return "";
  }
}

const char* GetHiveNameForLog(Registry::Hive hive) {
  switch (hive) {
    case Registry::Hive::kCurrentConfig:
      return "HKEY_CURRENT_CONFIG";
    case Registry::Hive::kCurrentUser:
      return "HKEY_CURRENT_USER";
    case Registry::Hive::kSoftware:
    case Registry::Hive::kSystem:
      return "HKEY_LOCAL_MACHINE";
  }

  return "[REQUESTED HIVE OUT OF ALLOW LIST RANGE]";
}

std::optional<std::pair<HKEY, std::string>> LookupRegistryPath(
    Registry::Hive hive, Registry::Key key, absl::string_view product_id) {
  std::optional<HKEY> hKey = GetHiveKey(hive);
  if (!hKey.has_value()) {
    LOG(ERROR) << "Requested registry hive is out of allow list range: "
               << static_cast<int>(hive);
    return std::nullopt;
  }

  std::optional<std::string> keyPath = GetKey(key, product_id);
  if (!keyPath.has_value()) {
    LOG(ERROR) << "Requested registry key is out of allow list range: "
               << static_cast<int>(key);
    return std::nullopt;
  }

  return std::make_pair(hKey.value(),
                        absl::StrCat(GetHiveRoot(hive), "\\", keyPath.value()));
}

// WindowsRegistryAccessor has trivial destructor.
WindowsRegistryAccessor windows_accessor;
RegistryAccessor* test_accessor = nullptr;

RegistryAccessor* getRegistryAccessor() {
  if (test_accessor == nullptr) {
    return &windows_accessor;
  }
  return test_accessor;
}

absl::string_view (*product_id_func)() = nullptr;

absl::string_view GetProductId() {
  if (product_id_func == nullptr) {
    return kQuickShareAppProductId;
  }
  return product_id_func();
}

#define REG_LOG(severity, message, hive, key, val, pretty_name, status)     \
  LOG(severity) << message << " " << GetHiveNameForLog(hive) << "\\" << key \
                << "\\" << val                                              \
                << (pretty_name.has_value()                                 \
                        ? " (" + std::string(pretty_name.value()) + ")"     \
                        : "")                                               \
                << ", status: " << status;
}  // namespace

std::optional<uint32_t> Registry::ReadDword(
    Registry::Hive hive, Registry::Key key, const std::string& value,
    std::optional<absl::string_view> pretty_name) {
  DWORD result = 0;

  std::optional<std::pair<HKEY, std::string>> path =
      LookupRegistryPath(hive, key, GetProductId());
  if (!path.has_value()) {
    return std::nullopt;
  }

  HKEY hKey = path.value().first;
  const std::string& qualified_key = path.value().second;

  LSTATUS status =
      getRegistryAccessor()->ReadDWordValue(hKey, qualified_key, value, result);
  if (status != ERROR_SUCCESS) {
    REG_LOG(WARNING, "Unable to read registry value", hive, qualified_key,
            value, pretty_name, status);
    return std::nullopt;
  }
  REG_LOG(INFO, "Successfully read registry value", hive, qualified_key, value,
          pretty_name, status);
  return static_cast<uint32_t>(result);
}

std::optional<std::string> Registry::ReadString(
    Registry::Hive hive, Registry::Key key, const std::string& value,
    std::optional<absl::string_view> pretty_name) {
  std::optional<std::tuple<HKEY, std::string>> path =
      LookupRegistryPath(hive, key, GetProductId());
  if (!path.has_value()) {
    return std::nullopt;
  }

  HKEY hKey = std::get<0>(path.value());
  const std::string& qualified_key = std::get<1>(path.value());

  std::string result;
  LSTATUS status = getRegistryAccessor()->ReadStringValue(hKey, qualified_key,
                                                          value, result);
  if (status != ERROR_SUCCESS) {
    REG_LOG(WARNING, "Unable to read registry value", hive, qualified_key,
            value, pretty_name, status);
    return std::nullopt;
  }
  REG_LOG(INFO, "Successfully read registry value", hive, qualified_key, value,
          pretty_name, status);
  return result;
}

bool Registry::SetDword(Registry::Hive hive, Registry::Key key,
                        const std::string& value, uint32_t data,
                        bool create_sub_key) {
  std::optional<std::tuple<HKEY, std::string>> path =
      LookupRegistryPath(hive, key, GetProductId());
  if (!path.has_value()) {
    return false;
  }

  HKEY hKey = std::get<0>(path.value());
  const std::string& qualified_key = std::get<1>(path.value());

  LSTATUS status = getRegistryAccessor()->WriteDWordValue(
      hKey, qualified_key, value, data, create_sub_key);
  std::optional<absl::string_view> empty = std::nullopt;
  if (status == ERROR_SUCCESS) {
    REG_LOG(INFO, "Successfully wrote " << data << " to", hive, qualified_key,
            value, empty, status);
    return true;
  }
  REG_LOG(WARNING, "Failed to write " << data << " to", hive, qualified_key,
          value, empty, status);
  return false;
}

bool Registry::SetString(Hive hive, Key key, const std::string& value,
                         const std::string& data, bool create_sub_key) {
  std::optional<std::tuple<HKEY, std::string>> path =
      LookupRegistryPath(hive, key, GetProductId());
  if (!path.has_value()) {
    return false;
  }

  HKEY hKey = std::get<0>(path.value());
  const std::string& qualified_key = std::get<1>(path.value());

  LSTATUS status = getRegistryAccessor()->WriteStringValue(
      hKey, qualified_key, value, data, create_sub_key);
  std::optional<absl::string_view> empty = std::nullopt;
  if (status == ERROR_SUCCESS) {
    REG_LOG(INFO, "Successfully wrote " << data << " to", hive, qualified_key,
            value, empty, status);
    return true;
  }
  REG_LOG(WARNING, "Failed to write " << data << " to", hive, qualified_key,
          value, empty, status);
  return false;
}

bool Registry::EnumValues(
    Hive hive, Key key,
    absl::flat_hash_map<std::string, std::string>& string_values,
    absl::flat_hash_map<std::string, DWORD>& dword_values) {
  std::optional<std::tuple<HKEY, std::string>> path =
      LookupRegistryPath(hive, key, GetProductId());
  if (!path.has_value()) {
    return false;
  }

  HKEY hKey = std::get<0>(path.value());
  const std::string& qualified_key = std::get<1>(path.value());

  LSTATUS status = getRegistryAccessor()->EnumValues(
      hKey, qualified_key, string_values, dword_values);
  if (status != ERROR_SUCCESS) {
    REG_LOG(WARNING, "Failed to enumerate values", hive, qualified_key,
            /*value=*/"", /*pretty_name=*/std::optional<absl::string_view>(),
            status);
    return false;
  }
  return true;
}

void Registry::SetRegistryAccessorForTest(RegistryAccessor* registry_accessor) {
  test_accessor = registry_accessor;
}

void Registry::SetProductIdGetter(absl::string_view (*product_id_getter)()) {
  product_id_func = product_id_getter;
}

}  // namespace nearby::platform::windows
