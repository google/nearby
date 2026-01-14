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

#ifndef PLATFORM_IMPL_LINUX_API_BLE_GATT_CLIENT_H_
#define PLATFORM_IMPL_LINUX_API_BLE_GATT_CLIENT_H_

#include <list>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/Types.h>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/bluez_gatt_characteristic_client.h"
#include "internal/platform/implementation/linux/bluez_gatt_service_client.h"

namespace nearby {
namespace linux {

class BluezGattDiscovery final : public bluez::BluezObjectManager {
 public:
  explicit BluezGattDiscovery(std::shared_ptr<sdbus::IConnection> system_bus)
      : bluez::BluezObjectManager(*system_bus),
        system_bus_(system_bus),
        shutdown_(false),
        pending_discovery_(0) {}
  ~BluezGattDiscovery() override { Shutdown(); }

  bool InitializeKnownServices() ABSL_LOCKS_EXCLUDED(mutex_);

  using CallbackIter = typename std::list<absl::AnyInvocable<void()>>::iterator;
  CallbackIter AddPeripheralConnection(
      const sdbus::ObjectPath &device_object_path,
      absl::AnyInvocable<void()> disconnected_callback_)
      ABSL_LOCKS_EXCLUDED(peripheral_disconnected_callbacks_mutex_);
  void RemovePeripheralConnection(const sdbus::ObjectPath &device_object_path,
                                  BluezGattDiscovery::CallbackIter cb)
      ABSL_LOCKS_EXCLUDED(peripheral_disconnected_callbacks_mutex_);

  bool DiscoverServiceAndCharacteristics(
      const sdbus::ObjectPath &device_object_path, const Uuid &service_uuid,
      const std::vector<Uuid> &characteristic_uuids, CancellationFlag &cancel)
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<bluez::GattCharacteristicClient> GetCharacteristic(
      const sdbus::ObjectPath &device_object_path, const Uuid &service_uuid,
      const Uuid &characteristic_uuid) ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<bluez::GattCharacteristicClient> GetSubscribedCharacteristic(
      const sdbus::ObjectPath &device_object_path, const Uuid &service_uuid,
      const Uuid &characteristic_uuid,
      absl::AnyInvocable<void(absl::string_view value)>
          on_characteristic_changed_cb) ABSL_LOCKS_EXCLUDED(mutex_);

 protected:
  void onInterfacesAdded(
      const sdbus::ObjectPath &objectPath,
      const std::map<std::string, std::map<std::string, sdbus::Variant>>
          &interfacesAndProperties) override ABSL_LOCKS_EXCLUDED(mutex_);
  void onInterfacesRemoved(const sdbus::ObjectPath &objectPath,
                           const std::vector<std::string> &interfaces) override
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  std::optional<std::tuple<Uuid, Uuid, sdbus::ObjectPath>>
  characteristicProperties(
      const sdbus::ObjectPath &char_path,
      const std::map<std::string, sdbus::Variant> &properties)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void Shutdown() ABSL_LOCKS_EXCLUDED(mutex_);

  std::shared_ptr<sdbus::IConnection> system_bus_;

  absl::Mutex peripheral_disconnected_callbacks_mutex_;
  absl::flat_hash_map<sdbus::ObjectPath, std::list<absl::AnyInvocable<void()>>>
      peripheral_disconnected_callbacks_
          ABSL_GUARDED_BY(peripheral_disconnected_callbacks_mutex_);

  absl::Mutex mutex_;
  absl::flat_hash_map<sdbus::ObjectPath, std::unique_ptr<GattServiceClient>>
      cached_services_ ABSL_GUARDED_BY(mutex_);
  // Tuple order: service uuid, characteristic uuid, device object path
  absl::flat_hash_map<std::tuple<Uuid, Uuid, sdbus::ObjectPath>,
                      sdbus::ObjectPath>
      discovered_characteristics_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<sdbus::ObjectPath,
                      std::tuple<Uuid, Uuid, sdbus::ObjectPath>>
      characteristics_properties_ ABSL_GUARDED_BY(mutex_);
  bool shutdown_ ABSL_GUARDED_BY(mutex_);
  std::size_t pending_discovery_ ABSL_GUARDED_BY(mutex_);
};

// https://developer.android.com/reference/android/bluetooth/BluetoothGatt
//
// Representation of a client GATT connection to a remote GATT server.
class GattClient : public api::ble_v2::GattClient {
 public:
  GattClient(const GattClient &) = delete;
  GattClient(GattClient &&) = delete;
  GattClient &operator=(const GattClient &) = delete;
  GattClient &operator=(GattClient &&) = delete;

  explicit GattClient(std::shared_ptr<sdbus::IConnection> system_bus,
                      const sdbus::ObjectPath &peripheral_object_path,
                      std::shared_ptr<BluezGattDiscovery> gatt_discovery,
                      absl::AnyInvocable<void()> disconnected_callback)
      : system_bus_(std::move(system_bus)),
        peripheral_object_path_(peripheral_object_path),
        gatt_discovery_(std::move(gatt_discovery)),
        discovery_cancel_(false) {
    disconnected_callback_it_ = gatt_discovery->AddPeripheralConnection(
        peripheral_object_path_, std::move(disconnected_callback));
  }
  ~GattClient() override {
    absl::MutexLock lock(&disconnected_callback_mutex_);
    if (!discovery_cancel_.Cancelled()) {
      discovery_cancel_.Cancel();
      gatt_discovery_->RemovePeripheralConnection(peripheral_object_path_,
                                                  disconnected_callback_it_);
    }
  }
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#discoverServices()
  //
  // Discovers available service and characteristics on this connection.
  // Returns whether or not discovery finished successfully.
  //
  // This function should block until discovery has finished.
  bool DiscoverServiceAndCharacteristics(
      const Uuid &service_uuid,
      const std::vector<Uuid> &characteristic_uuids) override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#getService(java.util.UUID)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGattService.html#getCharacteristic(java.util.UUID)
  //
  // Retrieves a GATT characteristic. On error, does not return a value.
  //
  // DiscoverServiceAndCharacteristics() should be called before this method to
  // fetch all available services and characteristics first.
  //
  // It is okay for duplicate services to exist, as long as the specified
  // characteristic UUID is unique among all services of the same UUID.
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<api::ble_v2::GattCharacteristic> GetCharacteristic(
      const Uuid &service_uuid, const Uuid &characteristic_uuid) override
      ABSL_LOCKS_EXCLUDED(characteristics_mutex_);

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#readCharacteristic(android.bluetooth.BluetoothGattCharacteristic)
  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#getValue()
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<std::string> ReadCharacteristic(
      const api::ble_v2::GattCharacteristic &characteristic) override
      ABSL_LOCKS_EXCLUDED(characteristics_mutex_);

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#setValue(byte[])
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#writeCharacteristic(android.bluetooth.BluetoothGattCharacteristic)
  //
  // Sends a remote characteristic write request to the server and returns
  // whether or not it was successful.
  bool WriteCharacteristic(
      const api::ble_v2::GattCharacteristic &characteristic,
      absl::string_view value, WriteType type) override
      ABSL_LOCKS_EXCLUDED(characteristics_mutex_);

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#setCharacteristicNotification(android.bluetooth.BluetoothGattCharacteristic,%20boolean)
  //
  // Enable or disable notifications/indications for a given characteristic.
  bool SetCharacteristicSubscription(
      const api::ble_v2::GattCharacteristic &characteristic, bool enable,
      absl::AnyInvocable<void(absl::string_view value)>
          on_characteristic_changed_cb) override
      ABSL_LOCKS_EXCLUDED(characteristics_mutex_);

  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#disconnect()
  void Disconnect() override;

 private:
  std::shared_ptr<sdbus::IConnection> system_bus_;
  sdbus::ObjectPath peripheral_object_path_;
  std::shared_ptr<BluezGattDiscovery> gatt_discovery_;

  absl::Mutex disconnected_callback_mutex_;
  BluezGattDiscovery::CallbackIter disconnected_callback_it_
      ABSL_GUARDED_BY(disconnected_callback_mutex_);
  CancellationFlag discovery_cancel_;

  using CharacteristicProxy =
      std::variant<std::unique_ptr<bluez::GattCharacteristicClient>,
                   std::unique_ptr<bluez::SubscribedGattCharacteristicClient>>;
  absl::Mutex characteristics_mutex_;
  absl::flat_hash_map<api::ble_v2::GattCharacteristic, CharacteristicProxy>
      characteristics_ ABSL_GUARDED_BY(characteristics_mutex_);
};

}  // namespace linux
}  // namespace nearby

#endif
