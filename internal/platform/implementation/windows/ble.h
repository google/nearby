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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_H_

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/ble_gatt_server.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Devices.Bluetooth.Advertisement.h"

namespace nearby {
namespace windows {

// Container of operations that can be performed over the BLE medium.
class BleMedium : public api::ble::BleMedium {
 public:
  explicit BleMedium(api::BluetoothAdapter& adapter);
  ~BleMedium() override = default;

  // Returns true once the Ble advertising has been initiated.
  bool StartAdvertising(const api::ble::BleAdvertisementData& advertising_data,
                        api::ble::AdvertiseParameters advertising_parameters)
      override ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopAdvertising() override ABSL_LOCKS_EXCLUDED(mutex_);

  std::unique_ptr<AdvertisingSession> StartAdvertising(
      const api::ble::BleAdvertisementData& advertising_data,
      api::ble::AdvertiseParameters advertise_set_parameters,
      AdvertisingCallback callback) override ABSL_LOCKS_EXCLUDED(mutex_);

  bool StartScanning(const Uuid& service_uuid,
                     api::ble::TxPowerLevel tx_power_level,
                     ScanCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopScanning() override ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<ScanningSession> StartScanning(
      const Uuid& service_uuid, api::ble::TxPowerLevel tx_power_level,
      ScanningCallback callback) override;
  std::unique_ptr<api::ble::GattServer> StartGattServer(
      api::ble::ServerGattConnectionCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<api::ble::GattClient> ConnectToGattServer(
      api::ble::BlePeripheral::UniqueId peripheral_id,
      api::ble::TxPowerLevel tx_power_level,
      api::ble::ClientGattConnectionCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<api::ble::BleServerSocket> OpenServerSocket(
      const std::string& service_id) override ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<api::ble::BleSocket> Connect(
      const std::string& service_id, api::ble::TxPowerLevel tx_power_level,
      api::ble::BlePeripheral::UniqueId remote_peripheral_id,
      CancellationFlag* cancellation_flag) override ABSL_LOCKS_EXCLUDED(mutex_);
  bool IsExtendedAdvertisementsAvailable() override;

  void AddAlternateUuidForService(uint16_t uuid,
                                  const std::string& service_id) override;

 private:
  friend class BleMediumTest;

  // A wrapper class for BluetoothLEAdvertisementWatcher.
  class AdvertisementWatcher {
   public:
    AdvertisementWatcher() = default;
    ~AdvertisementWatcher();

    // Initializes an advertisement watcher with no service uuid filter.
    void Initialize(
        const winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Devices::Bluetooth::Advertisement::
                BluetoothLEAdvertisementWatcher,
            winrt::Windows::Devices::Bluetooth::Advertisement::
                BluetoothLEAdvertisementReceivedEventArgs>& received_handler,
        const winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Devices::Bluetooth::Advertisement::
                BluetoothLEAdvertisementWatcher,
            winrt::Windows::Devices::Bluetooth::Advertisement::
                BluetoothLEAdvertisementWatcherStoppedEventArgs>&
            stopped_handler) ABSL_LOCKS_EXCLUDED(mutex_);

    // Initializes an advertisement watcher with a filter for UUID16 service
    // data for `service_uuid`.
    void InitializeWithServiceFilter(
        uint16_t service_uuid,
        const winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Devices::Bluetooth::Advertisement::
                BluetoothLEAdvertisementWatcher,
            winrt::Windows::Devices::Bluetooth::Advertisement::
                BluetoothLEAdvertisementReceivedEventArgs>& received_handler,
        const winrt::Windows::Foundation::TypedEventHandler<
            winrt::Windows::Devices::Bluetooth::Advertisement::
                BluetoothLEAdvertisementWatcher,
            winrt::Windows::Devices::Bluetooth::Advertisement::
                BluetoothLEAdvertisementWatcherStoppedEventArgs>&
            stopped_handler) ABSL_LOCKS_EXCLUDED(mutex_);

    // Start scanning for advertisements.
    // Returns true if the watcher is successfully started.
    bool Start() ABSL_LOCKS_EXCLUDED(mutex_);

    // Stop scanning for advertisements.
    void Stop() ABSL_LOCKS_EXCLUDED(mutex_);

   private:
    absl::Mutex mutex_;
    bool initialized_ ABSL_GUARDED_BY(mutex_) = false;
    winrt::Windows::Devices::Bluetooth::Advertisement::
        BluetoothLEAdvertisementWatcher watcher_ ABSL_GUARDED_BY(mutex_);
    winrt::event_token watcher_token_ ABSL_GUARDED_BY(mutex_);
    winrt::event_token advertisement_received_token_ ABSL_GUARDED_BY(mutex_);
  };

  bool StartBleAdvertising(
      const api::ble::BleAdvertisementData& advertising_data,
      api::ble::AdvertiseParameters advertising_parameters)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool StopBleAdvertising() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool StartGattAdvertising(
      const api::ble::BleAdvertisementData& advertising_data,
      api::ble::AdvertiseParameters advertising_parameters)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool StopGattAdvertising() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

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

  uint64_t GenerateSessionId() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  std::unique_ptr<BleMedium::AdvertisementWatcher> CreateBleWatcher(
      uint16_t service_uuid);

  absl::Mutex mutex_;

  BluetoothAdapter* const adapter_;
  Uuid service_uuid_;
  uint16_t service_uuid16_ = 0;
  api::ble::TxPowerLevel tx_power_level_;
  ScanCallback scan_callback_;
  std::vector<std::unique_ptr<AdvertisementWatcher>> watchers_
      ABSL_GUARDED_BY(mutex_);

  // std::map<Uuid, std::map<uint64_t, ScanningCallback>>
  absl::flat_hash_map<Uuid, absl::flat_hash_map<uint64_t, ScanningCallback>>
      service_uuid_to_session_map_ ABSL_GUARDED_BY(mutex_);

  // WinRT objects
  winrt::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementPublisher publisher_ ABSL_GUARDED_BY(mutex_) =
          nullptr;

  bool is_ble_publisher_started_ ABSL_GUARDED_BY(mutex_) = false;
  bool is_gatt_publisher_started_ ABSL_GUARDED_BY(mutex_) = false;

  winrt::event_token publisher_token_ ABSL_GUARDED_BY(mutex_);

  BleGattServer* ble_gatt_server_ = nullptr;
  // Map of alternative BLE service UUID16s for a given Nearby service.
  // If a device does not support BLE extended advertisements, an alternate
  // service UUID16 may be used to trigger a GATT connection to retrieve GATT
  // characteristics for the Nearby service.
  // This is needed if the addition of the normal service data for the
  // Copresence service uuid causes the advertisement packet to exceed the
  // maximum size.
  absl::flat_hash_map<uint16_t, std::string> alternate_uuids_for_service_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_H_
