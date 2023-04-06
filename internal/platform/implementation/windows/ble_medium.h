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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_MEDIUM_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_MEDIUM_H_

#include <guiddef.h>

#include <future>  //  NOLINT
#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/ble.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "winrt/Windows.Devices.Bluetooth.Advertisement.h"

namespace nearby {
namespace windows {

// Only Fast Advertisement is supported where the remote device's static
// bluetooth MAC address is resolved from Nearby Share Contact certificates
// using salt and encrypted_metadata_key from the BLE advertisement packet.
// The MAC address is then used directly to establish a Bluetooth Classic
// RFCOMM socket.

// Container of operations that can be performed over the BLE medium.
class BleMedium : public api::BleMedium {
 public:
  explicit BleMedium(api::BluetoothAdapter& adapter);
  ~BleMedium() override = default;

  bool StartAdvertising(
      const std::string& service_id, const ByteArray& advertisement_bytes,
      const std::string& fast_advertisement_service_uuid) override;

  bool StopAdvertising(const std::string& service_id) override;

  // Returns true once the BLE scan has been initiated.
  bool StartScanning(const std::string& service_id,
                     const std::string& fast_advertisement_service_uuid,
                     DiscoveredPeripheralCallback callback) override;

  // Returns true once BLE scanning for service_id is well and truly stopped;
  // after this returns, there must be no more invocations of the
  // DiscoveredPeripheralCallback passed in to StartScanning() for service_id.
  bool StopScanning(const std::string& service_id) override;

  // Returns true once BLE socket connection requests to service_id can be
  // accepted.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback) override;

  bool StopAcceptingConnections(const std::string& service_id) override;

  // Connects to a BLE peripheral.
  // On success, returns a new BleSocket.
  // On error, returns nullptr.
  std::unique_ptr<api::BleSocket> Connect(api::BlePeripheral& peripheral,
                                          const std::string& service_id,
                                          CancellationFlag* cancellation_flag);

 private:
  void PublisherHandler(
      ::winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisher publisher,
      ::winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisherStatusChangedEventArgs args);
  void AdvertisementReceivedHandler(
      ::winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementWatcher watcher,
      ::winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementReceivedEventArgs args);
  void WatcherHandler(::winrt::Windows::Devices::Bluetooth::Advertisement::
                          BluetoothLEAdvertisementWatcher watcher,
                      ::winrt::Windows::Devices::Bluetooth::Advertisement::
                          BluetoothLEAdvertisementWatcherStoppedEventArgs args);

  BluetoothAdapter* adapter_;
  std::string service_id_;

  DiscoveredPeripheralCallback advertisement_received_callback_;

  // Map to protect the pointer for BlePeripheral because
  // DiscoveredPeripheralCallback only keeps the pointer to the object
  absl::Mutex peripheral_map_mutex_;
  absl::flat_hash_map<std::string, std::unique_ptr<BlePeripheral>>
      peripheral_map_ ABSL_GUARDED_BY(peripheral_map_mutex_);

  // WinRT objects
  ::winrt::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementPublisher publisher_ = nullptr;
  ::winrt::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementWatcher watcher_ = nullptr;

  bool is_publisher_started_ = false;
  bool is_watcher_started_ = false;

  ::winrt::event_token publisher_token_;
  ::winrt::event_token watcher_token_;
  ::winrt::event_token advertisement_received_token_;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_MEDIUM_H_
