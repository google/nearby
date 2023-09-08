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

#ifndef PLATFORM_IMPL_LINUX_BLUEZ_ADVERTISEMENT_MONITOR_H_
#define PLATFORM_IMPL_LINUX_BLUEZ_ADVERTISEMENT_MONITOR_H_
#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/StandardInterfaces.h>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/bluetooth_devices.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/advertisement_monitor_server.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace linux {
namespace bluez {
class AdvertisementMonitor final
    : public sdbus::AdaptorInterfaces<org::bluez::AdvertisementMonitor1_adaptor,
                                      sdbus::ManagedObject_adaptor> {
 public:
  AdvertisementMonitor(const AdvertisementMonitor&) = delete;
  AdvertisementMonitor(AdvertisementMonitor&&) = delete;
  AdvertisementMonitor& operator=(const AdvertisementMonitor&) = delete;
  AdvertisementMonitor& operator=(AdvertisementMonitor&&) = delete;

  AdvertisementMonitor(sdbus::IConnection& system_bus, Uuid service_uuid,
                       api::ble_v2::TxPowerLevel tx_power_level,
                       absl::string_view type,
                       std::shared_ptr<BluetoothDevices> devices,
                       api::ble_v2::BleMedium::ScanCallback scan_callback);
  AdvertisementMonitor(sdbus::IConnection& system_bus, Uuid service_uuid,
                       api::ble_v2::TxPowerLevel tx_power_level,
                       absl::string_view type,
                       std::shared_ptr<BluetoothDevices> devices,
                       api::ble_v2::BleMedium::ScanningCallback scan_callback);
  ~AdvertisementMonitor() { unregisterAdaptor(); }

 private:
  // Methods
  void Release() override {}
  void Activate() override {
    if (start_scanning_result_callback_ != nullptr) {
      start_scanning_result_callback_(absl::OkStatus());
    }
  }

  void DeviceFound(const sdbus::ObjectPath& device) override;
  void DeviceLost(const sdbus::ObjectPath& device) override;

  // Properties
  std::string Type() override { return type_; };
  int16_t RSSILowThreshold() override { return 0; };
  int16_t RSSIHighThreshold() override {
    return bluez::TxPowerLevelDbm(tx_power_level_);
  }
  uint16_t RSSISamplingPeriod() override {
    // The Windows implementation uses a sampling interval of 2 seconds.
    return 20;
  }
  std::vector<sdbus::Struct<uint8_t, uint8_t, std::vector<uint8_t>>> Patterns()
      override {
    std::array<char, 16> service_id_data = service_uuid_.data();
    return {{0,
             0x16,
             {static_cast<uint8_t>(service_id_data[3] & 0xFF),
              static_cast<uint8_t>(service_id_data[2] & 0xFF)}}};
  };

  std::shared_ptr<BluetoothDevices> devices_;
  api::ble_v2::BleMedium::ScanCallback scan_callback_;
  absl::AnyInvocable<void(absl::Status)> start_scanning_result_callback_;

  std::string type_;
  Uuid service_uuid_;
  api::ble_v2::TxPowerLevel tx_power_level_;
};
}  // namespace bluez
}  // namespace linux
}  // namespace nearby

#endif
