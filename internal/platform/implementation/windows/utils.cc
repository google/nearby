// Copyright 2020-2024 Google LLC
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

#include "internal/platform/implementation/windows/utils.h"

// clang-format off
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
// clang-format on

// Standard C/C++ headers
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Nearby connections headers
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Networking.Connectivity.h"
#include "winrt/base.h"

namespace nearby::windows {

using winrt::Windows::Foundation::IInspectable;

Uuid winrt_guid_to_nearby_uuid(const ::winrt::guid& guid) {
  int64_t data1 = guid.Data1;
  int64_t data2 = guid.Data2;
  int64_t data3 = guid.Data3;

  int64_t msb = ((data1 >> 24) & 0xff) << 56 | ((data1 >> 16) & 0xff) << 48 |
                ((data1 >> 8) & 0xff) << 40 | ((data1) & 0xff) << 32 |
                ((data2 >> 8) & 0xff) << 24 | ((data2) & 0xff) << 16 |
                ((data3 >> 8) & 0xff) << 8 | (data3 & 0xff);

  int64_t lsb =
      ((int64_t)guid.Data4[0]) << 56 | ((int64_t)guid.Data4[1]) << 48 |
      ((int64_t)guid.Data4[2]) << 40 | ((int64_t)guid.Data4[3]) << 32 |
      ((int64_t)guid.Data4[4]) << 24 | ((int64_t)guid.Data4[5]) << 16 |
      ((int64_t)guid.Data4[6]) << 8 | (int64_t)guid.Data4[7];

  return Uuid(msb, lsb);
}

winrt::guid nearby_uuid_to_winrt_guid(Uuid uuid) {
  winrt::guid guid;
  uint64_t msb = uuid.GetMostSigBits();
  guid.Data1 = ((msb >> 56) & 0xff) << 24 | ((msb >> 48) & 0xff) << 16 |
               ((msb >> 40) & 0xff) << 8 | ((msb >> 32) & 0xff);
  guid.Data2 = ((msb >> 24) & 0xff) << 8 | ((msb >> 16) & 0xff);
  guid.Data3 = ((msb >> 8) & 0xff) << 8 | (msb & 0xff);
  uint64_t lsb = uuid.GetLeastSigBits();
  guid.Data4[0] = (lsb >> 56) & 0xff;
  guid.Data4[1] = (lsb >> 48) & 0xff;
  guid.Data4[2] = (lsb >> 40) & 0xff;
  guid.Data4[3] = (lsb >> 32) & 0xff;
  guid.Data4[4] = (lsb >> 24) & 0xff;
  guid.Data4[5] = (lsb >> 16) & 0xff;
  guid.Data4[6] = (lsb >> 8) & 0xff;
  guid.Data4[7] = lsb & 0xff;
  return guid;
}

bool is_nearby_uuid_equal_to_winrt_guid(const Uuid& uuid,
                                        const ::winrt::guid& guid) {
  return uuid == winrt_guid_to_nearby_uuid(guid);
}

bool InspectableReader::ReadBoolean(IInspectable inspectable) {
  if (inspectable == nullptr) {
    return false;
  }

  auto property_value =
      inspectable.try_as<winrt::Windows::Foundation::IPropertyValue>();
  if (property_value == nullptr) {
    throw std::invalid_argument("no property value interface.");
  }
  if (property_value.Type() !=
      winrt::Windows::Foundation::PropertyType::Boolean) {
    throw std::invalid_argument("not uin16 data type.");
  }

  return property_value.GetBoolean();
}

uint16_t InspectableReader::ReadUint16(IInspectable inspectable) {
  if (inspectable == nullptr) {
    return 0;
  }

  auto property_value =
      inspectable.try_as<winrt::Windows::Foundation::IPropertyValue>();
  if (property_value == nullptr) {
    throw std::invalid_argument("no property value interface.");
  }
  if (property_value.Type() !=
      winrt::Windows::Foundation::PropertyType::UInt16) {
    throw std::invalid_argument("not uin16 data type.");
  }

  return property_value.GetUInt16();
}

uint32_t InspectableReader::ReadUint32(IInspectable inspectable) {
  if (inspectable == nullptr) {
    return 0;
  }

  auto property_value =
      inspectable.try_as<winrt::Windows::Foundation::IPropertyValue>();
  if (property_value == nullptr) {
    throw std::invalid_argument("no property value interface.");
  }
  if (property_value.Type() !=
      winrt::Windows::Foundation::PropertyType::UInt32) {
    throw std::invalid_argument("not uin32 data type.");
  }

  return property_value.GetUInt32();
}

std::string InspectableReader::ReadString(IInspectable inspectable) {
  if (inspectable == nullptr) {
    return "";
  }

  auto property_value =
      inspectable.try_as<winrt::Windows::Foundation::IPropertyValue>();
  if (property_value == nullptr) {
    throw std::invalid_argument("no property value interface.");
  }
  if (property_value.Type() !=
      winrt::Windows::Foundation::PropertyType::String) {
    throw std::invalid_argument("not string data type.");
  }

  return nearby::windows::string_utils::WideStringToString(
      property_value.GetString().c_str());
}

std::vector<std::string> InspectableReader::ReadStringArray(
    IInspectable inspectable) {
  std::vector<std::string> result;
  if (inspectable == nullptr) {
    return result;
  }

  auto property_value =
      inspectable.try_as<winrt::Windows::Foundation::IPropertyValue>();
  if (property_value == nullptr) {
    throw std::invalid_argument("no property value interface.");
  }
  if (property_value.Type() !=
      winrt::Windows::Foundation::PropertyType::StringArray) {
    throw std::invalid_argument("not string array data type.");
  }

  winrt::com_array<winrt::hstring> strings;
  property_value.GetStringArray(strings);

  for (const winrt::hstring& str : strings) {
    result.push_back(winrt::to_string(str));
  }
  return result;
}

GUID InspectableReader::ReadGuid(IInspectable inspectable) {
  if (inspectable == nullptr) {
    return GUID{};
  }
  auto property_value =
      inspectable.try_as<winrt::Windows::Foundation::IPropertyValue>();
  if (property_value == nullptr) {
    throw std::invalid_argument("no property value interface.");
  }
  if (property_value.Type() !=
      winrt::Windows::Foundation::PropertyType::Guid) {
    throw std::invalid_argument("not guid data type.");
  }

  return property_value.GetGuid();
}

std::optional<std::wstring> GetDnsHostName() {
  DWORD size = 0;

  // Get length of the computer name.
  if (GetComputerNameExW(ComputerNameDnsHostname, nullptr, &size) == 0) {
    if (GetLastError() != ERROR_MORE_DATA) {
      LOG(ERROR) << ": Failed to get device dns name size, error:"
                 << GetLastError();
      return std::nullopt;
    }
  }
  std::wstring device_name(size, L' ');
  if (GetComputerNameExW(ComputerNameDnsHostname, device_name.data(), &size) !=
      0) {
    // On input size includes null termination.
    // On output size excludes null termination.
    device_name.resize(size);
    return device_name;
  }

  LOG(ERROR) << ": Failed to get device dns name, error:" << GetLastError();
  return std::nullopt;
}

bool IsIntelWifiAdapter() {
  bool found_intel = false;
  LOG(INFO) << "Starting scan for Intel Wi-Fi adapters...";

  // 1. Get a handle to all network adapters present in the system
  HDEVINFO h_dev_info =
      SetupDiGetClassDevsW(&GUID_DEVCLASS_NET, nullptr, nullptr, DIGCF_PRESENT);
  // SetupDiDestroyDeviceInfoList needs to be called in the end of this function
  if (h_dev_info == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Failed to get Class Devs handle. Error: " << GetLastError();
    return false;
  }

  SP_DEVINFO_DATA dev_info_data;
  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  // 2. Enumerate through the devices
  for (DWORD i = 0; SetupDiEnumDeviceInfo(h_dev_info, i, &dev_info_data); i++) {
    // 3. Get the Hardware ID property (contains VEN_XXXX and DEV_XXXX)
    DWORD hardware_id_size = 0;
    if (!SetupDiGetDeviceRegistryPropertyW(h_dev_info, &dev_info_data,
                                           SPDRP_HARDWAREID, nullptr, nullptr,
                                           0, &hardware_id_size) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      LOG(ERROR) << "Failed to get Hardware ID size. Error: " << GetLastError();
      continue;
    }
    std::wstring hardware_id;
    hardware_id.resize((hardware_id_size + sizeof(wchar_t) - 1) /
                       sizeof(wchar_t));
    if (SetupDiGetDeviceRegistryPropertyW(
            h_dev_info, &dev_info_data, SPDRP_HARDWAREID, nullptr,
            reinterpret_cast<BYTE*>(hardware_id.data()),
            hardware_id.size() * sizeof(wchar_t), nullptr)) {
      // REG_MULTI_SZ contains multiple null-terminated strings.
      // We search the whole buffer for the Intel Vendor ID (8086).
      // 4. Check for Intel Vendor ID: 8086
      // On you Windows computer, go to "Device Manager" --> "Network adapters"
      // --> Right click Wifi device
      // --> click "properties" --> click "Details" --> In "Property" dropdown
      // list, choose "Hardware ID", then you will see:
      // Hardware IDs look like: PCI\VEN_8086&DEV_2723...,
      // https://learn.microsoft.com/en-us/windows-hardware/drivers/install/identifiers-for-pci-devices
      // https://learn.microsoft.com/en-us/windows-hardware/drivers/install/inf-models-section
      if (hardware_id.find(L"VEN_8086") !=  // NOLINT - ClangTidy asks to replace
                                      // find() with absl::StrContains(), but
                                      // this is not applicable to wide string
                                      // and will cause compile error.
          std::wstring::npos) {
        // 5. Ensure it's a Wi-Fi/Wireless device
        // We check the description to make sure we aren't flagging an Intel
        // Ethernet chip
        std::wstring to_log_hw_id = hardware_id;
        // Replace all null characters with spaces to avoid truncation.
        std::replace(to_log_hw_id.begin(), to_log_hw_id.end(), L'\0', L' ');
        LOG(INFO) << "  - Hardware ID: "
                  << string_utils::WideStringToString(to_log_hw_id);

        DWORD desc_buf_size = 0;
        if (!SetupDiGetDeviceRegistryPropertyW(h_dev_info, &dev_info_data,
                                               SPDRP_DEVICEDESC, nullptr,
                                               nullptr, 0, &desc_buf_size) &&
            GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
          continue;
        }

        std::wstring name;
        name.resize((desc_buf_size + sizeof(wchar_t) - 1) / sizeof(wchar_t));
        if (SetupDiGetDeviceRegistryPropertyW(
                h_dev_info, &dev_info_data, SPDRP_DEVICEDESC, nullptr,
                reinterpret_cast<BYTE*>(name.data()),
                name.size() * sizeof(wchar_t), nullptr)) {
          if (name.find(L"Wi-Fi") !=  // NOLINT - ClangTidy asks to replace
                                      // find() with absl::StrContains(), but
                                      // this is not applicable to wide string
                                      // and will cause compile error.
                  std::wstring::npos ||
              name.find(L"Wireless") !=  // NOLINT
                  std::wstring::npos ||
              name.find(L"Killer") !=  // NOLINT
                  std::wstring::npos) {
            LOG(INFO) << "Found Intel Wi-Fi: " << name;
            found_intel = true;
            break;
          }
        }
      }
    }
  }

  SetupDiDestroyDeviceInfoList(h_dev_info);
  return found_intel;
}

}  // namespace nearby::windows
