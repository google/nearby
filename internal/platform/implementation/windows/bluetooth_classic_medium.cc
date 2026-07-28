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

#include "internal/platform/implementation/windows/bluetooth_classic_medium.h"

#include <windows.h>

#include <cstdint>
#include <functional>
#include <ios>
#include <memory>
#include <regex>  // NOLINT
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/bluetooth_classic_device.h"
#include "internal/platform/implementation/windows/bluetooth_classic_server_socket.h"
#include "internal/platform/implementation/windows/bluetooth_classic_socket.h"
#include "internal/platform/implementation/windows/bluetooth_pairing.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.Rfcomm.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/mac_address.h"

namespace nearby::windows {
namespace {
using ::winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommDeviceService;
using ::winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceId;
using ::winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceProvider;
using ::winrt::Windows::Devices::Enumeration::DeviceAccessInformation;
using ::winrt::Windows::Devices::Enumeration::DeviceAccessStatus;
using ::winrt::Windows::Devices::Enumeration::DeviceInformation;
using ::winrt::Windows::Devices::Enumeration::DeviceInformationKind;
using ::winrt::Windows::Devices::Enumeration::DeviceInformationUpdate;
using ::winrt::Windows::Devices::Enumeration::DeviceWatcher;
using ::winrt::Windows::Devices::Enumeration::DeviceWatcherStatus;
using ::winrt::Windows::Foundation::IInspectable;
using ::winrt::Windows::Foundation::Collections::IMapView;
using ::winrt::Windows::Storage::Streams::DataReader;
using ::winrt::Windows::Storage::Streams::DataWriter;
using ::winrt::Windows::Storage::Streams::UnicodeEncoding;

// Used to control the dump output for device information. It is only for debug
// purpose.
constexpr bool kEnableDumpDeviceInfomation = true;
// The maximum length of Bluetooth device name Android can discover.
constexpr int kAndroidDiscoverableBluetoothNameMaxLength = 37;  // bytes
// Used to select bluetooth devices.
constexpr wchar_t kBluetoothSelector[] =
    L"System.Devices.Aep.ProtocolId:=\"{e0cbf06c-cd8b-4647-bb8a-263b43f0f974}"
    L"\"";

// The Id of the Service Name SDP attribute
constexpr uint16_t SdpServiceNameAttributeId = 0x100;

// The SDP Type of the Service Name SDP attribute.
// The first byte in the SDP Attribute encodes the SDP Attribute Type as
// follows:
//    -  the Attribute Type size in the least significant 3 bits,
//    -  the SDP Attribute Type value in the most significant 5 bits.
constexpr char SdpServiceNameAttributeType = (4 << 3) | 5;

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
      LOG(INFO) << "System.ItemNameDisplay: "
                << InspectableReader::ReadString(property.Value());
    } else if (property.Key() == L"System.Devices.Aep.CanPair") {
      LOG(INFO) << "System.Devices.Aep.CanPair: "
                << InspectableReader::ReadBoolean(property.Value());
    } else if (property.Key() == L"System.Devices.Aep.IsPaired") {
      LOG(INFO) << "System.Devices.Aep.IsPaired: "
                << InspectableReader::ReadBoolean(property.Value());
    } else if (property.Key() == L"System.Devices.Aep.IsPresent") {
      LOG(INFO) << "System.Devices.Aep.IsPresent: "
                << InspectableReader::ReadBoolean(property.Value());
    } else if (property.Key() == L"System.Devices.Aep.DeviceAddress") {
      LOG(INFO) << "System.Devices.Aep.DeviceAddress: "
                << InspectableReader::ReadString(property.Value());
    }
  }
}

}  // namespace

BluetoothClassicMedium::BluetoothClassicMedium(
    api::BluetoothAdapter& bluetooth_adapter)
    : bluetooth_adapter_(dynamic_cast<BluetoothAdapter&>(bluetooth_adapter)) {
  InitializeDeviceWatcher();
}

BluetoothClassicMedium::~BluetoothClassicMedium() {
  // Clear the close notifier to prevent UAF if the server_socket_ outlives
  // the BluetoothClassicMedium.
  if (raw_server_socket_ != nullptr) {
    raw_server_socket_->SetCloseNotifier(nullptr);
  }
}

bool BluetoothClassicMedium::StartDiscovery(
    BluetoothClassicMedium::DiscoveryCallback discovery_callback) {
  VLOG(1) << "StartDiscovery is called.";

  bool result = false;
  discovery_callback_ = std::move(discovery_callback);

  if (!IsWatcherStarted()) {
    result = StartScanning();
  }

  return result;
}

bool BluetoothClassicMedium::StopDiscovery() {
  VLOG(1) << "StopDiscovery is called.";

  bool result = false;

  if (IsWatcherStarted()) {
    result = StopScanning();
    discovery_callback_ = {};
  }

  return result;
}

std::unique_ptr<api::BluetoothSocket> BluetoothClassicMedium::ConnectToService(
    api::BluetoothDevice& remote_device, const std::string& service_uuid,
    CancellationFlag* cancellation_flag) {
  try {
    LOG(INFO) << "ConnectToService is called.";
    if (service_uuid.empty()) {
      LOG(ERROR) << __func__ << ": service_uuid not specified.";
      return nullptr;
    }

    const std::regex pattern(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-"
        "F]"
        "{12}$");

    // Must check for valid pattern as the guid constructor will throw on an
    // invalid format
    if (!std::regex_match(service_uuid, pattern)) {
      LOG(ERROR) << __func__ << ": invalid service_uuid: " << service_uuid;
      return nullptr;
    }

    winrt::guid service(service_uuid);

    if (cancellation_flag == nullptr) {
      LOG(ERROR) << __func__ << ": cancellation_flag not specified.";
      return nullptr;
    }

    BluetoothDevice* remote_device_to_connect_ =
        GetRemoteDeviceFromApiDevice(&remote_device);

    if (remote_device_to_connect_ == nullptr ||
        remote_device_to_connect_->GetId().empty()) {
      LOG(ERROR) << __func__
                 << ": Failed to get remote device to connect.";
      return nullptr;
    }

    std::string device_id = remote_device_to_connect_->GetId();

    if (!HaveAccess(device_id)) {
      LOG(ERROR) << __func__
                 << ": Failed to gain access to device: " << device_id;
      return nullptr;
    }

    RfcommDeviceService requested_service(
        GetRequestedService(remote_device_to_connect_, service));

    if (!FeatureFlags::GetInstance()
             .GetFlags()
             .skip_service_discovery_before_connecting_to_rfcomm &&
        !CheckSdp(requested_service)) {
      LOG(ERROR) << __func__ << ": Invalid SDP.";
      return nullptr;
    }

    auto rfcomm_socket = std::make_unique<BluetoothSocket>();

    if (cancellation_flag->Cancelled()) {
      LOG(INFO)
          << __func__
          << ": Bluetooth Classic socket connection cancelled for device: "
          << device_id << ", service: " << service_uuid;
      return nullptr;
    }
    nearby::CancellationFlagListener cancellation_flag_listener(
        cancellation_flag, [&rfcomm_socket]() { rfcomm_socket->Close(); });

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
    LOG(ERROR) << __func__ << ": Exception connecting bluetooth async: "
               << exception.what();
    return nullptr;
  } catch (const winrt::hresult_error& ex) {
    LOG(ERROR) << __func__
               << ": Exception connecting bluetooth async, error code: "
               << ex.code()
               << ", error message: " << winrt::to_string(ex.message());
    return nullptr;
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
    return nullptr;
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
std::shared_ptr<api::BluetoothServerSocket>
BluetoothClassicMedium::ListenForService(const std::string& service_name,
                                         const std::string& service_uuid) {
  VLOG(1) << "ListenForService is called with service name: " << service_name
          << ".";
  if (service_uuid.empty()) {
    LOG(ERROR) << __func__ << ": service_uuid was empty.";
    return nullptr;
  }

  if (service_name.empty()) {
    LOG(ERROR) << __func__ << ": service_name was empty.";
    return nullptr;
  }

  service_name_ = service_name;
  service_uuid_ = service_uuid;

  if (rfcomm_provider_ != nullptr) {
    LOG(WARNING) << __func__
                  << ": Ignore StartAdvertising due to no change to "
                    "current advertising.";
    return server_socket_;
  }

  auto server_socket = StartAdvertising();

  if (!server_socket) {
    LOG(ERROR) << __func__ << ": Failed to start listening.";
    return nullptr;
  }

  raw_server_socket_ = server_socket.get();
  server_socket_ = std::move(server_socket);
  return server_socket_;
}

api::BluetoothDevice* BluetoothClassicMedium::GetRemoteDevice(
    MacAddress mac_address) {
  VLOG(1) << "GetRemoteDevice is called with mac_address: "
          << mac_address.ToString();
  absl::MutexLock lock(devices_map_mutex_);
  for (auto& [device_id, bluetooth_device] :
        device_id_to_bluetooth_device_map_) {
    if (bluetooth_device->GetMacAddress() == mac_address) {
      return bluetooth_device.get();
    }
  }
  for (auto& [device_id, bluetooth_device] :
        cached_bluetooth_devices_map_) {
    if (bluetooth_device->GetMacAddress() == mac_address) {
      return bluetooth_device.get();
    }
  }
  // If device is not known, create using MacAddress and add to cached
  // devices.
  auto native_bluetooth_device = winrt::Windows::Devices::Bluetooth::
      BluetoothDevice::FromBluetoothAddressAsync(mac_address.address())
          .get();
  if (native_bluetooth_device == nullptr) {
    LOG(ERROR) << __func__
               << ": Failed to get native bluetooth device from mac address: "
               << mac_address.ToString();
    return nullptr;
  }
  auto bt_device = std::make_unique<BluetoothDevice>(native_bluetooth_device);
  std::string device_id = bt_device->GetId();
  VLOG(1) << __func__ << ": Created BluetoothDevice " << device_id
          << " from mac address: " << mac_address.ToString();
  auto result = cached_bluetooth_devices_map_.insert_or_assign(
      device_id, std::move(bt_device));
  return result.first->second.get();
}

void BluetoothClassicMedium::InitializeDeviceWatcher() {
  try {
    // create watcher
    const winrt::param::iterable<winrt::hstring> requested_properties =
        winrt::single_threaded_vector<winrt::hstring>(
            {winrt::to_hstring("System.Devices.Aep.IsPresent"),
             winrt::to_hstring("System.Devices.Aep.DeviceAddress")});

    device_watcher_ = DeviceInformation::CreateWatcher(
        kBluetoothSelector,                           // aqsFilter
        requested_properties,                         // additionalProperties
        DeviceInformationKind::AssociationEndpoint);  // kind

    //  An app must subscribe to all of the added, removed, and updated events
    //  to be notified when there are device additions, removals or updates. If
    //  an app handles only the added event, it will not receive an update if a
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
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": InitializeDeviceWatcher exception: " << exception.what();
  } catch (const winrt::hresult_error& ex) {
    LOG(ERROR) << __func__
               << ": InitializeDeviceWatcher exception: " << ex.code() << ": "
               << winrt::to_string(ex.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }
}

bool BluetoothClassicMedium::HaveAccess(const std::string& device_id) {
  if (device_id.empty()) {
    return false;
  }

  DeviceAccessInformation access_information =
      DeviceAccessInformation::CreateFromId(winrt::to_hstring(device_id));

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
  RfcommServiceId rfcomm_service_id = RfcommServiceId::FromUuid(service);
  return device->GetRfcommServiceForIdAsync(rfcomm_service_id);
}

bool BluetoothClassicMedium::CheckSdp(RfcommDeviceService requested_service) {
  // Do various checks of the SDP record to make sure you are talking to a
  // device that actually supports the Bluetooth Rfcomm Service
  // https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.rfcomm.rfcommdeviceservice.getsdprawattributesasync?view=winrt-20348
  try {
    if (requested_service == nullptr) {
      LOG(WARNING) << __func__ << ": Request service is empty.";
      return false;
    }

    auto attributes = requested_service.GetSdpRawAttributesAsync().get();
    if (!attributes.HasKey(SdpServiceNameAttributeId)) {
      LOG(ERROR) << __func__ << ": Missing SdpServiceNameAttributeId.";
      return false;
    }

    auto attribute_reader =
        DataReader::FromBuffer(attributes.Lookup(SdpServiceNameAttributeId));

    auto attribute_type = attribute_reader.ReadByte();

    if (attribute_type != SdpServiceNameAttributeType) {
      LOG(ERROR) << __func__ << ": Missing SdpServiceNameAttributeType.";
      return false;
    }

    return true;
  } catch (...) {
    LOG(ERROR) << "Failed to get SDP information.";
    return false;
  }
}

bool BluetoothClassicMedium::RemoveRemoteDevice(absl::string_view device_id) {
  absl::MutexLock lock(devices_map_mutex_);

  auto it = device_id_to_bluetooth_device_map_.find(device_id);
  if (it != device_id_to_bluetooth_device_map_.end()) {
    auto node = device_id_to_bluetooth_device_map_.extract(device_id);
    removed_bluetooth_devices_.push_back(std::move(node.mapped()));
    return true;
  }

  return false;
}

BluetoothDevice* BluetoothClassicMedium::AssignRemoteDevice(
    std::unique_ptr<BluetoothDevice> device) {
  absl::MutexLock lock(devices_map_mutex_);
  std::string device_id = device->GetId();
  auto result = device_id_to_bluetooth_device_map_.insert_or_assign(
      device_id, std::move(device));
  return result.first->second.get();
}

BluetoothDevice* BluetoothClassicMedium::GetRemoteDeviceInternal(
    absl::string_view device_id) {
  absl::MutexLock lock(devices_map_mutex_);
  auto it = device_id_to_bluetooth_device_map_.find(device_id);
  if (it == device_id_to_bluetooth_device_map_.end()) {
    LOG(WARNING) << __func__ << ": Bluetooth device " << device_id
                  << " is not in list.";
    return nullptr;
  }
  return it->second.get();
}

BluetoothDevice* BluetoothClassicMedium::GetRemoteDeviceFromApiDevice(
    api::BluetoothDevice* api_device) {
  absl::MutexLock lock(devices_map_mutex_);
  for (auto& [device_id, bluetooth_device] :
        device_id_to_bluetooth_device_map_) {
    if (bluetooth_device.get() == api_device) {
      return bluetooth_device.get();
    }
  }
  for (auto& [device_id, bluetooth_device] :
        cached_bluetooth_devices_map_) {
    if (bluetooth_device.get() == api_device) {
      return bluetooth_device.get();
    }
  }
  return nullptr;
}

bool BluetoothClassicMedium::StartScanning() {
  if (!IsWatcherStarted()) {
    if (device_watcher_ == nullptr) {
      LOG(ERROR) << __func__
                 << ": Failed to start scanning due to no available watcher.";
      return false;
    }

    {
      absl::MutexLock lock(devices_map_mutex_);
      device_id_to_bluetooth_device_map_.clear();
      removed_bluetooth_devices_.clear();
      if (!cached_bluetooth_devices_map_.empty()) {
        for (auto& [device_id, bluetooth_device] :
             cached_bluetooth_devices_map_) {
          device_id_to_bluetooth_device_map_[device_id] =
              std::move(bluetooth_device);
        }

        cached_bluetooth_devices_map_.clear();
      }
    }

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

  LOG(ERROR) << __func__
             << ": Attempted to start scanning when watcher already started.";
  return false;
}

bool BluetoothClassicMedium::StopScanning() {
  if (IsWatcherRunning()) {
    device_watcher_.Stop();
    return true;
  }
  LOG(ERROR) << __func__
             << ": Attempted to stop scanning when watcher already stopped.";
  return false;
}

winrt::fire_and_forget BluetoothClassicMedium::DeviceWatcher_Added(
    DeviceWatcher sender, DeviceInformation device_info) {
  std::string device_id = winrt::to_string(device_info.Id());
  LOG(INFO) << "Device added " << device_id;
  if (!IsWatcherStarted()) {
    LOG(WARNING) << __func__ << ": Ignore the Bluetooth device " << device_id
                 << " due to watcher not started.";
    return winrt::fire_and_forget();
  }

  if (GetRemoteDeviceInternal(device_id) != nullptr) {
    // We're already tracking this one
    LOG(WARNING) << __func__ << ": Bluetooth device " << device_id
                 << " is alreay added.";
    return winrt::fire_and_forget();
  }
  IMapView<winrt::hstring, IInspectable> properties = device_info.Properties();
  DumpDeviceInformation(properties);

  // If device no item name, ignore it.
  if (!properties.HasKey(L"System.ItemNameDisplay")) {
    LOG(WARNING) << __func__ << ": Ignore the Bluetooth device " << device_id
                 << " due to no name.";
    return winrt::fire_and_forget();
  }

  std::string device_name = InspectableReader::ReadString(
      properties.Lookup(L"System.ItemNameDisplay"));
  if (device_name.empty()) {
    LOG(WARNING) << __func__ << ": Ignore the Bluetooth device " << device_id
                 << " due to empty name.";
    return winrt::fire_and_forget();
  }

  // If device doesn't support pair, ignore it.
  if (!properties.HasKey(L"System.Devices.Aep.CanPair")) {
    LOG(WARNING) << __func__ << ": Ignore the Bluetooth device " << device_id
                 << " due to no pair property.";
    return winrt::fire_and_forget();
  }

  if (!InspectableReader::ReadBoolean(
          properties.Lookup(L"System.Devices.Aep.CanPair"))) {
    LOG(WARNING) << __func__ << ": Ignore the Bluetooth device " << device_id
                 << " due to not support pair.";
    return winrt::fire_and_forget();
  }

  // Get device mac address.
  if (!properties.HasKey(L"System.Devices.Aep.DeviceAddress")) {
    LOG(WARNING) << __func__ << ": Ignore the Bluetooth device " << device_id
                 << " with no mac address.";
    return winrt::fire_and_forget();
  }

  std::string mac_address_string = InspectableReader::ReadString(
      properties.Lookup(L"System.Devices.Aep.DeviceAddress"));
  if (mac_address_string.empty()) {
    LOG(WARNING) << __func__ << ": Ignore the Bluetooth device " << device_id
                 << " cannot read mac address.";
    return winrt::fire_and_forget();
  }
  MacAddress mac_address;
  if (!MacAddress::FromString(mac_address_string, mac_address)) {
    LOG(WARNING) << __func__ << ": Ignore the Bluetooth device " << device_id
                 << " cannot parse mac address: " << mac_address_string;
    return winrt::fire_and_forget();
  }
  std::unique_ptr<BluetoothDevice> bt_device;
  {
    absl::MutexLock lock(devices_map_mutex_);
    // Check if device is already in the cached devices map.
    auto it = cached_bluetooth_devices_map_.find(device_id);
    if (it != cached_bluetooth_devices_map_.end()) {
      auto node = cached_bluetooth_devices_map_.extract(it);
      bt_device = std::move(node.mapped());
    }
  }
  if (bt_device == nullptr) {
    bt_device =
        std::make_unique<BluetoothDevice>(device_id, device_name, mac_address);
  }
  BluetoothDevice* device = AssignRemoteDevice(std::move(bt_device));

  LOG(INFO) << __func__ << ": Notifying bluetooth device " << device_id
            << " added";
  if (discovery_callback_.device_discovered_cb != nullptr) {
    discovery_callback_.device_discovered_cb(*device);
  }
  return winrt::fire_and_forget();
}

winrt::fire_and_forget BluetoothClassicMedium::DeviceWatcher_Updated(
    DeviceWatcher sender, DeviceInformationUpdate device_update_info) {
  std::string device_id = winrt::to_string(device_update_info.Id());
  LOG(INFO) << "Device updated " << device_id;
  if (!IsWatcherStarted()) {
    LOG(WARNING) << __func__ << ": Ignore the Bluetooth device " << device_id
                 << " due to watcher not started.";
    return winrt::fire_and_forget();
  }
  BluetoothDevice* device = GetRemoteDeviceInternal(device_id);
  if (device == nullptr) {
    LOG(ERROR) << __func__ << ": Failed to get remote device.";
    return winrt::fire_and_forget();
  }

  LOG(INFO) << "Device updated name: " << device->GetName() << " ("
            << device->GetId() << ")";
  IMapView<winrt::hstring, IInspectable> properties =
      device_update_info.Properties();
  DumpDeviceInformation(properties);

  // https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/device-information-properties#associationendpoint-properties
  if (properties.HasKey(L"System.ItemNameDisplay")) {
    // we need to really change the name of the bluetooth device
    std::string new_device_name = InspectableReader::ReadString(
        properties.Lookup(L"System.ItemNameDisplay"));

    if (device->GetName() == new_device_name) {
      LOG(INFO) << "Device name is same as old name, ignore the update.";
    } else {
      device->SetName(new_device_name);

      LOG(INFO) << "Updated device name:" << device->GetName();

      if (discovery_callback_.device_name_changed_cb != nullptr) {
        discovery_callback_.device_name_changed_cb(*device);
      }
    }
  }

  // Indicates if the device is currently paired.
  if (properties.HasKey(L"System.Devices.Aep.IsPaired")) {
    bool new_paired_status = InspectableReader::ReadBoolean(
        properties.Lookup(L"System.Devices.Aep.IsPaired"));
    LOG(INFO) << __func__
              << ": Notifying device paired changed: " << std::boolalpha
              << new_paired_status;
  }

  return winrt::fire_and_forget();
}

winrt::fire_and_forget BluetoothClassicMedium::DeviceWatcher_Removed(
    DeviceWatcher sender, DeviceInformationUpdate device_update_info) {
  std::string device_id = winrt::to_string(device_update_info.Id());
  LOG(INFO) << "Device removed " << device_id;
  if (!IsWatcherStarted()) {
    LOG(WARNING) << __func__ << ": Ignore the Bluetooth device " << device_id
                 << " due to watcher not started.";
    return winrt::fire_and_forget();
  }

  api::BluetoothDevice* device = GetRemoteDeviceInternal(device_id);
  if (device == nullptr) {
    LOG(ERROR) << __func__ << ": Failed to get remote device.";
    return winrt::fire_and_forget();
  }

  LOG(INFO) << __func__ << ": Notifying bluetooth device (" << device_id
            << ") removed";
  if (discovery_callback_.device_lost_cb != nullptr) {
    discovery_callback_.device_lost_cb(*device);
  }

  RemoveRemoteDevice(device_id);

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

std::shared_ptr<BluetoothServerSocket>
BluetoothClassicMedium::StartAdvertising() {
  LOG(INFO) << __func__ << ": StartAdvertising is called.";

  std::shared_ptr<BluetoothServerSocket> server_socket;
  try {
    if (rfcomm_provider_ != nullptr && !StopAdvertising()) {
      LOG(WARNING) << __func__
                   << ": Failed to StartAdvertising due to cannot stop "
                      "running advertising.";
      return nullptr;
    }

    rfcomm_provider_ =
        RfcommServiceProvider::CreateAsync(
            RfcommServiceId::FromUuid(winrt::guid(service_uuid_)))
            .get();

    server_socket = BluetoothServerSocket::Create(
        winrt::to_string(rfcomm_provider_.ServiceId().AsString()));

    if (!server_socket->listen()) {
      LOG(ERROR) << __func__
                 << ": Failed to StartAdvertising due to cannot start socket.";
      rfcomm_provider_ = nullptr;
      return nullptr;
    }

    server_socket->SetCloseNotifier([&]() { StopAdvertising(); });

    // Set the SDP attributes and start Bluetooth advertising
    InitializeServiceSdpAttributes(rfcomm_provider_, service_name_);

    // Start to advertising.
    rfcomm_provider_.StartAdvertising(server_socket->stream_socket_listener(),
                                      /*is_discoverable=*/false);

    LOG(INFO) << "StartListening completed successfully.";
    return server_socket;
  } catch (std::exception exception) {
    // We will log and eat the exception since the caller
    // expects nullptr if it fails
    LOG(ERROR) << __func__
               << ": Exception setting up for listen: " << exception.what();

    if (rfcomm_provider_ != nullptr) {
      rfcomm_provider_ = nullptr;
    }
    return nullptr;
  } catch (const winrt::hresult_error& ex) {
    LOG(ERROR) << __func__ << ": Exception setting up for listen: " << ex.code()
               << ": " << winrt::to_string(ex.message());
    if (rfcomm_provider_ != nullptr) {
      rfcomm_provider_ = nullptr;
    }
    return nullptr;
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
    if (rfcomm_provider_ != nullptr) {
      rfcomm_provider_ = nullptr;
    }
    return nullptr;
  }
}

bool BluetoothClassicMedium::StopAdvertising() {
  VLOG(1) << __func__ << ": StopAdvertising is called";
  bool result = false;
  try {
    if (rfcomm_provider_ == nullptr) {
      LOG(ERROR) << __func__
                 << ": Ignore StopAdvertising due to no advertising.";
      return true;
    }

    rfcomm_provider_.StopAdvertising();

    LOG(INFO) << ": StopAdvertising completed successfully.";
    result = true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": StopAdvertising exception: " << exception.what();
  } catch (const winrt::hresult_error& ex) {
    LOG(ERROR) << __func__ << ": StopAdvertising exception: " << ex.code()
               << ": " << winrt::to_string(ex.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }

  rfcomm_provider_ = nullptr;
  server_socket_ = nullptr;
  raw_server_socket_ = nullptr;
  return result;
}

bool BluetoothClassicMedium::InitializeServiceSdpAttributes(
    RfcommServiceProvider rfcomm_provider, std::string service_name) {
  try {
    auto sdp_writer = DataWriter();

    // Write the Service Name Attribute.
    sdp_writer.WriteByte(SdpServiceNameAttributeType);

    // The length of the UTF-8 encoded Service Name SDP Attribute.
    sdp_writer.WriteByte(service_name.size());

    // The UTF-8 encoded Service Name value.
    sdp_writer.UnicodeEncoding(UnicodeEncoding::Utf8);
    sdp_writer.WriteString(winrt::to_hstring(service_name));

    // Set the SDP Attribute on the RFCOMM Service Provider.
    rfcomm_provider.SdpRawAttributes().Insert(SdpServiceNameAttributeId,
                                              sdp_writer.DetachBuffer());

    return true;
  } catch (...) {
    LOG(ERROR) << __func__ << ": Failed to InitializeServiceSdpAttributes.";
    return false;
  }
}

}  // namespace nearby::windows
