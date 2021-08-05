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
#include "platform/impl/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "platform/impl/windows/generated/winrt/base.h"

namespace location {
namespace nearby {
namespace windows {

// Represents a device. This class allows access to well-known device properties
// as well as additional properties specified during device enumeration.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.enumeration.deviceinformation?view=winrt-20348
using winrt::Windows::Devices::Enumeration::DeviceInformation;

// Represents the kind of DeviceInformation object.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.enumeration.deviceinformationkind?view=winrt-20348
using winrt::Windows::Devices::Enumeration::DeviceInformationKind;

// Contains updated properties for a DeviceInformation object.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.enumeration.deviceinformationupdate?view=winrt-20348
using winrt::Windows::Devices::Enumeration::DeviceInformationUpdate;

// Enumerates devices dynamically, so that the app receives notifications if
// devices are added, removed, or changed after the initial enumeration is
// complete.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.enumeration.devicewatcher?view=winrt-20348
using winrt::Windows::Devices::Enumeration::DeviceWatcher;

// Describes the state of a DeviceWatcher object.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.enumeration.devicewatcherstatus?view=winrt-20348
using winrt::Windows::Devices::Enumeration::DeviceWatcherStatus;

// Represents an instance of a service on a Bluetooth basic rate device.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.rfcomm.rfcommdeviceservice?view=winrt-20348
using winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommDeviceService;

// Indicates the status of the access to a device.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.enumeration.deviceaccessstatus?view=winrt-20348
using winrt::Windows::Devices::Enumeration::DeviceAccessStatus;

// Contains the information about access to a device.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.enumeration.deviceaccessinformation?view=winrt-20348
using winrt::Windows::Devices::Enumeration::DeviceAccessInformation;

// Represents an RFCOMM service ID.
//  https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.rfcomm.rfcommserviceid?view=winrt-20348
using winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceId;

// Reads data from an input stream.
//  https://docs.microsoft.com/en-us/uwp/api/windows.storage.streams.datareader?view=winrt-20348
using winrt::Windows::Storage::Streams::DataReader;

// Bluetooth protocol ID = \"{e0cbf06c-cd8b-4647-bb8a-263b43f0f974}\"
// https://docs.microsoft.com/en-us/windows/uwp/devices-sensors/aep-service-class-ids
#define BLUETOOTH_SELECTOR \
  L"System.Devices.Aep.ProtocolId:=\"{e0cbf06c-cd8b-4647-bb8a-263b43f0f974}\""

// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium : public api::BluetoothClassicMedium {
 public:
  explicit BluetoothClassicMedium();

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
  // TODO(b/184975123): replace with real implementation.
  std::unique_ptr<api::BluetoothServerSocket> ListenForService(
      const std::string& service_name,
      const std::string& service_uuid) override;

  // TODO(b/184975123): replace with real implementation.
  api::BluetoothDevice* GetRemoteDevice(
      const std::string& mac_address) override;

 private:
  bool StartScanning();
  bool StopScanning();
  bool IsWatcherStarted();
  bool IsWatcherRunning();

  // This is for a coroutine whose return type is winrt::fire_and_forget, which
  // handles async operations which don't have any dependencies.
  // https://docs.microsoft.com/en-us/uwp/cpp-ref-for-winrt/fire-and-forget
  winrt::fire_and_forget DeviceWatcher_Added(DeviceWatcher sender,
                                             DeviceInformation deviceInfo);

  winrt::fire_and_forget DeviceWatcher_Updated(
      DeviceWatcher sender, DeviceInformationUpdate deviceInfo);

  winrt::fire_and_forget DeviceWatcher_Removed(
      DeviceWatcher sender, DeviceInformationUpdate deviceInfo);

  // Check to make sure we can connect if we try
  bool HaveAccess(winrt::hstring deviceId);

  // Get the service requested
  winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommDeviceService
  GetRequestedService(location::nearby::windows::BluetoothDevice* device,
                      winrt::guid service);

  // Check to see that the device actually handles the requested service
  bool CheckSdp(winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommDeviceService
                    requestedService);

  BluetoothClassicMedium::DiscoveryCallback discovery_callback_;

  DeviceWatcher device_watcher_ = nullptr;

  // The Id of the Service Name SDP attribute
  const uint16 SdpServiceNameAttributeId = 0x100;

  // The SDP Type of the Service Name SDP attribute.
  // The first byte in the SDP Attribute encodes the SDP Attribute Type as
  // follows :
  //    -  the Attribute Type size in the least significant 3 bits,
  //    -  the SDP Attribute Type value in the most significant 5 bits.
  const char SdpServiceNameAttributeType = (4 << 3) | 5;

  // hstring is the only type of string winrt understands.
  // https://docs.microsoft.com/en-us/uwp/cpp-ref-for-winrt/hstring
  std::map<winrt::hstring, std::unique_ptr<BluetoothDevice>> devices_by_id_;

  // CRITICAL_SECTION is a lightweight synchronization mechanism
  // https://docs.microsoft.com/en-us/windows/win32/sync/critical-section-objects
  CRITICAL_SECTION critical_section_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_MEDIUM_H_
