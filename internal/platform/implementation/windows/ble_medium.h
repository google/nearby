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

#include <functional>
#include <future>  //  NOLINT
#include <string>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/ble.h"
#include "winrt/Windows.Devices.Bluetooth.Advertisement.h"

namespace location {
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
      const std::string& fast_advertisement_service_uuid) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool StopAdvertising(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true once the BLE scan has been initiated.
  bool StartScanning(const std::string& service_id,
                     const std::string& fast_advertisement_service_uuid,
                     DiscoveredPeripheralCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true once BLE scanning for service_id is well and truly stopped;
  // after this returns, there must be no more invocations of the
  // DiscoveredPeripheralCallback passed in to StartScanning() for service_id.
  bool StopScanning(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true once BLE socket connection requests to service_id can be
  // accepted.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool StopAcceptingConnections(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Connects to a BLE peripheral.
  // On success, returns a new BleSocket.
  // On error, returns nullptr.
  std::unique_ptr<api::BleSocket> Connect(
      api::BlePeripheral& peripheral, const std::string& service_id,
      CancellationFlag* cancellation_flag) override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  enum class PublisherState { kStarted = 0, kStopped, kError };

  enum class WatcherState { kStarted = 0, kStopped, kError };

  absl::Mutex mutex_;
  api::BluetoothAdapter* adapter_;
  ByteArray advertisement_byte_ ABSL_GUARDED_BY(mutex_);

  DiscoveredPeripheralCallback advertisement_received_callback_;

  // WinRT objects
  ::winrt::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementPublisher publisher_;
  ::winrt::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementWatcher watcher_;
  ::winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisement
      advertisement_;

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

  ::winrt::event_token publisher_token_;
  std::promise<PublisherState> publisher_started_promise_;
  std::promise<PublisherState> publisher_stopped_promise_;

  ::winrt::event_token watcher_token_;
  std::promise<WatcherState> watcher_started_promise_;
  std::promise<WatcherState> watcher_stopped_promise_;
  bool is_watcher_started_ = false;
  bool is_watcher_stopped_ = false;

  ::winrt::event_token advertisement_received_token_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_MEDIUM_H_
