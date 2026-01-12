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

#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_DEVICE_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_DEVICE_H_

#include <atomic>
#include <memory>

#include <sdbus-c++/Error.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/base/observer_list.h"
// #include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluez_device.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/device_client.h"

namespace nearby {
namespace linux {
// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.

class BluetoothDevice : public api::BluetoothDevice {
 public:
  using UniqueId = std::uint64_t;

  BluetoothDevice(const BluetoothDevice &) = delete;
  BluetoothDevice(BluetoothDevice &&) = delete;
  BluetoothDevice &operator=(const BluetoothDevice &) = delete;
  BluetoothDevice &operator=(BluetoothDevice &&) = delete;

  explicit BluetoothDevice(std::shared_ptr<bluez::Device> device);

  std::string GetName() const override;
  std::string GetMacAddress() const override;
  std::string GetAddressType() const;

  MacAddress GetAddress() const override { return last_known_address_; }

  std::optional<std::map<std::string, sdbus::Variant>> ServiceData() {
    auto device = device_;
    if (!device) return std::nullopt;

    try {
      return device->ServiceData();
    } catch (const sdbus::Error &e) {
      DBUS_LOG_PROPERTY_GET_ERROR(device, "ServiceData", e);
      return std::nullopt;
    }
  }

  bool Bonded() {
    auto device = device_;
    if (!device) return false;

    try {
      return device->Bonded();
    } catch (const sdbus::Error &e) {
      DBUS_LOG_METHOD_CALL_ERROR(device, "Bonded", e);
      return false;
    }
  }

  std::optional<sdbus::PendingAsyncCall> Pair() {
    auto device = device_;
    if (!device) return std::nullopt;

    try {
      return device->Pair();
    } catch (const sdbus::Error &e) {
      DBUS_LOG_METHOD_CALL_ERROR(device, "Pair", e);
      return std::nullopt;
    }
  }

  bool CancelPairing() {
    auto device = device_;
    if (!device) return false;

    try {
      device->CancelPairing();
      return true;
    } catch (const sdbus::Error &e) {
      DBUS_LOG_METHOD_CALL_ERROR(device, "CancelPairing", e);
      return false;
    }
  }

  void SetPairReplyCallback(absl::AnyInvocable<void(const sdbus::Error *)> cb) {
    auto device = device_;
    if (device) device->SetPairReplyCallback(std::move(cb));
  }

  bool ConnectToProfile(absl::string_view service_uuid);
  void MarkLost() { lost_ = true; }
  void UnmarkLost() { lost_ = false; }
  bool Lost() const { return lost_; }

 private:
  UniqueId unique_id_;
  std::atomic_bool lost_;

  mutable absl::Mutex properties_mutex_;
  mutable std::string last_known_name_ ABSL_GUARDED_BY(properties_mutex_);
  mutable MacAddress last_known_address_ ABSL_GUARDED_BY(properties_mutex_);

  std::shared_ptr<bluez::Device> device_;
};

class MonitoredBluetoothDevice final
    : public BluetoothDevice,
      public sdbus::ProxyInterfaces<sdbus::Properties_proxy> {
 public:
  using sdbus::ProxyInterfaces<sdbus::Properties_proxy>::registerProxy;
  using sdbus::ProxyInterfaces<sdbus::Properties_proxy>::unregisterProxy;
  using sdbus::ProxyInterfaces<sdbus::Properties_proxy>::getObjectPath;

  MonitoredBluetoothDevice(const MonitoredBluetoothDevice &) = delete;
  MonitoredBluetoothDevice(MonitoredBluetoothDevice &&) = delete;
  MonitoredBluetoothDevice &operator=(const MonitoredBluetoothDevice &) =
      delete;
  MonitoredBluetoothDevice &operator=(MonitoredBluetoothDevice &&) = delete;
  MonitoredBluetoothDevice(
      std::shared_ptr<sdbus::IConnection> system_bus,
      std::shared_ptr<bluez::Device> device,
      ObserverList<api::BluetoothClassicMedium::Observer> &observers);
  ~MonitoredBluetoothDevice() override { unregisterProxy(); }

  void SetDiscoveryCallback(
      std::shared_ptr<api::BluetoothClassicMedium::DiscoveryCallback> &callback)
      ABSL_LOCKS_EXCLUDED(discovery_cb_mutex_) {
    absl::MutexLock lock(&discovery_cb_mutex_);
    discovery_cb_ = callback;
  };

 protected:
  void onPropertiesChanged(
      const std::string &interfaceName,
      const std::map<std::string, sdbus::Variant> &changedProperties,
      const std::vector<std::string> &invalidatedProperties) override;

 private:
  std::shared_ptr<sdbus::IConnection> system_bus_;
  std::shared_ptr<api::BluetoothClassicMedium::DiscoveryCallback>
  GetDiscoveryCallback() ABSL_LOCKS_EXCLUDED(discovery_cb_mutex_) {
    discovery_cb_mutex_.ReaderLock();
    auto callback = discovery_cb_.lock();
    discovery_cb_mutex_.ReaderUnlock();

    return callback;
  }

  ObserverList<api::BluetoothClassicMedium::Observer> &observers_;
  absl::Mutex discovery_cb_mutex_;
  std::weak_ptr<api::BluetoothClassicMedium::DiscoveryCallback> discovery_cb_
      ABSL_GUARDED_BY(discovery_cb_mutex_);
};

}  // namespace linux
}  // namespace nearby

#endif
