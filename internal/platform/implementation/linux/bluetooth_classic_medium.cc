// Copyright 2023 Google LLC
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

#include <cstring>
#include <memory>

#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>

#include "absl/strings/string_view.h"
#include "internal/base/observer_list.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_bluez_profile.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_classic_medium.h"

#include "bluez_agent.h"
#include "internal/platform/implementation/linux/bluetooth_classic_server_socket.h"
#include "internal/platform/implementation/linux/bluetooth_classic_socket.h"
#include "internal/platform/implementation/linux/bluetooth_pairing.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
BluetoothClassicMedium::BluetoothClassicMedium(BluetoothAdapter &adapter)
    : system_bus_(adapter.GetConnection()),
      adapter_(adapter),
      observers_(std::make_shared<ObserverList<Observer>>()),
      devices_(std::make_shared<BluetoothDevices>(
          system_bus_, adapter.GetObjectPath(), *observers_)),
      device_watcher_(nullptr),
      agent_manager_(std::make_unique<AgentManager>(*system_bus_)),
      profile_manager_(
          std::make_unique<ProfileManager>(*system_bus_, *devices_)) {}

bool BluetoothClassicMedium::StartDiscovery(
    DiscoveryCallback discovery_callback) {
  device_watcher_ = std::make_unique<DeviceWatcher>(
      *system_bus_, adapter_.GetObjectPath(), adapter_, devices_,
      std::make_unique<DiscoveryCallback>(std::move(discovery_callback)),
      observers_);

  std::map<std::string, sdbus::Variant> filter;
  filter["Transport"] = "auto";
  auto &adapter = adapter_.GetBluezAdapterObject();

  try {
    adapter.SetDiscoveryFilter(filter);
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(&adapter, "SetDiscoveryFilter", e);
    device_watcher_ = nullptr;
    return false;
  }

  try {
    LOG(INFO) << __func__ << ": Starting BR/EDR discovery on "
                      << adapter_.GetObjectPath();
    adapter.StartDiscovery();
  } catch (const sdbus::Error &e) {
    if (e.getName() != "org.bluez.Error.InProgress") {
      DBUS_LOG_METHOD_CALL_ERROR(&adapter, "StartDiscovery", e);
      device_watcher_ = nullptr;
      return false;
    }
  }

  return true;
}

bool BluetoothClassicMedium::StopDiscovery() {
  auto &adapter = adapter_.GetBluezAdapterObject();
  LOG(INFO) << __func__ << "Stopping discovery on "
                    << adapter.getObjectPath();
  auto ret = true;
  try {
    adapter.StopDiscovery();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(&adapter, "StopDiscovery", e);
    ret = false;
  }
  device_watcher_ = nullptr;

  return ret;
}

std::unique_ptr<api::BluetoothSocket> BluetoothClassicMedium::ConnectToService(
    api::BluetoothDevice &remote_device, const std::string &service_uuid,
    CancellationFlag *cancellation_flag) {
  if (!profile_manager_->ProfileRegistered(service_uuid)) {
    if (!profile_manager_->Register(std::nullopt, service_uuid)) {
      LOG(ERROR) << __func__ << ": Could not register profile "
                         << service_uuid << " with Bluez";
      return nullptr;
    }
  }

  auto address = remote_device.GetMacAddress();
  auto device = devices_->get_device_by_address(address);
  if (device == nullptr) {
    LOG(ERROR) << __func__ << ": Device " << address
                       << " is no longer known";
    return nullptr;
  }
  if (!device -> Bonded())
  {

    LOG(ERROR) << __func__ << ": Device " << address
                       << " is not Bonded";
  }
  // Mark as pending BEFORE calling ConnectToProfile to win the race
  profile_manager_->MarkPendingOutgoing(service_uuid, address);

  if (!device->ConnectToProfile(service_uuid)) {
    profile_manager_->ClearPendingOutgoing(service_uuid, address);
    return nullptr;
  }

  auto fd = profile_manager_->GetServiceRecordFD(remote_device, service_uuid,
                                                 cancellation_flag);
  if (!fd.has_value()) {
    LOG(WARNING) << __func__
                         << ": Failed to get a new connection for profile "
                         << service_uuid << " for device " << address;
    return nullptr;
  }

  return std::unique_ptr<api::BluetoothSocket>(
      new BluetoothSocket(device, fd.value()));
}

std::unique_ptr<api::BluetoothServerSocket>
BluetoothClassicMedium::ListenForService(const std::string &service_name,
                                         const std::string &service_uuid) {
  LOG(INFO) << __func__ << ": Creating bluez agent on path: " << "/com/example/bluez_agent" ;

  if (!agent_manager_ -> AgentRegistered("/com/example/bluez_agent"))
  {
    if (!agent_manager_ -> Register(std::nullopt, "/com/example/bluez_agent"))
    {
      LOG(ERROR) << __func__ << ": Could not register agent " << service_name << " "
      << service_uuid;
      return nullptr;
    }
  }
  if (!profile_manager_->ProfileRegistered(service_uuid)) {
    if (!profile_manager_->Register(service_name, service_uuid)) {
      LOG(ERROR) << __func__ << ": Could not register profile "
                         << service_name << " " << service_uuid
                         << " with Bluez";
      return nullptr;
    }
  }

  return std::unique_ptr<api::BluetoothServerSocket>(
      new BluetoothServerSocket(*profile_manager_, service_uuid));
}

api::BluetoothDevice *BluetoothClassicMedium::GetRemoteDevice(
MacAddress mac_address) {
  auto device = devices_->get_device_by_address(mac_address.ToString());
  if (device == nullptr) return nullptr;

  return device.get();
}

std::unique_ptr<api::BluetoothPairing> BluetoothClassicMedium::CreatePairing(
    api::BluetoothDevice &remote_device) {
  auto device = devices_->get_device_by_address(remote_device.GetMacAddress());
  if (device == nullptr) return nullptr;

  return std::unique_ptr<api::BluetoothPairing>(
      new BluetoothPairing(adapter_, device));
}

}  // namespace linux
}  // namespace nearby
