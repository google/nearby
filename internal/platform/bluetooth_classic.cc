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

#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {

BluetoothClassicMedium::~BluetoothClassicMedium() {
  NEARBY_LOG(INFO, "~BluetoothClassicMedium: observer_list_ size: %d",
             observer_list_.size());
  if (!observer_list_.empty()) {
    impl_->RemoveObserver(this);
  }
  StopDiscovery();
  NEARBY_LOG(INFO, "eof ~BluetoothClassicMedium");
}

BluetoothSocket BluetoothClassicMedium::ConnectToService(
    BluetoothDevice& remote_device, const std::string& service_uuid,
    CancellationFlag* cancellation_flag) {
  NEARBY_LOG(INFO,
             "BluetoothClassicMedium::ConnectToService: service_uuid=%p, "
             "device=%p, [impl=%p]",
             service_uuid.c_str(), &remote_device, &remote_device.GetImpl());
  return BluetoothSocket(impl_->ConnectToService(
      remote_device.GetImpl(), service_uuid, cancellation_flag));
}

bool BluetoothClassicMedium::StartDiscovery(DiscoveryCallback callback) {
  NEARBY_LOG(INFO, "BluetoothClassicMedium::StartDiscovery");
  MutexLock lock(&mutex_);
  if (discovery_enabled_) {
    NEARBY_LOG(INFO, "BT Discovery already enabled; impl=%p", &GetImpl());
    return false;
  }
  bool success = impl_->StartDiscovery({
      .device_discovered_cb =
          [this](api::BluetoothDevice& device) {
            NEARBY_LOG(VERBOSE, "BT .device_discovered_cb for %p",
                       device.GetName().c_str());
            MutexLock lock(&mutex_);
            auto pair = devices_.emplace(
                &device, absl::make_unique<DeviceDiscoveryInfo>());
            auto& context = *pair.first->second;
            if (!pair.second) {
              NEARBY_LOG(INFO, "Adding (again) device=%p, impl=%p",
                         &context.device, &device);
              return;
            }
            context.device = BluetoothDevice(&device);
            NEARBY_LOG(INFO, "Adding device=%p, impl=%p", &context.device,
                       &device);
            if (!discovery_enabled_) return;
            discovery_callback_.device_discovered_cb(context.device);
          },
      .device_name_changed_cb =
          [this](api::BluetoothDevice& device) {
            NEARBY_LOG(VERBOSE, "BT .device_name_changed_cb for %p",
                       device.GetName().c_str());
            MutexLock lock(&mutex_);
            // If the device is not already in devices_, we should not be able
            // to change its name.
            if (devices_.find(&device) == devices_.end()) return;
            auto& context = *devices_[&device];
            NEARBY_LOG(INFO, "Renaming device=%p, impl=%p", &context.device,
                       &device);
            if (!discovery_enabled_) return;
            discovery_callback_.device_name_changed_cb(context.device);
          },
      .device_lost_cb =
          [this](api::BluetoothDevice& device) {
            NEARBY_LOG(VERBOSE, "BT .device_lost_cb for %p",
                       device.GetName().c_str());
            MutexLock lock(&mutex_);
            auto item = devices_.extract(&device);
            if (!item) {
              NEARBY_LOGS(WARNING)
                  << "Removing unknown device: " << device.GetMacAddress();
              return;
            }
            auto& context = *item.mapped();
            NEARBY_LOG(INFO, "Removing device=%p, impl=%p", &context.device,
                       &device);
            if (!discovery_enabled_) return;
            discovery_callback_.device_lost_cb(context.device);
          },
  });
  if (success) {
    discovery_callback_ = std::move(callback);
    devices_.clear();
    discovery_enabled_ = true;
  }
  NEARBY_LOG(INFO, "BT StartDiscovery result:%d; impl=%p", success, &GetImpl());
  return success;
}

bool BluetoothClassicMedium::StopDiscovery() {
  NEARBY_LOG(INFO, "BT StopDiscovery; impl=%p", &GetImpl());
  MutexLock lock(&mutex_);
  if (!discovery_enabled_) return true;
  discovery_enabled_ = false;
  discovery_callback_ = {};
  devices_.clear();
  NEARBY_LOG(INFO, "BT Discovery disabled: impl=%p", &GetImpl());
  return impl_->StopDiscovery();
}

void BluetoothClassicMedium::AddObserver(Observer* observer) {
  NEARBY_LOG(INFO, "BT AddObserver; impl=%p", &GetImpl());
  MutexLock lock(&mutex_);
  if (observer_list_.empty()) {
    impl_->AddObserver(this);
  }
  observer_list_.AddObserver(observer);
  NEARBY_LOG(INFO, "BT AddObserver done");
}
void BluetoothClassicMedium::RemoveObserver(Observer* observer) {
  NEARBY_LOG(INFO, "BT RemoveObserver; impl=%p", &GetImpl());
  MutexLock lock(&mutex_);
  observer_list_.RemoveObserver(observer);
  if (observer_list_.empty()) {
    impl_->RemoveObserver(this);
  }
  NEARBY_LOG(INFO, "BT RemoveObserver done");
}

// api::BluetoothClassicMedium::Observer methods
void BluetoothClassicMedium::DeviceAdded(api::BluetoothDevice& device) {
  NEARBY_LOG(VERBOSE, "BT DeviceAdded; name=%p, address=%p",
             device.GetName().c_str(), device.GetMacAddress().c_str());
  BluetoothDevice bt_device(&device);
  for (auto* observer : observer_list_.GetObservers()) {
    observer->DeviceAdded(bt_device);
  }
}
void BluetoothClassicMedium::DeviceRemoved(api::BluetoothDevice& device) {
  NEARBY_LOG(VERBOSE, "BT DeviceRemoved; name=%p, address=%p",
             device.GetName().c_str(), device.GetMacAddress().c_str());
  BluetoothDevice bt_device(&device);
  for (auto* observer : observer_list_.GetObservers()) {
    observer->DeviceRemoved(bt_device);
  }
}
void BluetoothClassicMedium::DeviceAddressChanged(
    api::BluetoothDevice& device, absl::string_view old_address) {
  NEARBY_LOG(
      VERBOSE, "BT DeviceAddressChanged; name=%p, address=%p, old_address=%p",
      device.GetName().c_str(), device.GetMacAddress().c_str(), old_address);
  BluetoothDevice bt_device(&device);
  for (auto* observer : observer_list_.GetObservers()) {
    observer->DeviceAddressChanged(bt_device, old_address);
  }
}
void BluetoothClassicMedium::DevicePairedChanged(api::BluetoothDevice& device,
                                                 bool new_paired_status) {
  NEARBY_LOG(VERBOSE, "BT DevicePairedChanged; name=%p, address=%p, status=%d",
             device.GetName().c_str(), device.GetMacAddress().c_str(),
             new_paired_status);
  BluetoothDevice bt_device(&device);
  for (auto* observer : observer_list_.GetObservers()) {
    observer->DevicePairedChanged(bt_device, new_paired_status);
  }
}
void BluetoothClassicMedium::DeviceConnectedStateChanged(
    api::BluetoothDevice& device, bool connected) {
  NEARBY_LOG(
      VERBOSE,
      "BT DeviceConnectedStateChanged: name=%p, address=%p, connected=%d",
      device.GetName().c_str(), device.GetMacAddress().c_str(), connected);
  BluetoothDevice bt_device(&device);
  for (auto* observer : observer_list_.GetObservers()) {
    observer->DeviceConnectedStateChanged(bt_device, connected);
  }
}

}  // namespace nearby
