// Copyright 2022-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_V2_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_V2_H_

#include <memory>
#include <string>

#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/windows/ble_gatt_server.h"
#include "internal/platform/implementation/windows/ble_v2_peripheral.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/bluetooth_classic.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Devices.Bluetooth.Advertisement.h"

namespace nearby {
namespace windows {

// Container of operations that can be performed over the BLE medium.
class BleV2Medium : public api::ble_v2::BleMedium {
 public:
  explicit BleV2Medium(api::BluetoothAdapter& adapter);
  ~BleV2Medium() override = default;

  // Returns true once the Ble advertising has been initiated.
  bool StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertising_parameters) override;
  bool StopAdvertising() override;

  std::unique_ptr<AdvertisingSession> StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertise_set_parameters,
      AdvertisingCallback callback) override;

  bool StartScanning(const Uuid& service_uuid,
                     api::ble_v2::TxPowerLevel tx_power_level,
                     ScanCallback callback) override;
  bool StopScanning() override;
  std::unique_ptr<ScanningSession> StartScanning(
      const Uuid& service_uuid, api::ble_v2::TxPowerLevel tx_power_level,
      ScanningCallback callback) override;
  std::unique_ptr<api::ble_v2::GattServer> StartGattServer(
      api::ble_v2::ServerGattConnectionCallback callback) override;
  std::unique_ptr<api::ble_v2::GattClient> ConnectToGattServer(
      api::ble_v2::BlePeripheral& peripheral,
      api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::ClientGattConnectionCallback callback) override;
  std::unique_ptr<api::ble_v2::BleServerSocket> OpenServerSocket(
      const std::string& service_id) override;
  std::unique_ptr<api::ble_v2::BleSocket> Connect(
      const std::string& service_id, api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::BlePeripheral& remote_peripheral,
      CancellationFlag* cancellation_flag) override;
  bool IsExtendedAdvertisementsAvailable() override;

  bool GetRemotePeripheral(const std::string& mac_address,
                           GetRemotePeripheralCallback callback) override;

  bool GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId id,
                           GetRemotePeripheralCallback callback) override;

 private:
  bool StartBleAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertising_parameters);
  bool StopBleAdvertising();

  bool StartGattAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertising_parameters);
  bool StopGattAdvertising();

  void PublisherHandler(
      winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisher publisher,
      winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisherStatusChangedEventArgs args);

  void AdvertisementReceivedHandler(
      winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementWatcher watcher,
      winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementReceivedEventArgs args);

  void AdvertisementFoundHandler(
      winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementWatcher watcher,
      winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementReceivedEventArgs args);

  void WatcherHandler(winrt::Windows::Devices::Bluetooth::Advertisement::
                          BluetoothLEAdvertisementWatcher watcher,
                      winrt::Windows::Devices::Bluetooth::Advertisement::
                          BluetoothLEAdvertisementWatcherStoppedEventArgs args);

  uint64_t GenerateSessionId();
  // Returns nullptr if `address` is invalid.
  BleV2Peripheral* GetOrCreatePeripheral(absl::string_view address);
  // Returns nullptr if `id` does not match a known peripheral.
  BleV2Peripheral* GetPeripheral(BleV2Peripheral::UniqueId id);

  void RemoveExpiredPeripherals()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(peripheral_map_mutex_);

  BluetoothAdapter* adapter_;
  Uuid service_uuid_;
  api::ble_v2::TxPowerLevel tx_power_level_;
  ScanCallback scan_callback_;

  absl::Mutex map_mutex_;
  // std::map<Uuid, std::map<uint64_t, ScanningCallback>>
  absl::flat_hash_map<Uuid, absl::flat_hash_map<uint64_t, ScanningCallback>>
      service_uuid_to_session_map_ ABSL_GUARDED_BY(map_mutex_);

  // WinRT objects
  ::winrt::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementPublisher publisher_ = nullptr;
  ::winrt::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementWatcher watcher_ = nullptr;

  bool is_ble_publisher_started_ = false;
  bool is_gatt_publisher_started_ = false;
  bool is_watcher_started_ = false;

  ::winrt::event_token publisher_token_;
  ::winrt::event_token watcher_token_;
  ::winrt::event_token advertisement_received_token_;

  BleGattServer* ble_gatt_server_ = nullptr;
  // Map to protect the pointer for BlePeripheral because
  // DiscoveredPeripheralCallback only keeps the pointer to the object
  absl::Mutex peripheral_map_mutex_;
  struct PeripheralInfo {
    absl::Time last_access_time;
    std::unique_ptr<BleV2Peripheral> peripheral;
  };
  absl::flat_hash_map<BleV2Peripheral::UniqueId, PeripheralInfo> peripheral_map_
      ABSL_GUARDED_BY(peripheral_map_mutex_);
  absl::Time cleanup_time_ ABSL_GUARDED_BY(peripheral_map_mutex_) = absl::Now();
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_V2_H_
