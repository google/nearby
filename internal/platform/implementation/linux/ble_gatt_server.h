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

#ifndef PLATFORM_IMPL_LINUX_API_BLE_GATT_SERVER_H_
#define PLATFORM_IMPL_LINUX_API_BLE_GATT_SERVER_H_

#include <memory>
#include <regex>

#include <sdbus-c++/IConnection.h>

#include "bluez_gatt_manager.h"
#include "bluez_gatt_profile.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_devices.h"
#include "internal/platform/implementation/linux/bluez_gatt_service_server.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace linux {
class LocalBlePeripheral : public api::ble_v2::BlePeripheral {
 public:
  explicit LocalBlePeripheral(BluetoothAdapter& adapter) : adapter_(adapter) {
    // temp fix till everything transitions to GGetAddress()
    unique_id_ = std::stoull(std::regex_replace(adapter_.GetMacAddress(),
      std::regex("[:\\-]"), ""), nullptr, 16);
  }

  std::string GetAddress() const override { return adapter_.GetMacAddress(); }
  UniqueId GetUniqueId() const override { return unique_id_; }

 private:
  BluetoothAdapter adapter_;
  UniqueId unique_id_;
};

class GattServer : public api::ble_v2::GattServer {
 public:
  GattServer(const GattServer&) = delete;
  GattServer(GattServer&&) = delete;
  GattServer& operator=(const GattServer&) = delete;
  GattServer& operator=(GattServer&&) = delete;

  explicit GattServer(sdbus::IConnection& system_bus, BluetoothAdapter& adapter,
                      std::shared_ptr<BluetoothDevices> devices,
                      api::ble_v2::ServerGattConnectionCallback server_cb)
      : system_bus_(system_bus),
        devices_(std::move(devices)),
        adapter_(adapter),
        local_peripheral_(adapter_),
        gatt_service_root_object_manager(std::make_unique<RootObjectManager>(system_bus_, "/com/google/nearby/medium/ble/gatt")),
        gatt_manager_(std::make_unique<bluez::GattManager>(system_bus_, adapter_.GetObjectPath())),
        server_cb_(std::make_shared<api::ble_v2::ServerGattConnectionCallback>(
            std::move(server_cb))) {}
  ~GattServer() override = default;

  absl::optional<api::ble_v2::GattCharacteristic> CreateCharacteristic(
      const Uuid& service_uuid, const Uuid& characteristic_uuid,
      api::ble_v2::GattCharacteristic::Permission permission,
      api::ble_v2::GattCharacteristic::Property property) override;
  bool UpdateCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic,
      const nearby::ByteArray& value) override;
  absl::Status NotifyCharacteristicChanged(
      const api::ble_v2::GattCharacteristic& characteristic, bool confirm,
      const ByteArray& new_value) override;
  void Stop() override;

 private:
  sdbus::IConnection& system_bus_;
  std::shared_ptr<BluetoothDevices> devices_;
  BluetoothAdapter adapter_;
  LocalBlePeripheral local_peripheral_;

  std::unique_ptr<RootObjectManager> gatt_service_root_object_manager;
  absl::Mutex profiles_mutex_;
  absl::flat_hash_map<Uuid, std::unique_ptr<bluez::GattProfile>> gatt_profiles_;
    ABSL_GUARDED_BY(profiles_mutex_)
  std::unique_ptr<bluez::GattManager> gatt_manager_;
  std::shared_ptr<api::ble_v2::ServerGattConnectionCallback> server_cb_;
  absl::Mutex services_mutex_;
  absl::flat_hash_map<Uuid, std::unique_ptr<bluez::GattServiceServer>> services_
      ABSL_GUARDED_BY(services_mutex_);
};

}  // namespace linux
}  // namespace nearby
#endif
