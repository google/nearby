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

#include "internal/platform/implementation/windows/bluetooth_classic_medium.h"

#include <stdio.h>
#include <windows.h>

#include <codecvt>
#include <fstream>
#include <locale>
#include <map>
#include <memory>
#include <regex>  // NOLINT
#include <string>
#include <utility>

#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/bluetooth_classic_device.h"
#include "internal/platform/implementation/windows/bluetooth_classic_server_socket.h"
#include "internal/platform/implementation/windows/bluetooth_classic_socket.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.Rfcomm.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/wifi_lan.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {
namespace {

using winrt::Windows::Foundation::IInspectable;
using winrt::Windows::Foundation::Collections::IMapView;

// Used to cntrol the dump output for device information. It is only for debug
// purpose.
constexpr bool kEnableDumpDeviceInfomation = false;

void DumpDeviceInformation(
    const IMapView<winrt::hstring, IInspectable>& properties) {
  if (!kEnableDumpDeviceInfomation) {
    return;
  }

  if (properties == nullptr) {
    return;
  }

  for (const auto& property : properties) {
    if (property.Key() == L"System.ItemNameDisplay") {
      NEARBY_LOGS(INFO) << "System.ItemNameDisplay: "
                        << InspectableReader::ReadString(property.Value());
    } else if (property.Key() == L"System.Devices.Aep.CanPair") {
      NEARBY_LOGS(INFO) << "System.Devices.Aep.CanPair: "
                        << InspectableReader::ReadBoolean(property.Value());
    } else if (property.Key() == L"System.Devices.Aep.IsPaired") {
      NEARBY_LOGS(INFO) << "System.Devices.Aep.IsPaired: "
                        << InspectableReader::ReadBoolean(property.Value());
    } else if (property.Key() == L"System.Devices.Aep.IsPresent") {
      NEARBY_LOGS(INFO) << "System.Devices.Aep.IsPresent: "
                        << InspectableReader::ReadBoolean(property.Value());
    } else if (property.Key() == L"System.Devices.Aep.DeviceAddress") {
      NEARBY_LOGS(INFO) << "System.Devices.Aep.DeviceAddress: "
                        << InspectableReader::ReadString(property.Value());
    }
  }
}

}  // namespace

constexpr uint8_t kAndroidDiscoverableBluetoothNameMaxLength = 37;  // bytes

BluetoothClassicMedium::BluetoothClassicMedium(
    api::BluetoothAdapter& bluetoothAdapter)
    : bluetooth_adapter_(dynamic_cast<BluetoothAdapter&>(bluetoothAdapter)) {
  InitializeDeviceWatcher();
  bluetooth_adapter_.RestoreRadioNameIfNecessary();

  bluetooth_adapter_.SetOnScanModeChanged(std::bind(
      &BluetoothClassicMedium::OnScanModeChanged, this, std::placeholders::_1));
}

BluetoothClassicMedium::~BluetoothClassicMedium() {}

void BluetoothClassicMedium::OnScanModeChanged(
    BluetoothAdapter::ScanMode scanMode) {
  NEARBY_LOGS(INFO) << __func__
                    << ": OnScanModeChanged is called with scanMode: "
                    << static_cast<int>(scanMode);

  if (scanMode == scan_mode_) {
    NEARBY_LOGS(INFO) << __func__ << ": No change of scan mode.";
    return;
  }

  scan_mode_ = scanMode;
  bool radio_discoverable =
      scan_mode_ == BluetoothAdapter::ScanMode::kConnectableDiscoverable;

  if (bluetooth_adapter_.GetName().size() >
      kAndroidDiscoverableBluetoothNameMaxLength) {
    // If the name longer than the android limitation, always set the value to
    // false.
    radio_discoverable = false;
  }

  if (is_radio_discoverable_ == radio_discoverable) {
    NEARBY_LOGS(INFO) << __func__ << ": No change of radio discovery.";
    return;
  }

  if (rfcomm_provider_ == nullptr) {
    NEARBY_LOGS(INFO) << __func__ << ": No advertising.";
    return;
  }

  try {
    rfcomm_provider_.StopAdvertising();
    rfcomm_provider_.StartAdvertising(
        raw_server_socket_->stream_socket_listener(), radio_discoverable);
    is_radio_discoverable_ = radio_discoverable;
    return;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": OnScanModeChanged exception: " << exception.what();
    return;
  } catch (const winrt::hresult_error& ex) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": OnScanModeChanged exception: " << ex.code() << ": "
                       << winrt::to_string(ex.message());
    return;
  }
}

bool BluetoothClassicMedium::StartDiscovery(
    BluetoothClassicMedium::DiscoveryCallback discovery_callback) {
  NEARBY_LOGS(INFO) << "StartDiscovery is called.";

  bool result = false;
  discovery_callback_ = std::move(discovery_callback);

  if (!IsWatcherStarted()) {
    result = StartScanning();
  }

  return result;
}

bool BluetoothClassicMedium::StopDiscovery() {
  NEARBY_LOGS(INFO) << "StopDiscovery is called.";

  bool result = false;

  if (IsWatcherStarted()) {
    result = StopScanning();
  }

  return result;
}

void BluetoothClassicMedium::InitializeDeviceWatcher() {
  // create watcher
  const winrt::param::iterable<winrt::hstring> RequestedProperties =
      winrt::single_threaded_vector<winrt::hstring>(
          {winrt::to_hstring("System.Devices.Aep.IsPresent"),
           winrt::to_hstring("System.Devices.Aep.DeviceAddress")});

  device_watcher_ = DeviceInformation::CreateWatcher(
      BLUETOOTH_SELECTOR,                           // aqsFilter
      RequestedProperties,                          // additionalProperties
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
  try {
    NEARBY_LOGS(INFO) << "ConnectToService is called.";
    if (service_uuid.empty()) {
      NEARBY_LOGS(ERROR) << __func__ << ": service_uuid not specified.";
      return nullptr;
    }

    const std::regex pattern(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-"
        "F]"
        "{12}$");

    // Must check for valid pattern as the guid constructor will throw on an
    // invalid format
    if (!std::regex_match(service_uuid, pattern)) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": invalid service_uuid: " << service_uuid;
      return nullptr;
    }

    winrt::guid service(service_uuid);

    if (cancellation_flag == nullptr) {
      NEARBY_LOGS(ERROR) << __func__ << ": cancellation_flag not specified.";
      return nullptr;
    }

    remote_device_to_connect_ =
        std::make_unique<BluetoothDevice>(remote_device.GetMacAddress());

    // First try, check if the remote device that we want to request connection
    // to has already been discovered by the Bluetooth Classic Device Watcher
    // beforehand inside the discovered_devices_by_id_ map
    std::map<winrt::hstring, std::unique_ptr<BluetoothDevice>>::const_iterator
        it = discovered_devices_by_id_.find(
            winrt::to_hstring(remote_device_to_connect_->GetId()));

    std::unique_ptr<BluetoothDevice> device = nullptr;
    BluetoothDevice* current_device = nullptr;

    if (it != discovered_devices_by_id_.end()) {
      current_device = it->second.get();
    } else {
      // The remote device was not discovered by the Bluetooth Classic Device
      // Watcher beforehand.
      // Second try, request Windows to scan for nearby
      // bluetooth devices that has this static mac address again in this
      // instance
      auto remote_bluetooth_device_from_mac_address =
          winrt::Windows::Devices::Bluetooth::BluetoothDevice::
              FromBluetoothAddressAsync(
                  mac_address_string_to_uint64(remote_device.GetMacAddress()))
                  .get();
      if (remote_bluetooth_device_from_mac_address == nullptr) {
        NEARBY_LOGS(ERROR) << __func__
                           << ": Windows failed to get remote bluetooth device "
                              "from static mac address.";
        return nullptr;
      }
      device = std::make_unique<BluetoothDevice>(
          remote_bluetooth_device_from_mac_address);
      current_device = device.get();
    }

    if (current_device == nullptr) {
      NEARBY_LOGS(ERROR) << __func__ << ": Failed to get current device.";
      return nullptr;
    }

    winrt::hstring device_id = winrt::to_hstring(current_device->GetId());

    if (!HaveAccess(device_id)) {
      NEARBY_LOGS(ERROR) << __func__ << ": Failed to gain access to device: "
                         << winrt::to_string(device_id);
      return nullptr;
    }

    RfcommDeviceService requested_service(
        GetRequestedService(current_device, service));

    if (!CheckSdp(requested_service)) {
      NEARBY_LOGS(ERROR) << __func__ << ": Invalid SDP.";
      return nullptr;
    }

    auto rfcomm_socket = std::make_unique<BluetoothSocket>();

    if (cancellation_flag->Cancelled()) {
      NEARBY_LOGS(INFO)
          << __func__
          << ": Bluetooth Classic socket connection cancelled for device: "
          << winrt::to_string(device_id) << ", service: " << service_uuid;
      return nullptr;
    }
    nearby::CancellationFlagListener cancellation_flag_listener(
        cancellation_flag, [&rfcomm_socket]() {
          rfcomm_socket->CancelIOAsync().get();
          rfcomm_socket->Close();
        });

    bool success =
        rfcomm_socket->Connect(requested_service.ConnectionHostName(),
                               requested_service.ConnectionServiceName());
    if (!success) {
      return nullptr;
    }

    return std::move(rfcomm_socket);
  } catch (std::exception exception) {
    // We will log and eat the exception since the caller
    // expects nullptr if it fails
    NEARBY_LOGS(ERROR) << __func__ << ": Exception connecting bluetooth async: "
                       << exception.what();
    return nullptr;
  } catch (const winrt::hresult_error& ex) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Exception connecting bluetooth async, error code: "
                       << ex.code()
                       << ", error message: " << winrt::to_string(ex.message());
    return nullptr;
  }
}

bool BluetoothClassicMedium::HaveAccess(winrt::hstring device_id) {
  if (device_id.empty()) {
    return false;
  }

  DeviceAccessInformation access_information =
      DeviceAccessInformation::CreateFromId(device_id);

  if (access_information == nullptr) {
    return false;
  }

  DeviceAccessStatus access_status = access_information.CurrentStatus();

  if (access_status == DeviceAccessStatus::DeniedByUser ||
      // This status is most likely caused by app permissions (did not declare
      // the device in the app's package.appxmanifest)
      // This status does not cover the case where the device is already
      // opened by another app.
      access_status == DeviceAccessStatus::DeniedBySystem ||
      // Most likely the device is opened by another app, but cannot be sure
      access_status == DeviceAccessStatus::Unspecified) {
    return false;
  }

  return true;
}

RfcommDeviceService BluetoothClassicMedium::GetRequestedService(
    BluetoothDevice* device, winrt::guid service) {
  RfcommServiceId rfcommServiceId = RfcommServiceId::FromUuid(service);
  return device->GetRfcommServiceForIdAsync(rfcommServiceId);
}

bool BluetoothClassicMedium::CheckSdp(RfcommDeviceService requestedService) {
  // Do various checks of the SDP record to make sure you are talking to a
  // device that actually supports the Bluetooth Rfcomm Service
  // https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.rfcomm.rfcommdeviceservice.getsdprawattributesasync?view=winrt-20348
  try {
    if (requestedService == nullptr) {
      return false;
    }

    auto attributes = requestedService.GetSdpRawAttributesAsync().get();
    if (!attributes.HasKey(Constants::SdpServiceNameAttributeId)) {
      NEARBY_LOGS(ERROR) << __func__ << ": Missing SdpServiceNameAttributeId.";
      return false;
    }

    auto attributeReader = DataReader::FromBuffer(
        attributes.Lookup(Constants::SdpServiceNameAttributeId));

    auto attributeType = attributeReader.ReadByte();

    if (attributeType != Constants::SdpServiceNameAttributeType) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Missing SdpServiceNameAttributeType.";
      return false;
    }

    return true;
  } catch (...) {
    NEARBY_LOGS(ERROR) << "Failed to get SDP information.";
    return false;
  }
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
  NEARBY_LOGS(INFO) << "ListenForService is called with service name: "
                    << service_name << ".";
  if (service_uuid.empty()) {
    NEARBY_LOGS(ERROR) << __func__ << ": service_uuid was empty.";
    return nullptr;
  }

  if (service_name.empty()) {
    NEARBY_LOGS(ERROR) << __func__ << ": service_name was empty.";
    return nullptr;
  }

  service_name_ = service_name;
  service_uuid_ = service_uuid;

  scan_mode_ = bluetooth_adapter_.GetScanMode();

  NEARBY_LOGS(INFO) << __func__
                    << ": scan_mode: " << static_cast<int>(scan_mode_);
  bool radio_discoverable =
      scan_mode_ == BluetoothAdapter::ScanMode::kConnectableDiscoverable;

  bool result = StartAdvertising(radio_discoverable);

  if (!result) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to start listening.";
    return nullptr;
  }

  return std::move(server_socket_);
}

api::BluetoothDevice* BluetoothClassicMedium::GetRemoteDevice(
    const std::string& mac_address) {
  return new BluetoothDevice(mac_address);
}

bool BluetoothClassicMedium::StartScanning() {
  if (!IsWatcherStarted()) {
    discovered_devices_by_id_.clear();

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
  NEARBY_LOGS(INFO) << "Device added " << winrt::to_string(deviceInfo.Id());
  IMapView<winrt::hstring, IInspectable> properties = deviceInfo.Properties();
  DumpDeviceInformation(properties);

  if (!IsWatcherStarted()) {
    return winrt::fire_and_forget();
  }

  // If device no item name, ignore it.
  if (!properties.HasKey(L"System.ItemNameDisplay")) {
    NEARBY_LOGS(WARNING) << __func__ << ": Ignore the Bluetooth device "
                         << winrt::to_string(deviceInfo.Id())
                         << " due to no name.";
    return winrt::fire_and_forget();
  }

  if (properties.Lookup(L"System.ItemNameDisplay") == nullptr) {
    NEARBY_LOGS(WARNING) << __func__ << ": Ignore the Bluetooth device "
                         << winrt::to_string(deviceInfo.Id())
                         << " due to empty name.";
    return winrt::fire_and_forget();
  }

  // If device doesn't support pair, ignore it.
  if (!properties.HasKey(L"System.Devices.Aep.CanPair")) {
    NEARBY_LOGS(WARNING) << __func__ << ": Ignore the Bluetooth device "
                         << winrt::to_string(deviceInfo.Id())
                         << " due to no pair property.";
    return winrt::fire_and_forget();
  }

  if (!InspectableReader::ReadBoolean(
          properties.Lookup(L"System.Devices.Aep.CanPair"))) {
    NEARBY_LOGS(WARNING) << __func__ << ": Ignore the Bluetooth device "
                         << winrt::to_string(deviceInfo.Id())
                         << " due to not support pair.";
    return winrt::fire_and_forget();
  }

  // Represents a Bluetooth device.
  // https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothdevice?view=winrt-20348
  std::unique_ptr<winrt::Windows::Devices::Bluetooth::BluetoothDevice>
      windowsBluetoothDevice;

  // Create an iterator for the internal list
  std::map<winrt::hstring, std::unique_ptr<BluetoothDevice>>::const_iterator
      it = discovered_devices_by_id_.find(deviceInfo.Id());

  // Add to our internal list if necessary
  if (it != discovered_devices_by_id_.end()) {
    // We're already tracking this one
    NEARBY_LOGS(WARNING) << __func__ << ": Bluetooth device "
                         << winrt::to_string(deviceInfo.Id())
                         << " is alreay added.";
    return winrt::fire_and_forget();
  }

  // Create a bluetooth device out of this id
  auto bluetoothDevice =
      winrt::Windows::Devices::Bluetooth::BluetoothDevice::FromIdAsync(
          deviceInfo.Id())
          .get();
  auto bluetoothDeviceP = std::make_unique<BluetoothDevice>(bluetoothDevice);

  discovered_devices_by_id_[deviceInfo.Id()] = std::move(bluetoothDeviceP);

  if (discovery_callback_.device_discovered_cb != nullptr) {
    discovery_callback_.device_discovered_cb(
        *discovered_devices_by_id_[deviceInfo.Id()]);
  }

  return winrt::fire_and_forget();
}

winrt::fire_and_forget BluetoothClassicMedium::DeviceWatcher_Updated(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  auto it = discovered_devices_by_id_.find(deviceInfoUpdate.Id());

  if (it == discovered_devices_by_id_.end()) {
    NEARBY_LOGS(WARNING) << __func__ << ": Bluetooth device "
                         << winrt::to_string(deviceInfoUpdate.Id())
                         << " is not in list.";
    return winrt::fire_and_forget();
  }

  NEARBY_LOGS(INFO)
      << "Device updated name: "
      << discovered_devices_by_id_[deviceInfoUpdate.Id()]->GetName() << " ("
      << winrt::to_string(deviceInfoUpdate.Id()) << ")";
  IMapView<winrt::hstring, IInspectable> properties =
      deviceInfoUpdate.Properties();
  DumpDeviceInformation(properties);

  if (!IsWatcherStarted()) {
    // Spurious call, watcher has stopped or wasn't started
    return winrt::fire_and_forget();
  }

  if (properties.HasKey(L"System.ItemNameDisplay")) {
    // we need to really change the name of the bluetooth device
    std::string new_device_name = InspectableReader::ReadString(
        properties.Lookup(L"System.ItemNameDisplay"));

    it->second->SetName(new_device_name);

    NEARBY_LOGS(INFO)
        << "Updated device name:"
        << discovered_devices_by_id_[deviceInfoUpdate.Id()]->GetName();

    discovery_callback_.device_name_changed_cb(
        *discovered_devices_by_id_[deviceInfoUpdate.Id()]);
  }

  return winrt::fire_and_forget();
}

winrt::fire_and_forget BluetoothClassicMedium::DeviceWatcher_Removed(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfo) {
  auto it = discovered_devices_by_id_.find(deviceInfo.Id());

  if (it == discovered_devices_by_id_.end()) {
    NEARBY_LOGS(WARNING) << __func__ << ": Bluetooth device "
                         << winrt::to_string(deviceInfo.Id())
                         << " is not in list.";
    return winrt::fire_and_forget();
  }

  NEARBY_LOGS(INFO) << "Device removed "
                    << discovered_devices_by_id_[deviceInfo.Id()]->GetName()
                    << " (" << winrt::to_string(deviceInfo.Id()) << ")";

  if (!IsWatcherStarted()) {
    return winrt::fire_and_forget();
  }

  if (discovery_callback_.device_lost_cb != nullptr) {
    discovery_callback_.device_lost_cb(
        *discovered_devices_by_id_[deviceInfo.Id()]);
  }

  discovered_devices_by_id_.erase(deviceInfo.Id());

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

bool BluetoothClassicMedium::StartAdvertising(bool radio_discoverable) {
  NEARBY_LOGS(INFO) << __func__
                    << ": StartAdvertising is called with radio_discoverable: "
                    << radio_discoverable << ".";

  try {
    if (rfcomm_provider_ != nullptr &&
        is_radio_discoverable_ == radio_discoverable) {
      NEARBY_LOGS(WARNING) << __func__
                           << ": Ignore StartAdvertising due to no change to "
                              "current advertising.";
      return true;
    }

    if (rfcomm_provider_ != nullptr && !StopAdvertising()) {
      NEARBY_LOGS(WARNING) << __func__
                           << ": Failed to StartAdvertising due to cannot stop "
                              "running advertising.";
      return false;
    }

    rfcomm_provider_ =
        RfcommServiceProvider::CreateAsync(
            RfcommServiceId::FromUuid(winrt::guid(service_uuid_)))
            .get();

    server_socket_ = std::make_unique<BluetoothServerSocket>(
        winrt::to_string(rfcomm_provider_.ServiceId().AsString()));

    raw_server_socket_ = server_socket_.get();

    if (!server_socket_->listen()) {
      NEARBY_LOGS(ERROR)
          << __func__
          << ": Failed to StartAdvertising due to cannot start socket.";
      server_socket_->Close();
      server_socket_ = nullptr;
      rfcomm_provider_ = nullptr;
      return false;
    }

    server_socket_->SetCloseNotifier([&]() { StopAdvertising(); });

    // Set the SDP attributes and start Bluetooth advertising
    InitializeServiceSdpAttributes(rfcomm_provider_, service_name_);

    // Start to advertising.
    rfcomm_provider_.StartAdvertising(server_socket_->stream_socket_listener(),
                                      radio_discoverable);
    is_radio_discoverable_ = radio_discoverable;

    NEARBY_LOGS(INFO) << ": StartListening completed successfully.";
    return true;
  } catch (std::exception exception) {
    // We will log and eat the exception since the caller
    // expects nullptr if it fails
    NEARBY_LOGS(ERROR) << __func__ << ": Exception setting up for listen: "
                       << exception.what();

    if (server_socket_ != nullptr) {
      server_socket_->Close();
      server_socket_ = nullptr;
    }

    if (rfcomm_provider_ != nullptr) {
      rfcomm_provider_ = nullptr;
    }

    return false;
  } catch (const winrt::hresult_error& ex) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Exception setting up for listen: " << ex.code()
                       << ": " << winrt::to_string(ex.message());
    if (server_socket_ != nullptr) {
      server_socket_->Close();
      server_socket_ = nullptr;
    }

    if (rfcomm_provider_ != nullptr) {
      rfcomm_provider_ = nullptr;
    }

    return false;
  }
}

bool BluetoothClassicMedium::StopAdvertising() {
  NEARBY_LOGS(INFO) << __func__ << ": StopAdvertising is called";

  try {
    if (rfcomm_provider_ == nullptr) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Ignore StopAdvertising due to no advertising.";
      return true;
    }

    rfcomm_provider_.StopAdvertising();
    rfcomm_provider_ = nullptr;
    raw_server_socket_ = nullptr;
    server_socket_ = nullptr;

    NEARBY_LOGS(INFO) << ": StopAdvertising completed successfully.";
    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": StopAdvertising exception: " << exception.what();
    return false;
  } catch (const winrt::hresult_error& ex) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": StopAdvertising exception: " << ex.code() << ": "
                       << winrt::to_string(ex.message());
    return false;
  }
}

bool BluetoothClassicMedium::InitializeServiceSdpAttributes(
    RfcommServiceProvider rfcomm_provider, std::string service_name) {
  try {
    auto sdpWriter = DataWriter();

    // Write the Service Name Attribute.
    sdpWriter.WriteByte(Constants::SdpServiceNameAttributeType);

    // The length of the UTF-8 encoded Service Name SDP Attribute.
    sdpWriter.WriteByte(service_name.size());

    // The UTF-8 encoded Service Name value.
    sdpWriter.UnicodeEncoding(UnicodeEncoding::Utf8);
    sdpWriter.WriteString(winrt::to_hstring(service_name));

    // Set the SDP Attribute on the RFCOMM Service Provider.
    rfcomm_provider.SdpRawAttributes().Insert(
        Constants::SdpServiceNameAttributeId, sdpWriter.DetachBuffer());

    return true;
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to InitializeServiceSdpAttributes.";
    return false;
  }
}

}  // namespace windows
}  // namespace nearby
