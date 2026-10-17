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

#include "sharing/internal/impl/windows/system_info.h"

// clang-format off
#include <windows.h>
#include <initguid.h>  // Defines the GUID_DEVCLASS_* values below.
#include <devguid.h>
#include <devpkey.h>
#include <psapi.h>
#include <setupapi.h>
// clang-format on

#include <cstdint>
#include <list>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/logging.h"

namespace nearby::sharing::windows {
namespace {

using ::nearby::windows::string_utils::WideStringToString;

std::string ReadRegistryString(HKEY root, const wchar_t* key,
                               const wchar_t* value) {
  DWORD size = 0;
  if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, nullptr, &size) !=
      ERROR_SUCCESS) {
    return "";
  }
  std::wstring data(size / sizeof(wchar_t), L'\0');
  if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, data.data(),
                   &size) != ERROR_SUCCESS) {
    return "";
  }
  data.resize(wcsnlen(data.c_str(), data.size()));
  return WideStringToString(data);
}

constexpr wchar_t kBiosKey[] = L"HARDWARE\\DESCRIPTION\\System\\BIOS";
constexpr wchar_t kCpuKey[] =
    L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
constexpr wchar_t kOsKey[] =
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

OSVERSIONINFOW GetOsVersionInfo() {
  // GetVersionEx is manifest-dependent; RtlGetVersion reports the real OS.
  OSVERSIONINFOW info{.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW)};
  using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
  auto rtl_get_version = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(
      GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
  if (rtl_get_version != nullptr) rtl_get_version(&info);
  return info;
}

int CountProcessorRelations(LOGICAL_PROCESSOR_RELATIONSHIP relation) {
  DWORD size = 0;
  GetLogicalProcessorInformationEx(relation, nullptr, &size);
  std::vector<uint8_t> buffer(size);
  auto* info =
      reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data());
  if (!GetLogicalProcessorInformationEx(relation, info, &size)) return 0;
  int count = 0;
  for (DWORD offset = 0; offset < size; offset += info->Size) {
    info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
        buffer.data() + offset);
    if (info->Relationship == relation) ++count;
  }
  return count;
}

std::string GetDeviceStringProperty(HDEVINFO devices, SP_DEVINFO_DATA& device,
                                    const DEVPROPKEY& key) {
  DEVPROPTYPE type = 0;
  DWORD size = 0;
  SetupDiGetDevicePropertyW(devices, &device, &key, &type, nullptr, 0, &size,
                            0);
  if (size == 0) return "";
  std::vector<uint8_t> buffer(size);
  if (!SetupDiGetDevicePropertyW(devices, &device, &key, &type, buffer.data(),
                                 size, nullptr, 0)) {
    return "";
  }
  if (type == DEVPROP_TYPE_STRING) {
    return WideStringToString(reinterpret_cast<const wchar_t*>(buffer.data()));
  }
  if (type == DEVPROP_TYPE_FILETIME && size >= sizeof(FILETIME)) {
    SYSTEMTIME time;
    if (FileTimeToSystemTime(reinterpret_cast<const FILETIME*>(buffer.data()),
                             &time)) {
      return absl::StrFormat("%04d-%02d-%02d", time.wYear, time.wMonth,
                             time.wDay);
    }
  }
  return "";
}

std::list<nearby::api::SystemInfo::DriverInfo> GetDriverInfos(
    const GUID& device_class) {
  std::list<nearby::api::SystemInfo::DriverInfo> infos;
  HDEVINFO devices =
      SetupDiGetClassDevsW(&device_class, nullptr, nullptr, DIGCF_PRESENT);
  if (devices == INVALID_HANDLE_VALUE) return infos;
  SP_DEVINFO_DATA device{.cbSize = sizeof(SP_DEVINFO_DATA)};
  for (DWORD index = 0; SetupDiEnumDeviceInfo(devices, index, &device);
       ++index) {
    std::string name =
        GetDeviceStringProperty(devices, device, DEVPKEY_Device_FriendlyName);
    if (name.empty()) {
      name =
          GetDeviceStringProperty(devices, device, DEVPKEY_Device_DeviceDesc);
    }
    infos.push_back({
        .manufacturer = GetDeviceStringProperty(devices, device,
                                                DEVPKEY_Device_Manufacturer),
        .device_name = name,
        .driver_provider_name = GetDeviceStringProperty(
            devices, device, DEVPKEY_Device_DriverProvider),
        .driver_version = GetDeviceStringProperty(
            devices, device, DEVPKEY_Device_DriverVersion),
        .driver_date = GetDeviceStringProperty(devices, device,
                                               DEVPKEY_Device_DriverDate),
    });
  }
  SetupDiDestroyDeviceInfoList(devices);
  return infos;
}

}  // namespace

std::string SystemInfo::GetComputerManufacturer() {
  return ReadRegistryString(HKEY_LOCAL_MACHINE, kBiosKey,
                            L"SystemManufacturer");
}

std::string SystemInfo::GetComputerModel() {
  return ReadRegistryString(HKEY_LOCAL_MACHINE, kBiosKey, L"SystemProductName");
}

int64_t SystemInfo::GetComputerPhysicalMemory() {
  MEMORYSTATUSEX status{.dwLength = sizeof(MEMORYSTATUSEX)};
  return GlobalMemoryStatusEx(&status) ? status.ullTotalPhys : 0;
}

int SystemInfo::GetComputerProcessorCount() {
  return CountProcessorRelations(RelationProcessorPackage);
}

int SystemInfo::GetComputerLogicProcessorCount() {
  return GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
}

int SystemInfo::GetProcessorMemoryInfo() {
  PROCESS_MEMORY_COUNTERS counters{};
  if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters,
                            sizeof(counters))) {
    return 0;
  }
  return static_cast<int>(counters.WorkingSetSize);
}

nearby::api::SystemInfo::BatteryChargeStatus SystemInfo::QueryBatteryInfo(
    int& seconds, int& percent, bool& battery_saver) {
  SYSTEM_POWER_STATUS status;
  if (!GetSystemPowerStatus(&status) || status.BatteryFlag == 255) {
    return BatteryChargeStatus::UNKNOWN;
  }
  if (status.BatteryFlag & 128) return BatteryChargeStatus::NO_BATTERY;
  seconds = status.BatteryLifeTime == static_cast<DWORD>(-1)
                ? 0
                : static_cast<int>(status.BatteryLifeTime);
  percent = status.BatteryLifePercent == 255 ? 0 : status.BatteryLifePercent;
  battery_saver = status.SystemStatusFlag == 1;
  if (status.BatteryFlag & 8) return BatteryChargeStatus::CHARGING;
  if (status.ACLineStatus == 1) return BatteryChargeStatus::CHARGED;
  if (status.ACLineStatus == 0) return BatteryChargeStatus::ON_BATTERY;
  return BatteryChargeStatus::UNKNOWN;
}

std::string SystemInfo::GetOsManufacturer() { return "Microsoft Corporation"; }

std::string SystemInfo::GetOsName() {
  std::string name =
      ReadRegistryString(HKEY_LOCAL_MACHINE, kOsKey, L"ProductName");
  // Windows 11 keeps "Windows 10" in ProductName; the build number decides.
  if (GetOsVersionInfo().dwBuildNumber >= 22000) {
    absl::StrReplaceAll({{"Windows 10", "Windows 11"}}, &name);
  }
  return name;
}

std::string SystemInfo::GetOsVersion() {
  OSVERSIONINFOW info = GetOsVersionInfo();
  return absl::StrCat(info.dwMajorVersion, ".", info.dwMinorVersion, ".",
                      info.dwBuildNumber);
}

std::string SystemInfo::GetOsArchitecture() {
  SYSTEM_INFO info;
  GetNativeSystemInfo(&info);
  switch (info.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_ARM64:
      return "ARM64";
    case PROCESSOR_ARCHITECTURE_AMD64:
      return "x64";
    case PROCESSOR_ARCHITECTURE_INTEL:
      return "x86";
    default:
      return "unknown";
  }
}

std::string SystemInfo::GetOsLanguage() {
  wchar_t locale[LOCALE_NAME_MAX_LENGTH];
  if (LCIDToLocaleName(MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT),
                       locale, LOCALE_NAME_MAX_LENGTH, 0) == 0) {
    return "";
  }
  return WideStringToString(locale);
}

std::string SystemInfo::GetProcessorManufacturer() {
  return ReadRegistryString(HKEY_LOCAL_MACHINE, kCpuKey, L"VendorIdentifier");
}

std::string SystemInfo::GetProcessorName() {
  return ReadRegistryString(HKEY_LOCAL_MACHINE, kCpuKey,
                            L"ProcessorNameString");
}

std::list<nearby::api::SystemInfo::DriverInfo>
SystemInfo::GetBluetoothDriverInfos() {
  return GetDriverInfos(GUID_DEVCLASS_BLUETOOTH);
}

std::list<nearby::api::SystemInfo::DriverInfo>
SystemInfo::GetNetworkDriverInfos() {
  return GetDriverInfos(GUID_DEVCLASS_NET);
}

void SystemInfo::GetBatteryUsageReport(const FilePath& save_path) {
  std::wstring command =
      L"powercfg.exe /batteryreport /output \"" + save_path.ToWideString() +
      L"\"";
  STARTUPINFOW startup{.cb = sizeof(STARTUPINFOW)};
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                      &process)) {
    LOG(WARNING) << "Failed to start powercfg: " << GetLastError();
    return;
  }
  WaitForSingleObject(process.hProcess, 60 * 1000);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
}

}  // namespace nearby::sharing::windows
