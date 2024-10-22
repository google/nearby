// Copyright 2020-2023 Google LLC
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

#ifndef PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_MEDIUM_H_
#define PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_MEDIUM_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "internal/base/observer_list.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/bluetooth_classic_device.h"
#include "internal/platform/implementation/windows/bluetooth_classic_server_socket.h"
#include "internal/platform/implementation/windows/bluetooth_classic_socket.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"

namespace nearby {
namespace windows {

// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium : public api::BluetoothClassicMedium {
 public:
  explicit BluetoothClassicMedium(api::BluetoothAdapter& bluetooth_adapter);
  ~BluetoothClassicMedium() override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#startDiscovery()
  bool StartDiscovery(DiscoveryCallback discovery_callback) override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#cancelDiscovery()
  //
  // Returns true once discovery is well and truly stopped; after this returns,
  // there must be no more invocations of the DiscoveryCallback passed in to
  // StartDiscovery().
  bool StopDiscovery() override;

  // A combination of
  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#createInsecureRfcommSocketToServiceRecord
  // followed by
  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#connect().
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  // On success, returns a new BluetoothSocket.
  // On error, throw's an exception
  std::unique_ptr<api::BluetoothSocket> ConnectToService(
      api::BluetoothDevice& remote_device, const std::string& service_uuid,
      CancellationFlag* cancellation_flag) override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#listenUsingInsecureRfcommWithServiceRecord
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  //  Returns nullptr error.
  std::unique_ptr<api::BluetoothServerSocket> ListenForService(
      const std::string& service_name,
      const std::string& service_uuid) override;

  api::BluetoothDevice* GetRemoteDevice(
      const std::string& mac_address) override;

  // Return a Bluetooth pairing instance to handle the pairing process with the
  // remote device.
  std::unique_ptr<api::BluetoothPairing> CreatePairing(
      api::BluetoothDevice& remote_device) override;

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  // Removes an observer. It's OK to remove an unregistered observer.
  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

 private:
  bool StartScanning();
  bool StopScanning();
  bool StartAdvertising(bool radio_discoverable);
  bool StopAdvertising();
  bool InitializeServiceSdpAttributes(
      ::winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceProvider
          rfcomm_provider,
      std::string service_name);
  bool IsWatcherStarted();
  bool IsWatcherRunning();
  void InitializeDeviceWatcher();
  void OnScanModeChanged(BluetoothAdapter::ScanMode scan_mode);

  // This is for a coroutine whose return type is winrt::fire_and_forget, which
  // handles async operations which don't have any dependencies.
  // https://docs.microsoft.com/en-us/uwp/cpp-ref-for-winrt/fire-and-forget
  winrt::fire_and_forget DeviceWatcher_Added(
      ::winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      ::winrt::Windows::Devices::Enumeration::DeviceInformation device_info);

  winrt::fire_and_forget DeviceWatcher_Updated(
      ::winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      ::winrt::Windows::Devices::Enumeration::DeviceInformationUpdate
          device_update_info);

  winrt::fire_and_forget DeviceWatcher_Removed(
      ::winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      ::winrt::Windows::Devices::Enumeration::DeviceInformationUpdate
          device_update_info);

  // Check to make sure we can connect if we try
  bool HaveAccess(::winrt::hstring device_id);

  // Get the service requested
  RfcommDeviceService GetRequestedService(BluetoothDevice* device,
                                          ::winrt::guid service);

  // Check to see that the device actually handles the requested service
  bool CheckSdp(RfcommDeviceService requested_service);

  BluetoothClassicMedium::DiscoveryCallback discovery_callback_;

  ::winrt::Windows::Devices::Enumeration::DeviceWatcher device_watcher_ =
      nullptr;

  std::unique_ptr<BluetoothSocket> bluetooth_socket_;

  std::string service_name_;
  std::string service_uuid_;

  // Map MAC address to bluetooth device.
  absl::flat_hash_map<std::string, std::unique_ptr<BluetoothDevice>>
      mac_address_to_bluetooth_device_map_;

  // Track removed devices.
  absl::flat_hash_map<std::string, std::unique_ptr<BluetoothDevice>>
      removed_bluetooth_devices_map_;

  BluetoothAdapter& bluetooth_adapter_;

  BluetoothAdapter::ScanMode scan_mode_ = BluetoothAdapter::ScanMode::kUnknown;

  // Used for advertising.
  ::winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceProvider
      rfcomm_provider_ = nullptr;
  std::unique_ptr<BluetoothServerSocket> server_socket_ = nullptr;
  BluetoothServerSocket* raw_server_socket_ = nullptr;
  bool is_radio_discoverable_ = false;
  ObserverList<Observer> observers_;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_MEDIUM_H_
