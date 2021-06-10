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

#include "platform/impl/windows/bluetooth_classic_medium.h"

#include "platform/impl/windows/bluetooth_classic_device.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Devices.Enumeration.h"
#include "winrt/Windows.Foundation.Collections.h"

using namespace winrt;
using namespace winrt::Windows::Devices::Enumeration;

namespace winrt::bluetooth_classic_medium::implementation {
BluetoothClassicMedium::~BluetoothClassicMedium() {}

// TODO(b/184975123): replace with real implementation.
location::nearby::api::BluetoothDevice* GetRemoteDevice(
    const std::string& mac_address) {
  return nullptr;
}

bool BluetoothClassicMedium::StartDiscovery(
    BluetoothClassicMedium::DiscoveryCallback discovery_callback) {
  EnterCriticalSection(&critical_section);

  bool result = false;
  discovery_callback_ = discovery_callback;

  if (!IsWatcherStarted()) {
    result = start_scanning();
  }

  LeaveCriticalSection(&critical_section);

  return result;
}

bool BluetoothClassicMedium::StopDiscovery() {
  EnterCriticalSection(&critical_section);

  bool result = false;

  if (IsWatcherStarted()) {
    result = stop_scanning();
  }

  LeaveCriticalSection(&critical_section);

  return result;
}

std::unique_ptr<location::nearby::api::BluetoothSocket>
BluetoothClassicMedium::ConnectToService(
    location::nearby::api::BluetoothDevice& remote_device,
    const std::string& service_uuid,
    location::nearby::CancellationFlag* cancellation_flag) {
  return nullptr;
}

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
BluetoothClassicMedium::ListenForService(const std::string& service_name,
                                         const std::string& service_uuid) {
  return nullptr;
}

// TODO(b/184975123): replace with real implementation.
location::nearby::api::BluetoothDevice* BluetoothClassicMedium::GetRemoteDevice(
    const std::string& mac_address) {
  return nullptr;
}

bool BluetoothClassicMedium::start_scanning() {
  if (!IsWatcherStarted()) {
    // required properties
    auto requestedProperties = winrt::single_threaded_vector<hstring>(
        {L"System.Devices.Aep.DeviceAddress"});

    // create watcher
    device_watcher_ = DeviceInformation::CreateWatcher(
        BLUETOOTH_SELECTOR, nullptr,
        DeviceInformationKind::AssociationEndpoint);

    // register event handlers before starting the watcher.
    event_token devicePairedWatcherAddedToken = device_watcher_.Added(
        {this, &BluetoothClassicMedium::DeviceWatcher_Added});

    event_token devicePairedWatcherUpdatedToken = device_watcher_.Updated(
        {this, &BluetoothClassicMedium::DeviceWatcher_Updated});

    event_token devicePairedWatcherRemovedToken = device_watcher_.Removed(
        {this, &BluetoothClassicMedium::DeviceWatcher_Removed});

    // The Start method can only be called when the DeviceWatcher is in the
    // Created, Stopped or Aborted state.
    auto status = device_watcher_.Status();

    if (status == winrt::Windows::Devices::Enumeration::DeviceWatcherStatus::
                      Created ||
        status == winrt::Windows::Devices::Enumeration::DeviceWatcherStatus::
                      Stopped ||
        status == winrt::Windows::Devices::Enumeration::DeviceWatcherStatus::
                      Aborted) {
      device_watcher_.Start();

      return true;
    }
  }

  return false;
}

bool BluetoothClassicMedium::stop_scanning() {
  if (IsWatcherRunning()) {
    device_watcher_.Stop();
    return true;
  }

  return false;
}

fire_and_forget BluetoothClassicMedium::DeviceWatcher_Added(
    DeviceWatcher sender, DeviceInformation deviceInfo) {
  if (IsWatcherStarted()) {
    std::unique_ptr<location::nearby::windows::BluetoothDevice>
        bluetoothDevice =
            std::make_unique<location::nearby::windows::BluetoothDevice>(
                location::nearby::windows::BluetoothDevice(
                    deviceInfo.Id(), deviceInfo.Name(), deviceInfo.Kind(),
                    deviceInfo.Properties()));

    std::map<winrt::hstring,
             std::unique_ptr<location::nearby::windows::BluetoothDevice>>::
        const_iterator it = devices_by_id_.find(deviceInfo.Id());

    if (it == devices_by_id_.end()) {
      devices_by_id_[deviceInfo.Id()] = std::move(bluetoothDevice);
    }

    if (discovery_callback_.device_discovered_cb != nullptr) {
      discovery_callback_.device_discovered_cb(*devices_by_id_[deviceInfo.Id()]);
    }
  }

  return fire_and_forget();
}

fire_and_forget BluetoothClassicMedium::DeviceWatcher_Updated(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  auto currentDevice = std::move(devices_by_id_[deviceInfoUpdate.Id()]);
  if (IsWatcherStarted()) {
    // Check for device name change
  }

  return fire_and_forget();
}

fire_and_forget BluetoothClassicMedium::DeviceWatcher_Removed(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfo) {
  if (IsWatcherStarted()) {
    std::unique_ptr<location::nearby::api::BluetoothDevice> bluetoothDevice =
        std::move(devices_by_id_[deviceInfo.Id()]);

    devices_by_id_.erase(deviceInfo.Id());

    if (discovery_callback_.device_lost_cb != nullptr) {
      discovery_callback_.device_lost_cb(*bluetoothDevice);
    }
  }

  return fire_and_forget();
}

bool BluetoothClassicMedium::IsWatcherStarted() {
  if (device_watcher_ == nullptr) {
    return false;
  }
  DeviceWatcherStatus status = device_watcher_.Status();
  return (status == DeviceWatcherStatus::Started) ||
         (status == DeviceWatcherStatus::EnumerationCompleted);
}

bool BluetoothClassicMedium::IsWatcherRunning() {
  if (device_watcher_ == nullptr) {
    return false;
  }

  DeviceWatcherStatus status = device_watcher_.Status();
  return (status == DeviceWatcherStatus::Started) ||
         (status == DeviceWatcherStatus::EnumerationCompleted) ||
         (status == DeviceWatcherStatus::Stopping);
}

}  // namespace winrt::bluetooth_classic_medium::implementation