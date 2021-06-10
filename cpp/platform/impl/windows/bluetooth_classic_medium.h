// Copyright 2020 Google LLC
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

#include <Windows.h>
#include <stdio.h>

#include "platform/impl/windows/bluetooth_classic_device.h"
#include "winrt/Windows.Devices.Enumeration.h"
#include "winrt/base.h"

using namespace winrt;
using namespace winrt::Windows::Devices::Enumeration;

namespace winrt::bluetooth_classic_medium::implementation {
#define BLUETOOTH_SELECTOR \
  L"System.Devices.Aep.ProtocolId:=\"{e0cbf06c-cd8b-4647-bb8a-263b43f0f974}\""

// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium
    : public location::nearby::api::BluetoothClassicMedium {
 public:
  explicit BluetoothClassicMedium() {
    InitializeCriticalSection(&critical_section);
  }
  // TODO(b/184975123): replace with real implementation.
  ~BluetoothClassicMedium() override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#startDiscovery()
  bool StartDiscovery(DiscoveryCallback discovery_callback) override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#cancelDiscovery()
  //
  // Returns true once discovery is well and truly stopped; after this returns,
  // there must be no more invocations of the DiscoveryCallback passed in to
  // StartDiscovery().
  // TODO(b/184975123): replace with real implementation.
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
  // On error, returns nullptr.
  // TODO(b/184975123): replace with real implementation.
  std::unique_ptr<location::nearby::api::BluetoothSocket> ConnectToService(
      location::nearby::api::BluetoothDevice& remote_device,
      const std::string& service_uuid,
      location::nearby::CancellationFlag* cancellation_flag) override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#listenUsingInsecureRfcommWithServiceRecord
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  //  Returns nullptr error.
  // TODO(b/184975123): replace with real implementation.
  std::unique_ptr<location::nearby::api::BluetoothServerSocket>
  ListenForService(const std::string& service_name,
                   const std::string& service_uuid) override;

  // TODO(b/184975123): replace with real implementation.
  location::nearby::api::BluetoothDevice* GetRemoteDevice(
      const std::string& mac_address) override;

 private:
  bool start_scanning();
  bool stop_scanning();
  fire_and_forget DeviceWatcher_Added(DeviceWatcher sender,
                                      DeviceInformation deviceInfo);

  fire_and_forget DeviceWatcher_Updated(DeviceWatcher sender,
                                        DeviceInformationUpdate deviceInfo);

  fire_and_forget DeviceWatcher_Removed(DeviceWatcher sender,
                                        DeviceInformationUpdate deviceInfo);

  BluetoothClassicMedium::DiscoveryCallback discovery_callback_;
  DeviceWatcher device_watcher_ = nullptr;
  bool IsWatcherStarted();
  bool IsWatcherRunning();

  std::map<winrt::hstring, std::unique_ptr<location::nearby::windows::BluetoothDevice>> devices_by_id_;
  CRITICAL_SECTION critical_section;
};
}  // namespace winrt::bluetooth_classic_medium::implementation

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_MEDIUM_H_
