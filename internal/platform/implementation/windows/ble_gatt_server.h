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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_GATT_SERVER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_GATT_SERVER_H_

#include <windows.h>

#include <memory>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/types/optional.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/ble_v2_peripheral.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {

class BleGattServer : public api::ble_v2::GattServer {
 public:
  // Make sure the adapter parameter is not null.
  BleGattServer(api::BluetoothAdapter* adapter,
                api::ble_v2::ServerGattConnectionCallback callback);
  ~BleGattServer() override = default;
  absl::optional<api::ble_v2::GattCharacteristic> CreateCharacteristic(
      const Uuid& service_uuid, const Uuid& characteristic_uuid,
      api::ble_v2::GattCharacteristic::Permission permission,
      api::ble_v2::GattCharacteristic::Property property) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool UpdateCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic,
      const nearby::ByteArray& value) override ABSL_LOCKS_EXCLUDED(mutex_);

  absl::Status NotifyCharacteristicChanged(
      const api::ble_v2::GattCharacteristic& characteristic, bool confirm,
      const ByteArray& new_value) override ABSL_LOCKS_EXCLUDED(mutex_);

  void Stop() override ABSL_LOCKS_EXCLUDED(mutex_);

  bool StartAdvertisement(const ByteArray& service_data, bool is_connectable)
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopAdvertisement() ABSL_LOCKS_EXCLUDED(mutex_);

  void SetCloseNotifier(absl::AnyInvocable<void()> notifier)
      ABSL_LOCKS_EXCLUDED(mutex_);

  api::ble_v2::BlePeripheral& GetBlePeripheral() override {
    return peripheral_;
  }

 private:
  // Used to save native data related to the GATT characteristic.
  struct GattCharacteristicData {
    api::ble_v2::GattCharacteristic gatt_characteristic;
    ByteArray data;
    ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattLocalCharacteristic local_characteristic = nullptr;
    ::winrt::Windows::Foundation::Collections::IVectorView<
        ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
            GattSubscribedClient>
        subscribed_clients;
    ::winrt::event_token read_token{};
    ::winrt::event_token write_token{};
    ::winrt::event_token subscribed_clients_changed_token{};
  };

  bool InitializeGattServer() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void NotifyValueChanged(
      const api::ble_v2::GattCharacteristic& gatt_characteristic)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  GattCharacteristicData* FindGattCharacteristicData(
      const ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattLocalCharacteristic& gatt_local_characteristic)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  GattCharacteristicData* FindGattCharacteristicData(
      const api::ble_v2::GattCharacteristic& gatt_characteristic)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  ::winrt::fire_and_forget Characteristic_ReadRequestedAsync(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattLocalCharacteristic const& gatt_local_characteristic,
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattReadRequestedEventArgs args)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  ::winrt::fire_and_forget Characteristic_WriteRequestedAsync(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattLocalCharacteristic const& gatt_local_characteristic,
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattWriteRequestedEventArgs args)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void Characteristic_SubscribedClientsChanged(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattLocalCharacteristic const& gatt_local_characteristic,
      ::winrt::Windows::Foundation::IInspectable const& args)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void ServiceProvider_AdvertisementStatusChanged(
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattServiceProvider const& sender,
      ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattServiceProviderAdvertisementStatusChangedEventArgs const& args)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  absl::Mutex mutex_;

  BluetoothAdapter* const adapter_ = nullptr;
  BleV2Peripheral peripheral_;
  api::ble_v2::ServerGattConnectionCallback gatt_connection_callback_{};

  ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
      GattServiceProvider gatt_service_provider_ ABSL_GUARDED_BY(mutex_) =
          nullptr;

  absl::AnyInvocable<void()> close_notifier_ ABSL_GUARDED_BY(mutex_) = nullptr;

  Uuid service_uuid_ ABSL_GUARDED_BY(mutex_);
  std::vector<GattCharacteristicData> gatt_characteristic_datas_
      ABSL_GUARDED_BY(mutex_);

  bool is_advertising_ ABSL_GUARDED_BY(mutex_) = false;
  bool is_gatt_server_inited_ ABSL_GUARDED_BY(mutex_) = false;

  ::winrt::event_token service_provider_advertisement_changed_token_
      ABSL_GUARDED_BY(mutex_) = {};
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_GATT_SERVER_H_
