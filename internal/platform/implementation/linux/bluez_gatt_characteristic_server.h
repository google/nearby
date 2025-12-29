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

#ifndef PLATFORM_IMPL_LINUX_BLUEZ_GATT_CHARACTERISTIC_SERVER_H_
#define PLATFORM_IMPL_LINUX_BLUEZ_GATT_CHARACTERISTIC_SERVER_H_

#include <atomic>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/MethodResult.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/bluetooth_devices.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/gatt_characteristic_server.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
namespace bluez {
class GattCharacteristicServer final
    : public sdbus::AdaptorInterfaces<org::bluez::GattCharacteristic1_adaptor,
                                      sdbus::Properties_adaptor,
                                      sdbus::ManagedObject_adaptor> {
 public:
  GattCharacteristicServer(const GattCharacteristicServer &) = delete;
  GattCharacteristicServer(GattCharacteristicServer &&) = delete;
  GattCharacteristicServer &operator=(const GattCharacteristicServer &) =
      delete;
  GattCharacteristicServer &operator=(GattCharacteristicServer &&) = delete;

  GattCharacteristicServer(
      sdbus::IConnection &system_bus,
      const sdbus::ObjectPath &service_object_path, size_t num,
      const api::ble_v2::GattCharacteristic &characteristic,
      std::shared_ptr<api::ble_v2::ServerGattConnectionCallback> server_cb,
      std::shared_ptr<BluetoothDevices> devices)
      : AdaptorInterfaces(system_bus, bluez::gatt_characteristic_path(
                                          service_object_path, num)),
        devices_(std::move(devices)),
        server_cb_(std::move(server_cb)),
        characteristic_(characteristic),
        service_object_path_(service_object_path),
        notifying_(false),
        confirmed_(false),
        notify_sessions_(0) {
    registerAdaptor();
    LOG(INFO)
        << __func__ << "Creating a "
        << org::bluez::GattCharacteristic1_adaptor::INTERFACE_NAME
        << " object at " << getObjectPath();
  }
  ~GattCharacteristicServer() { unregisterAdaptor(); }

  void Update(const nearby::ByteArray &value)
      ABSL_LOCKS_EXCLUDED(static_value_mutex_);
  absl::Status NotifyChanged(bool confirm, const ByteArray &new_value)
      ABSL_LOCKS_EXCLUDED(confirmed_mutex_);

 private:
  // Methods
  void ReadValue(sdbus::Result<std::vector<uint8_t>> &&result,
                 std::map<std::string, sdbus::Variant> options) override
      ABSL_LOCKS_EXCLUDED(cached_value_mutex_, static_value_mutex_);
  void WriteValue(sdbus::Result<> &&result, std::vector<uint8_t> value,
                  std::map<std::string, sdbus::Variant> options) override;
  void StartNotify() override;
  void StopNotify() override;
  void Confirm() override ABSL_LOCKS_EXCLUDED(confirmed_mutex_) {
    absl::MutexLock lock(&confirmed_mutex_);
    confirmed_ = true;
  };

  // Properties
  std::string UUID() override { return std::string{characteristic_.uuid}; }
  sdbus::ObjectPath Service() override { return service_object_path_; }
  bool Notifying() override { return notifying_; }
  std::vector<std::string> Flags() override;
  std::vector<uint8_t> Value() override
      ABSL_LOCKS_EXCLUDED(cached_value_mutex_) {
    absl::ReaderMutexLock lock(&cached_value_mutex_);
    return cached_value_;
  }

  std::shared_ptr<BluetoothDevices> devices_;
  std::shared_ptr<api::ble_v2::ServerGattConnectionCallback> server_cb_;
  api::ble_v2::GattCharacteristic characteristic_;

  // Set by `GattServer::UpdateCharacteristic()`
  absl::Mutex static_value_mutex_;
  std::optional<std::vector<uint8_t>> static_value_
      ABSL_GUARDED_BY(static_value_mutex_);

  sdbus::ObjectPath service_object_path_;
  std::atomic_bool notifying_;
  absl::Mutex cached_value_mutex_;
  std::vector<uint8_t> cached_value_ ABSL_GUARDED_BY(cached_value_mutex_);

  absl::Mutex confirmed_mutex_;
  bool confirmed_;

  std::atomic_size_t notify_sessions_;
};

}  // namespace bluez
}  // namespace linux
}  // namespace nearby

#endif
