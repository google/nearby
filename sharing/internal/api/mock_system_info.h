// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_SYSTEM_INFO_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_SYSTEM_INFO_H_

#include <cstdint>
#include <list>
#include <string>

#include "gmock/gmock.h"
#include "absl/strings/string_view.h"
#include "sharing/internal/api/system_info.h"

namespace nearby::sharing::api {

class MockSystemInfo : public nearby::api::SystemInfo {
 public:
  MockSystemInfo() = default;
  MockSystemInfo(const MockSystemInfo&) = delete;
  MockSystemInfo& operator=(const MockSystemInfo&) = delete;
  ~MockSystemInfo() override = default;

  MOCK_METHOD(std::string, GetComputerManufacturer, (), (override));
  MOCK_METHOD(std::string, GetComputerModel, (), (override));
  MOCK_METHOD(int64_t, GetComputerPhysicalMemory, (), (override));
  MOCK_METHOD(int, GetComputerProcessorCount, (), (override));
  MOCK_METHOD(int, GetComputerLogicProcessorCount, (), (override));
  MOCK_METHOD(int, GetProcessorMemoryInfo, (), (override));

  MOCK_METHOD(BatteryChargeStatus, QueryBatteryInfo,
              (int& seconds, int& percent, bool& battery_saver), (override));

  // Operating system related information.
  MOCK_METHOD(std::string, GetOsManufacturer, (), (override));
  MOCK_METHOD(std::string, GetOsName, (), (override));
  MOCK_METHOD(std::string, GetOsVersion, (), (override));
  MOCK_METHOD(std::string, GetOsArchitecture, (), (override));
  MOCK_METHOD(std::string, GetOsLanguage, (), (override));

  // CPU related information.
  MOCK_METHOD(std::string, GetProcessorManufacturer, (), (override));
  MOCK_METHOD(std::string, GetProcessorName, (), (override));

  // Driver related information.
  MOCK_METHOD(std::list<DriverInfo>, GetBluetoothDriverInfos, (), (override));
  MOCK_METHOD(std::list<DriverInfo>, GetNetworkDriverInfos, (), (override));

  MOCK_METHOD(void, GetBatteryUsageReport, (absl::string_view save_path),
              (override));
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_SYSTEM_INFO_H_
