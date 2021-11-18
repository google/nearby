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

#include <windows.h>
#include <stdio.h>

#include <codecvt>
#include <locale>
#include <regex>  // NOLINT
#include <string>

#include "platform/base/cancellation_flag.h"
#include "platform/base/cancellation_flag_listener.h"
#include "platform/base/exception.h"
#include "platform/impl/windows/bluetooth_adapter.h"
#include "platform/impl/windows/bluetooth_classic_device.h"
#include "platform/impl/windows/bluetooth_classic_server_socket.h"
#include "platform/impl/windows/bluetooth_classic_socket.h"
#include "platform/impl/windows/generated/winrt/Windows.Devices.Bluetooth.Rfcomm.h"
#include "platform/impl/windows/generated/winrt/Windows.Devices.Bluetooth.h"
#include "platform/impl/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "platform/impl/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "platform/impl/windows/generated/winrt/base.h"
#include "platform/impl/windows/utils.h"
#include "platform/public/logging.h"

namespace location {
namespace nearby {
namespace windows {

BluetoothClassicMedium::BluetoothClassicMedium(
    api::BluetoothAdapter& bluetoothAdapter)
    : bluetooth_adapter_(dynamic_cast<BluetoothAdapter&>(bluetoothAdapter)) {
  InitializeCriticalSection(&critical_section_);

  InitializeDeviceWatcher();

  bluetooth_adapter_.SetOnScanModeChanged(std::bind(
      &BluetoothClassicMedium::OnScanModeChanged, this, std::placeholders::_1));
}

BluetoothClassicMedium::~BluetoothClassicMedium() {}

void BluetoothClassicMedium::OnScanModeChanged(
    BluetoothAdapter::ScanMode scanMode) {
  scan_mode_ = scanMode;
  bool radioDiscoverable = bluetooth_adapter_.GetScanMode() ==
                           BluetoothAdapter::ScanMode::kConnectableDiscoverable;

  if (bluetooth_server_socket_ != nullptr) {
    bluetooth_server_socket_->SetScanMode(radioDiscoverable);
  }
}

bool BluetoothClassicMedium::StartDiscovery(
    BluetoothClassicMedium::DiscoveryCallback discovery_callback) {
  EnterCriticalSection(&critical_section_);

  bool result = false;
  discovery_callback_ = discovery_callback;

  if (!IsWatcherStarted()) {
    result = StartScanning();
  }

  LeaveCriticalSection(&critical_section_);

  return result;
}

bool BluetoothClassicMedium::StopDiscovery() {
  EnterCriticalSection(&critical_section_);

  bool result = false;

  if (IsWatcherStarted()) {
    result = StopScanning();
  }

  LeaveCriticalSection(&critical_section_);

  return result;
}

void BluetoothClassicMedium::InitializeDeviceWatcher() {
  // create watcher
  device_watcher_ = DeviceInformation::CreateWatcher(
      BLUETOOTH_SELECTOR,                           // aqsFilter
      nullptr,                                      // additionalProperties
      DeviceInformationKind::AssociationEndpoint);  // kind

  //  An app must subscribe to all of the added, removed, and updated events to
  //  be notified when there are device additions, removals or updates. If an
  //  app handles only the added event, it will not receive an update if a
  //  device is added to the system after the initial device enumeration
  //  completes. register event handlers before starting the watcher

  //  Event that is raised when a device is added to the collection enumerated
  //  by the DeviceWatcher.
  // https://docs.microsoft.com/en-us/uwp/api/windows.devices.enumeration.devicewatcher.added?view=winrt-20348
  device_watcher_.Added({this, &BluetoothClassicMedium::DeviceWatcher_Added});

  // Event that is raised when a device is updated in the collection of
  // enumerated devices.
  // https://docs.microsoft.com/en-us/uwp/api/windows.devices.enumeration.devicewatcher.updated?view=winrt-20348
  device_watcher_.Updated(
      {this, &BluetoothClassicMedium::DeviceWatcher_Updated});

  // Event that is raised when a device is removed from the collection of
  // enumerated devices.
  // https://docs.microsoft.com/en-us/uwp/api/windows.devices.enumeration.devicewatcher.removed?view=winrt-20348
  device_watcher_.Removed(
      {this, &BluetoothClassicMedium::DeviceWatcher_Removed});
}

std::unique_ptr<api::BluetoothSocket> BluetoothClassicMedium::ConnectToService(
    api::BluetoothDevice& remote_device, const std::string& service_uuid,
    CancellationFlag* cancellation_flag) {
  if (service_uuid.empty()) {
    NEARBY_LOGS(ERROR) << __func__ << ": service_uuid not specified.";
    return nullptr;
  }

  const std::regex pattern(
      "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]"
      "{12}$");

  // Must check for valid pattern as the guid constructor will throw on an
  // invalid format
  if (!regex_match(service_uuid, pattern)) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": invalid service_uuid: " << service_uuid;
    return nullptr;
  }

  winrt::guid service(service_uuid);

  if (cancellation_flag == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": cancellation_flag not specified.";
    return nullptr;
  }

  BluetoothDevice* currentDevice =
      dynamic_cast<BluetoothDevice*>(&remote_device);

  if (currentDevice == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to get current device.";
    return nullptr;
  }

  winrt::hstring deviceId = winrt::to_hstring(currentDevice->GetId());

  if (!HaveAccess(deviceId)) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to gain access to device: "
                       << winrt::to_string(deviceId);
    return nullptr;
  }

  RfcommDeviceService requestedService(
      GetRequestedService(currentDevice, service));

  if (!CheckSdp(requestedService)) {
    NEARBY_LOGS(ERROR) << __func__ << ": Invalid SDP.";
    return nullptr;
  }

  device_watcher_.Stop();

  EnterCriticalSection(&critical_section_);

  std::unique_ptr<BluetoothSocket> rfcommSocket =
      std::make_unique<BluetoothSocket>();

  location::nearby::CancellationFlagListener cancellationFlagListener(
      cancellation_flag,
      [&rfcommSocket]() { rfcommSocket.get()->CancelIOAsync().get(); });

  try {
    rfcommSocket->Connect(requestedService.ConnectionHostName(),
                               requestedService.ConnectionServiceName());
  } catch (std::exception exception) {
    // We will log and eat the exception since the caller
    // expects nullptr if it fails
    NEARBY_LOGS(ERROR) << __func__ << ": Exception connecting bluetooth async: "
                       << exception.what();

    LeaveCriticalSection(&critical_section_);

    return nullptr;
  }

  LeaveCriticalSection(&critical_section_);

  return rfcommSocket;
}

bool BluetoothClassicMedium::HaveAccess(winrt::hstring deviceId) {
  DeviceAccessStatus accessStatus =
      DeviceAccessInformation::CreateFromId(deviceId).CurrentStatus();

  if (accessStatus == DeviceAccessStatus::DeniedByUser ||
      // This status is most likely caused by app permissions (did not declare
      // the device in the app's package.appxmanifest)
      // This status does not cover the case where the device is already opened
      // by another app.
      accessStatus == DeviceAccessStatus::DeniedBySystem ||
      // Most likely the device is opened by another app, but cannot be sure
      accessStatus == DeviceAccessStatus::Unspecified) {
    return false;
  }

  return true;
}

RfcommDeviceService BluetoothClassicMedium::GetRequestedService(
    BluetoothDevice* device, winrt::guid service) {
  RfcommServiceId rfcommServiceId = RfcommServiceId::FromUuid(service);

  // Retrieves all Rfcomm Services on the Remote Bluetooth Device matching the
  // specified RfcommServiceId.
  //  https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothdevice.getrfcommservicesforidasync?view=winrt-20348
  IAsyncOperation<RfcommDeviceServicesResult> rfcommServices =
      device->GetRfcommServicesForIdAsync(rfcommServiceId);

  RfcommDeviceService requestedService(nullptr);

  if (rfcommServices.get().Services().Size() > 0) {
    requestedService = rfcommServices.get().Services().GetAt(0);
  } else {
    NEARBY_LOGS(ERROR) << __func__ << ": No services found.";
    return nullptr;
  }

  return requestedService;
}

bool BluetoothClassicMedium::CheckSdp(RfcommDeviceService requestedService) {
  // Do various checks of the SDP record to make sure you are talking to a
  // device that actually supports the Bluetooth Rfcomm Service
  // https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.rfcomm.rfcommdeviceservice.getsdprawattributesasync?view=winrt-20348
  auto attributes = requestedService.GetSdpRawAttributesAsync().get();
  if (!attributes.HasKey(Constants::SdpServiceNameAttributeId)) {
    NEARBY_LOGS(ERROR) << __func__ << ": Missing SdpServiceNameAttributeId.";
    return false;
  }

  auto attributeReader = DataReader::FromBuffer(
      attributes.Lookup(Constants::SdpServiceNameAttributeId));

  auto attributeType = attributeReader.ReadByte();

  if (attributeType != Constants::SdpServiceNameAttributeType) {
    NEARBY_LOGS(ERROR) << __func__ << ": Missing SdpServiceNameAttributeType.";
    return false;
  }

  return true;
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
std::unique_ptr<api::BluetoothServerSocket>
BluetoothClassicMedium::ListenForService(const std::string& service_name,
                                         const std::string& service_uuid) {
  if (service_uuid.empty()) {
    NEARBY_LOGS(ERROR) << __func__ << ": service_uuid was empty.";
    return nullptr;
  }

  if (service_name.empty()) {
    NEARBY_LOGS(ERROR) << __func__ << ": service_name was empty.";
    return nullptr;
  }

  auto bluetooth_server_socket =
      std::make_unique<location::nearby::windows::BluetoothServerSocket>(
          service_name, service_uuid);

  if (bluetooth_server_socket == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to create the server socket.";
    return nullptr;
  }

  bool radioDiscoverable = bluetooth_adapter_.GetScanMode() ==
                           BluetoothAdapter::ScanMode::kConnectableDiscoverable;

  Exception result = bluetooth_server_socket->StartListening(radioDiscoverable);

  if (result.value != Exception::kSuccess) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to start listening.";
    return nullptr;
  }

  return std::move(bluetooth_server_socket);
}

api::BluetoothDevice* BluetoothClassicMedium::GetRemoteDevice(
    const std::string& mac_address) {
  return new BluetoothDevice(mac_address);
}

bool BluetoothClassicMedium::StartScanning() {
  if (!IsWatcherStarted()) {
    // The Start method can only be called when the DeviceWatcher is in the
    // Created, Stopped or Aborted state.
    auto status = device_watcher_.Status();

    if (status == DeviceWatcherStatus::Created ||
        status == DeviceWatcherStatus::Stopped ||
        status == DeviceWatcherStatus::Aborted) {
      device_watcher_.Start();

      return true;
    }
  }

  NEARBY_LOGS(ERROR)
      << __func__
      << ": Attempted to start scanning when watcher already started.";
  return false;
}

bool BluetoothClassicMedium::StopScanning() {
  if (IsWatcherRunning()) {
    device_watcher_.Stop();
    return true;
  }
  NEARBY_LOGS(ERROR)
      << __func__
      << ": Attempted to stop scanning when watcher already stopped.";
  return false;
}

winrt::fire_and_forget BluetoothClassicMedium::DeviceWatcher_Added(
    DeviceWatcher sender, DeviceInformation deviceInfo) {
  if (IsWatcherStarted()) {
    // Represents a Bluetooth device.
    // https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothdevice?view=winrt-20348
    std::unique_ptr<winrt::Windows::Devices::Bluetooth::BluetoothDevice>
        windowsBluetoothDevice;

    // Create an iterator for the internal list
    std::map<winrt::hstring, std::unique_ptr<BluetoothDevice>>::const_iterator
        it = devices_by_id_.find(deviceInfo.Id());

    // Add to our internal list if necessary
    if (it == devices_by_id_.end()) {
      // Create a bluetooth device out of this id
      winrt::Windows::Devices::Bluetooth::BluetoothDevice::FromIdAsync(
          deviceInfo.Id())
          .Completed(
              [this, deviceInfo, wbd = std::move(windowsBluetoothDevice)](
                  auto&& async,
                  winrt::Windows::Foundation::AsyncStatus status) {
                EnterCriticalSection(&critical_section_);

                std::unique_ptr<BluetoothDevice> bluetoothDevice =
                    std::make_unique<BluetoothDevice>(async.get());

                devices_by_id_[deviceInfo.Id()] = std::move(bluetoothDevice);

                if (discovery_callback_.device_discovered_cb != nullptr) {
                  discovery_callback_.device_discovered_cb(
                      *devices_by_id_[deviceInfo.Id()]);
                }

                LeaveCriticalSection(&critical_section_);
              });
    }
  }

  return winrt::fire_and_forget();
}

winrt::fire_and_forget BluetoothClassicMedium::DeviceWatcher_Updated(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  if (IsWatcherStarted()) {
    // TODO(jfcarroll): Check for device name change
  }

  return winrt::fire_and_forget();
}

winrt::fire_and_forget BluetoothClassicMedium::DeviceWatcher_Removed(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfo) {
  EnterCriticalSection(&critical_section_);

  if (IsWatcherStarted()) {
    if (discovery_callback_.device_lost_cb != nullptr) {
      discovery_callback_.device_lost_cb(*devices_by_id_[deviceInfo.Id()]);
    }

    devices_by_id_.erase(deviceInfo.Id());
  }

  LeaveCriticalSection(&critical_section_);

  return winrt::fire_and_forget();
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

}  // namespace windows
}  // namespace nearby
}  // namespace location
