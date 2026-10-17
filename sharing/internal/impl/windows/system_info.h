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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_SYSTEM_INFO_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_SYSTEM_INFO_H_

#include <cstdint>
#include <list>
#include <string>

#include "internal/base/file_path.h"
#include "sharing/internal/api/system_info.h"

namespace nearby::sharing::windows {

// System facts from the registry, SetupAPI device properties and Win32 power
// and memory APIs.
class SystemInfo : public nearby::api::SystemInfo {
 public:
  std::string GetComputerManufacturer() override;
  std::string GetComputerModel() override;
  int64_t GetComputerPhysicalMemory() override;
  int GetComputerProcessorCount() override;
  int GetComputerLogicProcessorCount() override;
  int GetProcessorMemoryInfo() override;
  BatteryChargeStatus QueryBatteryInfo(int& seconds, int& percent,
                                       bool& battery_saver) override;
  std::string GetOsManufacturer() override;
  std::string GetOsName() override;
  std::string GetOsVersion() override;
  std::string GetOsArchitecture() override;
  std::string GetOsLanguage() override;
  std::string GetProcessorManufacturer() override;
  std::string GetProcessorName() override;
  std::list<DriverInfo> GetBluetoothDriverInfos() override;
  std::list<DriverInfo> GetNetworkDriverInfos() override;
  void GetBatteryUsageReport(const FilePath& save_path) override;
};

}  // namespace nearby::sharing::windows

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_SYSTEM_INFO_H_
