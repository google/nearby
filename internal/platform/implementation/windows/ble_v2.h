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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_V2_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_V2_H_

#include <memory>
#include <string>

#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/bluetooth_classic.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
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
      api::ble_v2::AdvertiseParameters advertising_parameters) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopAdvertising() override ABSL_LOCKS_EXCLUDED(mutex_);

  std::unique_ptr<AdvertisingSession> StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertise_set_parameters,
      AdvertisingCallback callback) override ABSL_LOCKS_EXCLUDED(mutex_);

  bool StartScanning(const Uuid& service_uuid,
                     api::ble_v2::TxPowerLevel tx_power_level,
                     ScanCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopScanning() override ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<ScanningSession> StartScanning(
      const Uuid& service_uuid, api::ble_v2::TxPowerLevel tx_power_level,
      ScanningCallback callback) override ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<api::ble_v2::GattServer> StartGattServer(
      api::ble_v2::ServerGattConnectionCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<api::ble_v2::GattClient> ConnectToGattServer(
      api::ble_v2::BlePeripheral& peripheral,
      api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::ClientGattConnectionCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<api::ble_v2::BleServerSocket> OpenServerSocket(
      const std::string& service_id) override ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<api::ble_v2::BleSocket> Connect(
      const std::string& service_id, api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::BlePeripheral& remote_peripheral,
      CancellationFlag* cancellation_flag) override ABSL_LOCKS_EXCLUDED(mutex_);
  bool IsExtendedAdvertisementsAvailable() override { return false; }

  BluetoothAdapter& GetAdapter() { return *adapter_; }

 private:
  absl::Mutex mutex_;
  BluetoothAdapter* adapter_;
  ByteArray advertisement_byte_ ABSL_GUARDED_BY(mutex_);

  bool advertising_started_ = false;
  bool advertising_error_ = false;
  bool advertising_stopped_ = false;

  bool scanning_started_ = false;
  bool scanning_error_ = false;
  bool scanning_stopped_ = false;

  // WinRT objects
  winrt::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementPublisher publisher_;
  winrt::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementWatcher watcher_;
  winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisement
      advertisement_;

  winrt::event_token publisher_token_;
  void PublisherHandler(
      winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisher publisher,
      winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisherStatusChangedEventArgs args);
  absl::AnyInvocable<void()> publisher_started_callback_
      ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> publisher_stopped_callback_
      ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> publisher_error_callback_ ABSL_GUARDED_BY(mutex_);

  winrt::event_token watcher_token_;
  void WatcherHandler(winrt::Windows::Devices::Bluetooth::Advertisement::
                          BluetoothLEAdvertisementWatcher watcher,
                      winrt::Windows::Devices::Bluetooth::Advertisement::
                          BluetoothLEAdvertisementWatcherStoppedEventArgs args);
  absl::AnyInvocable<void()> watcher_started_callback_ ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> watcher_stopped_callback_ ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> watcher_error_callback_ ABSL_GUARDED_BY(mutex_);

  winrt::event_token advertisement_received_token_;
  void AdvertisementReceivedHandler(
      winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementWatcher watcher,
      winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementReceivedEventArgs args);
  ScanCallback scan_response_received_callback_;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_V2_H_
