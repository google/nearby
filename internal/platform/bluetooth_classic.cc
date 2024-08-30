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

#include "internal/platform/bluetooth_classic.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/socket.h"

namespace nearby {
using location::nearby::proto::connections::Medium;

MediumSocket* BluetoothSocket::CreateVirtualSocket(OutputStream* outputstream) {
  if (IsVirtualSocket()) {
    NEARBY_LOGS(WARNING)
        << "Creating the virtual socket on a virtual socket is not allowed.";
    return nullptr;
  }
  auto virtual_socket = std::make_shared<BluetoothSocket>(outputstream);
  return virtual_socket.get();
}

MediumSocket* BluetoothSocket::CreateVirtualSocket(
    const std::string& salted_service_id_hash_key, OutputStream* outputstream,
    Medium medium,
    absl::flat_hash_map<std::string, std::shared_ptr<MediumSocket>>*
        virtual_sockets_ptr) {
  if (IsVirtualSocket()) {
    NEARBY_LOGS(WARNING)
        << "Creating the virtual socket on a virtual socket is not allowed.";
    return nullptr;
  }

  auto virtual_socket = std::make_shared<BluetoothSocket>(outputstream);
  virtual_socket->impl_ = this->impl_;
  NEARBY_LOGS(WARNING) << "Created the virtual socket for Medium: "
                       << Medium_Name(virtual_socket->GetMedium());

  if (virtual_sockets_ptr_ == nullptr) {
    virtual_sockets_ptr_ = virtual_sockets_ptr;
  }

  (*virtual_sockets_ptr_)[salted_service_id_hash_key] = virtual_socket;
  NEARBY_LOGS(INFO) << "virtual_sockets_ size: "
                    << virtual_sockets_ptr_->size();
  return virtual_socket.get();
}

BluetoothClassicMedium::~BluetoothClassicMedium() {
  NEARBY_LOGS(INFO) << "~BluetoothClassicMedium: observer_list_ size: "
                    << observer_list_.size();
  if (!observer_list_.empty()) {
    impl_->RemoveObserver(this);
  }
  StopDiscovery();
  NEARBY_LOGS(INFO) << "eof ~BluetoothClassicMedium";
}

BluetoothSocket BluetoothClassicMedium::ConnectToService(
    BluetoothDevice& remote_device, const std::string& service_uuid,
    CancellationFlag* cancellation_flag) {
  NEARBY_LOGS(INFO) << "BluetoothClassicMedium::ConnectToService: "
                       "service_uuid="
                    << service_uuid
                    << ", device=" << remote_device.GetMacAddress()
                    << ", [impl=" << &remote_device.GetImpl() << "]";
  return BluetoothSocket(impl_->ConnectToService(
      remote_device.GetImpl(), service_uuid, cancellation_flag));
}

bool BluetoothClassicMedium::StartDiscovery(DiscoveryCallback callback) {
  NEARBY_LOGS(INFO) << "BluetoothClassicMedium::StartDiscovery";
  MutexLock lock(&mutex_);
  if (discovery_enabled_) {
    NEARBY_LOGS(INFO) << "BT Discovery already enabled; impl=" << &GetImpl();
    return false;
  }
  bool success = impl_->StartDiscovery({
      .device_discovered_cb =
          [this](api::BluetoothDevice& device) {
            NEARBY_VLOG(1) << "BT .device_discovered_cb for "
                           << device.GetName();
            MutexLock lock(&mutex_);
            auto pair = devices_.emplace(
                &device, std::make_unique<DeviceDiscoveryInfo>());
            auto& context = *pair.first->second;
            if (!pair.second) {
              NEARBY_LOGS(INFO)
                  << "Adding (again) device=" << context.device.GetMacAddress()
                  << ",impl=" << &device;
              return;
            }
            context.device = BluetoothDevice(&device);
            NEARBY_LOGS(INFO)
                << "Adding device=" << context.device.GetMacAddress()
                << ",impl=" << &device;
            if (!discovery_enabled_) return;
            discovery_callback_.device_discovered_cb(context.device);
          },
      .device_name_changed_cb =
          [this](api::BluetoothDevice& device) {
            NEARBY_VLOG(1) << "BT .device_name_changed_cb for "
                           << device.GetName();
            MutexLock lock(&mutex_);
            // If the device is not already in devices_, we should not be able
            // to change its name.
            if (devices_.find(&device) == devices_.end()) return;
            auto& context = *devices_[&device];
            NEARBY_LOGS(INFO)
                << "Renaming device=" << context.device.GetMacAddress()
                << ",impl=" << &device;
            if (!discovery_enabled_) return;
            discovery_callback_.device_name_changed_cb(context.device);
          },
      .device_lost_cb =
          [this](api::BluetoothDevice& device) {
            NEARBY_VLOG(1) << "BT .device_lost_cb for "
                           << device.GetMacAddress();
            MutexLock lock(&mutex_);
            auto item = devices_.extract(&device);
            if (!item) {
              NEARBY_LOGS(WARNING)
                  << "Removing unknown device: " << device.GetMacAddress();
              return;
            }
            auto& context = *item.mapped();
            NEARBY_LOGS(INFO)
                << "Removing device=" << context.device.GetMacAddress()
                << ",impl=" << &device;
            if (!discovery_enabled_) return;
            discovery_callback_.device_lost_cb(context.device);
          },
  });
  if (success) {
    discovery_callback_ = std::move(callback);
    devices_.clear();
    discovery_enabled_ = true;
  }
  NEARBY_LOGS(INFO) << "BT StartDiscovery result:" << success
                    << ", impl=" << &GetImpl();
  return success;
}

bool BluetoothClassicMedium::StopDiscovery() {
  NEARBY_LOGS(INFO) << "BT StopDiscovery; impl=" << &GetImpl();
  MutexLock lock(&mutex_);
  if (!discovery_enabled_) return true;
  discovery_enabled_ = false;
  discovery_callback_ = {};
  devices_.clear();
  NEARBY_LOGS(INFO) << "BT Discovery disabled: impl=" << &GetImpl();
  return impl_->StopDiscovery();
}

void BluetoothClassicMedium::AddObserver(Observer* observer) {
  NEARBY_LOGS(INFO) << "BT AddObserver; impl=" << &GetImpl();
  MutexLock lock(&mutex_);
  if (observer_list_.empty()) {
    impl_->AddObserver(this);
  }
  observer_list_.AddObserver(observer);
  NEARBY_LOGS(INFO) << "BT AddObserver done";
}
void BluetoothClassicMedium::RemoveObserver(Observer* observer) {
  NEARBY_LOGS(INFO) << "BT RemoveObserver; impl=" << &GetImpl();
  MutexLock lock(&mutex_);
  observer_list_.RemoveObserver(observer);
  if (observer_list_.empty()) {
    impl_->RemoveObserver(this);
  }
  NEARBY_LOGS(INFO) << "BT RemoveObserver done";
}

// api::BluetoothClassicMedium::Observer methods
void BluetoothClassicMedium::DeviceAdded(api::BluetoothDevice& device) {
  NEARBY_VLOG(1) << "BT DeviceAdded; name=" << device.GetName()
                 << ", address=" << device.GetMacAddress();
  BluetoothDevice bt_device(&device);
  for (auto* observer : observer_list_.GetObservers()) {
    observer->DeviceAdded(bt_device);
  }
}
void BluetoothClassicMedium::DeviceRemoved(api::BluetoothDevice& device) {
  NEARBY_VLOG(1) << "BT DeviceRemoved; name=" << device.GetName()
                 << ", address=" << device.GetMacAddress();
  BluetoothDevice bt_device(&device);
  for (auto* observer : observer_list_.GetObservers()) {
    observer->DeviceRemoved(bt_device);
  }
}
void BluetoothClassicMedium::DeviceAddressChanged(
    api::BluetoothDevice& device, absl::string_view old_address) {
  NEARBY_VLOG(1) << "BT DeviceAddressChanged; name=" << device.GetName()
                 << ", address=" << device.GetMacAddress()
                 << ", old_address=" << old_address;
  BluetoothDevice bt_device(&device);
  for (auto* observer : observer_list_.GetObservers()) {
    observer->DeviceAddressChanged(bt_device, old_address);
  }
}
void BluetoothClassicMedium::DevicePairedChanged(api::BluetoothDevice& device,
                                                 bool new_paired_status) {
  NEARBY_VLOG(1) << "BT DevicePairedChanged; name=" << device.GetName()
                 << ", address=" << device.GetMacAddress()
                 << ", status=" << new_paired_status;
  BluetoothDevice bt_device(&device);
  for (auto* observer : observer_list_.GetObservers()) {
    observer->DevicePairedChanged(bt_device, new_paired_status);
  }
}
void BluetoothClassicMedium::DeviceConnectedStateChanged(
    api::BluetoothDevice& device, bool connected) {
  NEARBY_VLOG(1) << "BT DeviceConnectedStateChanged: name=" << device.GetName()
                 << ", address=" << device.GetMacAddress()
                 << ", connected=" << connected;
  BluetoothDevice bt_device(&device);
  for (auto* observer : observer_list_.GetObservers()) {
    observer->DeviceConnectedStateChanged(bt_device, connected);
  }
}

}  // namespace nearby
