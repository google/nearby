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

#ifndef PLATFORM_IMPL_LINUX_API_BLE_V2_MEDIUM_H_
#define PLATFORM_IMPL_LINUX_API_BLE_V2_MEDIUM_H_

#include <memory>
#include <optional>
#include <vector>

#include <sdbus-c++/IConnection.h>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/ble_v2.h"
// #include "internal/platform/implementation/linux/ble_gatt_client.h"
#include "bluez_gatt_manager.h"
#include "internal/platform/implementation/linux/ble_v2_server_socket.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_devices.h"
#include "internal/platform/implementation/linux/bluez_advertisement_monitor.h"
#include "internal/platform/implementation/linux/bluez_advertisement_monitor_manager.h"
#include "internal/platform/implementation/linux/bluez_le_advertisement.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace linux {
class BleV2Medium final : public api::ble_v2::BleMedium {
 public:
  BleV2Medium(const BleV2Medium &) = delete;
  BleV2Medium(BleV2Medium &&) = delete;
  BleV2Medium &operator=(const BleV2Medium &) = delete;
  BleV2Medium &operator=(BleV2Medium &&) = delete;

  explicit BleV2Medium(BluetoothAdapter &adapter);
  ~BleV2Medium() override = default;

  bool StartAdvertising(
      const api::ble_v2::BleAdvertisementData &advertising_data,
      api::ble_v2::AdvertiseParameters advertise_set_parameters) override
      ABSL_LOCKS_EXCLUDED(cur_adv_mutex_);
  std::unique_ptr<AdvertisingSession> StartAdvertising(
      const api::ble_v2::BleAdvertisementData &advertising_data,
      api::ble_v2::AdvertiseParameters advertise_set_parameters,
      AdvertisingCallback callback) ABSL_LOCKS_EXCLUDED(advs_mutex_) override;
  bool StopAdvertising() override ABSL_LOCKS_EXCLUDED(advs_mutex_);

  bool StartScanning(const Uuid &service_uuid,
                     api::ble_v2::TxPowerLevel tx_power_level,
                     ScanCallback callback) override
      ABSL_LOCKS_EXCLUDED(active_adv_monitors_mutex_);
  bool StopScanning() override ABSL_LOCKS_EXCLUDED(active_adv_monitors_mutex_);

  std::unique_ptr<ScanningSession> StartScanning(
      const Uuid &service_uuid, api::ble_v2::TxPowerLevel tx_power_level,
      ScanningCallback callback) override;

  std::unique_ptr<api::ble_v2::GattServer> StartGattServer(
      api::ble_v2::ServerGattConnectionCallback callback) override;

  std::unique_ptr<api::ble_v2::GattClient> ConnectToGattServer(
      api::ble_v2::BlePeripheral::UniqueId peripheral_id,
      api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::ClientGattConnectionCallback callback) override;

  std::unique_ptr<api::ble_v2::BleServerSocket> OpenServerSocket(
      const std::string &service_id) override;

  std::unique_ptr<api::ble_v2::BleL2capServerSocket> OpenL2capServerSocket(
      const std::string &service_id) override;

  std::unique_ptr<api::ble_v2::BleSocket> Connect(
      const std::string &service_id, api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::BlePeripheral::UniqueId peripheral_id,
      CancellationFlag *cancellation_flag) override;

  std::unique_ptr<api::ble_v2::BleL2capSocket> ConnectOverL2cap(
      int psm, const std::string &service_id,
      api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::BlePeripheral::UniqueId peripheral_id,
      CancellationFlag *cancellation_flag) override;

  bool StartMultipleServicesScanning(const std::vector<Uuid> &service_uuids,
                                     api::ble_v2::TxPowerLevel tx_power_level,
                                     ScanCallback callback) override;

  bool PauseMediumScanning() override;

  bool ResumeMediumScanning() override;

  bool IsExtendedAdvertisementsAvailable() override;

  void AddAlternateUuidForService(uint16_t uuid,
                                  const std::string &service_id) override;

  std::optional<api::ble_v2::BlePeripheral::UniqueId>
  RetrieveBlePeripheralIdFromNativeId(
      const std::string &ble_peripheral_native_id) override;

  // bool GetRemotePeripheral(const std::string &mac_address,
  //                          GetRemotePeripheralCallback callback) override;
  // bool GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId id,
  //                          GetRemotePeripheralCallback callback) override;

 private:
  bool StartLEDiscovery();

  bool MonitorManagerSupportsOr() {
    std::vector<std::string> supported_types;
    try {
      supported_types = adv_monitor_manager_->SupportedMonitorTypes();
    } catch (const sdbus::Error &e) {
      DBUS_LOG_PROPERTY_GET_ERROR(adv_monitor_manager_, "SupportedMonitorTypes",
                                  e);
      return false;
    }

    auto is_supported_type = [](std::string pattern) {
      return pattern == "or_patterns";
    };

    auto end = supported_types.cend();
    return std::find_if(supported_types.cbegin(), end, is_supported_type) !=
           end;
  }

  std::shared_ptr<sdbus::IConnection> system_bus_;
  BluetoothAdapter adapter_;
  ObserverList<api::BluetoothClassicMedium::Observer> observers_ = {};
  std::shared_ptr<BluetoothDevices> devices_;
  // std::shared_ptr<BluezGattDiscovery> gatt_discovery_;

  std::unique_ptr<RootObjectManager> root_object_manager_;
  std::unique_ptr<bluez::AdvertisementMonitorManager> adv_monitor_manager_;
  absl::Mutex active_adv_monitors_mutex_;
  absl::flat_hash_map<
      Uuid,
      std::pair<std::unique_ptr<bluez::AdvertisementMonitor>, std::unique_ptr<DeviceWatcher>>>
      active_adv_monitors_ ABSL_GUARDED_BY(active_adv_monitors_mutex_);
  // Used by the synchronous variant of StartScanning
  std::optional<Uuid> cur_monitored_service_uuid_;

  std::unique_ptr<bluez::LEAdvertisementManager> adv_manager_;

  absl::Mutex cur_adv_mutex_;
  std::unique_ptr<bluez::LEAdvertisement> cur_adv_
      ABSL_GUARDED_BY(cur_adv_mutex_);

  absl::Mutex advs_mutex_;
  std::list<std::unique_ptr<bluez::LEAdvertisement>> advs_
      ABSL_GUARDED_BY(advs_mutex_);
};
}  // namespace linux
}  // namespace nearby

#endif
