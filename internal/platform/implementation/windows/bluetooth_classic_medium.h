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
#include <memory>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
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
#include "internal/platform/mac_address.h"

namespace nearby::windows {

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
  std::shared_ptr<api::BluetoothServerSocket> ListenForService(
      const std::string& service_name,
      const std::string& service_uuid) override;

  api::BluetoothDevice* GetRemoteDevice(MacAddress mac_address) override;

 private:
  bool StartScanning();
  bool StopScanning();
  std::shared_ptr<BluetoothServerSocket> StartAdvertising();
  bool StopAdvertising();
  bool InitializeServiceSdpAttributes(
      winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceProvider
          rfcomm_provider,
      std::string service_name);
  bool IsWatcherStarted();
  bool IsWatcherRunning();
  void InitializeDeviceWatcher();

  // This is for a coroutine whose return type is winrt::fire_and_forget, which
  // handles async operations which don't have any dependencies.
  // https://docs.microsoft.com/en-us/uwp/cpp-ref-for-winrt/fire-and-forget
  winrt::fire_and_forget DeviceWatcher_Added(
      winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      winrt::Windows::Devices::Enumeration::DeviceInformation device_info);

  winrt::fire_and_forget DeviceWatcher_Updated(
      winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      winrt::Windows::Devices::Enumeration::DeviceInformationUpdate
          device_update_info);

  winrt::fire_and_forget DeviceWatcher_Removed(
      winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      winrt::Windows::Devices::Enumeration::DeviceInformationUpdate
          device_update_info);

  // Check to make sure we can connect if we try
  bool HaveAccess(const std::string& device_id);

  // Get the service requested
  winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommDeviceService
  GetRequestedService(BluetoothDevice* device, ::winrt::guid service);

  // Check to see that the device actually handles the requested service
  bool CheckSdp(winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommDeviceService
                    requested_service);

  // Methods to handle bluetooth devices
  bool RemoveRemoteDevice(absl::string_view device_id)
      ABSL_LOCKS_EXCLUDED(devices_map_mutex_);
  BluetoothDevice* AssignRemoteDevice(std::unique_ptr<BluetoothDevice> device)
      ABSL_LOCKS_EXCLUDED(devices_map_mutex_);
  BluetoothDevice* GetRemoteDeviceInternal(absl::string_view device_id)
      ABSL_LOCKS_EXCLUDED(devices_map_mutex_);
  BluetoothDevice* GetRemoteDeviceFromApiDevice(
      api::BluetoothDevice* api_device)
      ABSL_LOCKS_EXCLUDED(devices_map_mutex_);

  BluetoothClassicMedium::DiscoveryCallback discovery_callback_;

  winrt::Windows::Devices::Enumeration::DeviceWatcher device_watcher_ =
      nullptr;

  std::unique_ptr<BluetoothSocket> bluetooth_socket_;

  std::string service_name_;
  std::string service_uuid_;

  absl::Mutex devices_map_mutex_;

  // Map device id to bluetooth device.
  absl::flat_hash_map<std::string, std::unique_ptr<BluetoothDevice>>
      device_id_to_bluetooth_device_map_ ABSL_GUARDED_BY(devices_map_mutex_);

  // Track removed devices.
  std::list<std::unique_ptr<BluetoothDevice>>
      removed_bluetooth_devices_ ABSL_GUARDED_BY(devices_map_mutex_);

  // The caller may call to create Bluetooth device when scanning is off.
  absl::flat_hash_map<std::string, std::unique_ptr<BluetoothDevice>>
      cached_bluetooth_devices_map_ ABSL_GUARDED_BY(devices_map_mutex_);

  BluetoothAdapter& bluetooth_adapter_;

  // Used for advertising.
  winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceProvider
      rfcomm_provider_ = nullptr;
  std::shared_ptr<api::BluetoothServerSocket> server_socket_;
  // Raw pointer to the BluetoothServerSocket impl class that is held by the
  // shared_ptr server_socket_.  The lifetime of this pointer is guaranteed by
  // the shared_ptr.
  BluetoothServerSocket* raw_server_socket_ = nullptr;
};

}  // namespace nearby::windows

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_MEDIUM_H_
