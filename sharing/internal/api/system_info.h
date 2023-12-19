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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SYSTEM_INFO_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SYSTEM_INFO_H_

#include <cstdint>
#include <iomanip>
#include <ios>
#include <list>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>

#include "absl/strings/string_view.h"

namespace nearby {
namespace api {

// SystemInfo provides information about the current computer system, such as
// CPU, memory, OS, driver and configuration information. SystemInfo doesn't
// include specific user information.
class SystemInfo {
 public:
  typedef struct _DriverInfo {
    std::string manufacturer;
    std::string device_name;
    std::string driver_provider_name;
    std::string driver_version;
    std::string driver_date;
  } DriverInfo;

  enum class BatteryChargeStatus {
    UNKNOWN = 0,
    NO_BATTERY,
    CHARGING,
    CHARGED,
    ON_BATTERY
  };

  virtual ~SystemInfo() = default;

  // Computer related information.
  virtual std::string GetComputerManufacturer() = 0;
  virtual std::string GetComputerModel() = 0;
  virtual int64_t GetComputerPhysicalMemory() = 0;
  virtual int GetComputerProcessorCount() = 0;
  virtual int GetComputerLogicProcessorCount() = 0;
  virtual int GetProcessorMemoryInfo() = 0;

  virtual BatteryChargeStatus QueryBatteryInfo(int& seconds, int& percent,
                                               bool& battery_saver) = 0;

  // Operating system related information.
  virtual std::string GetOsManufacturer() = 0;
  virtual std::string GetOsName() = 0;
  virtual std::string GetOsVersion() = 0;
  virtual std::string GetOsArchitecture() = 0;
  virtual std::string GetOsLanguage() = 0;

  // CPU related information.
  virtual std::string GetProcessorManufacturer() = 0;
  virtual std::string GetProcessorName() = 0;

  // Driver related information.
  virtual std::list<DriverInfo> GetBluetoothDriverInfos() = 0;
  virtual std::list<DriverInfo> GetNetworkDriverInfos() = 0;

  virtual void GetBatteryUsageReport(absl::string_view save_path) = 0;

  // Outputs all system information to a readable string.
  std::string Dump() {
    std::ostringstream oss;
    oss << "System Information" << std::endl;
    oss << "  Manufacturer: " << GetComputerManufacturer() << std::endl
        << "  Model: " << GetComputerModel() << std::endl
        << "  Physical Memory: " << GetComputerPhysicalMemory() << std::endl
        << "  Processor Count: " << GetComputerProcessorCount() << std::endl
        << "  Logic Processor Count: " << GetComputerLogicProcessorCount()
        << std::endl
        << "  OS Manufacturer: " << GetOsManufacturer() << std::endl
        << "  OS Name: " << GetOsName() << std::endl
        << "  OS Version: " << GetOsVersion() << std::endl
        << "  OS Architecture: " << GetOsArchitecture() << std::endl
        << "  OS Language: " << GetOsLanguage() << std::endl
        << "  CPU Manufacturer: " << GetProcessorManufacturer() << std::endl
        << "  CPU Name: " << GetProcessorName() << std::endl;

    oss << std::endl;
    std::optional<std::list<DriverInfo>> bluetooth_drivers =
        GetBluetoothDriverInfos();
    if (bluetooth_drivers.has_value()) {
      oss << "  Bluetooth Driver Information:" << std::endl;
      for (const DriverInfo& driver_info : *bluetooth_drivers) {
        oss << "    Manufacturer: " << driver_info.manufacturer << std::endl
            << "    Device Name: " << driver_info.device_name << std::endl
            << "    Driver Provider Name: " << driver_info.driver_provider_name
            << std::endl
            << "    Driver Version: " << driver_info.driver_version << std::endl
            << "    Driver Date: " << driver_info.driver_date << std::endl;
      }
    } else {
      oss << "  No Bluetooth Driver Information." << std::endl;
    }

    oss << std::endl;
    std::optional<std::list<DriverInfo>> network_drivers =
        GetNetworkDriverInfos();
    if (network_drivers.has_value()) {
      oss << "  Network Driver Information:" << std::endl;
      for (const DriverInfo& driver_info : *network_drivers) {
        oss << "    Manufacturer: " << driver_info.manufacturer << std::endl
            << "    Device Name: " << driver_info.device_name << std::endl
            << "    Driver Provider Name: " << driver_info.driver_provider_name
            << std::endl
            << "    Driver Version: " << driver_info.driver_version << std::endl
            << "    Driver Date: " << driver_info.driver_date << std::endl;
      }
    } else {
      oss << "  No Network Driver Information." << std::endl;
    }

    oss << std::endl;
    int seconds = 0, percentage = 0;
    bool battery_saver = false;
    BatteryChargeStatus battery_charge_status =
        QueryBatteryInfo(seconds, percentage, battery_saver);
    if (battery_charge_status == BatteryChargeStatus::UNKNOWN ||
        battery_charge_status == BatteryChargeStatus::NO_BATTERY) {
      oss << "  Computer Battery Information: UNKNOWN or NO_BATTERY"
          << std::endl;
    } else {
      oss << "  Computer Battery Information:" << std::endl;
      oss << "    Power State: "
          << BatteryChargeStatusToString(battery_charge_status) << std::endl
          << "    Battery Percentage: " << percentage << std::endl
          << "    Battery Left (Seconds): " << seconds << std::endl
          << "    Battery Saver: " << battery_saver << std::endl;
    }

    oss << std::endl;
    oss << "  Current Process Information:" << std::endl;
    oss << "    Physical Memory currently in use: " << std::fixed
        << std::setprecision(2)
        << GetProcessorMemoryInfo() / static_cast<double>(1024 * 1024) << "MB"
        << std::endl;

    return oss.str();
  }

 private:
  std::string BatteryChargeStatusToString(const BatteryChargeStatus state) {
    switch (state) {
      case BatteryChargeStatus::NO_BATTERY:
        return "NO_BATTERY";
      case BatteryChargeStatus::CHARGING:
        return "CHARGING";
      case BatteryChargeStatus::CHARGED:
        return "CHARGED";
      case BatteryChargeStatus::ON_BATTERY:
        return "ON_BATTERY";
      default:
        return "UNKNOWN";
    }
  }
};

}  // namespace api
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SYSTEM_INFO_H_
