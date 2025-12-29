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

#ifndef PLATFORM_IMPL_LINUX_BLUEZ_GATT_SERVICE_H_
#define PLATFORM_IMPL_LINUX_BLUEZ_GATT_SERVICE_H_

#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/bluez_gatt_characteristic_server.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/gatt_characteristic_server.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/gatt_service_server.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace linux {
namespace bluez {
class GattServiceServer final
    : public sdbus::AdaptorInterfaces<org::bluez::GattService1_adaptor,
                                      sdbus::ManagedObject_adaptor,
                                      sdbus::Properties_adaptor> {
 public:
  GattServiceServer(const GattServiceServer &) = delete;
  GattServiceServer(GattServiceServer &&) = delete;
  GattServiceServer &operator=(const GattServiceServer &) = delete;
  GattServiceServer &operator=(GattServiceServer &&) = delete;

  GattServiceServer(
      sdbus::IConnection &system_bus, size_t num, const Uuid &service_uuid,
      std::shared_ptr<api::ble_v2::ServerGattConnectionCallback> server_cb,
      std::shared_ptr<BluetoothDevices> devices)
      : AdaptorInterfaces(system_bus, bluez::gatt_service_path(num)),
        devices_(std::move(devices)),
        server_cb_(std::move(server_cb)),
        uuid_(service_uuid),
        primary_(true) {
    registerAdaptor();
    LOG(INFO) << __func__ << ": Created a "
                         << org::bluez::GattService1_adaptor::INTERFACE_NAME
                         << " object at " << getObjectPath();
  }

  ~GattServiceServer() {
    absl::MutexLock lock(&characterstics_mutex_);
    for (auto &[_uuid, characteristic] : characteristics_) {
      LOG(INFO) << __func__ << ": Removing characteristic "
                           << characteristic->getObjectPath();
      try {
        characteristic->emitInterfacesRemovedSignal(
            {org::bluez::GattCharacteristic1_adaptor::INTERFACE_NAME});
      } catch (const sdbus::Error &e) {
        LOG(ERROR)
            << __func__
            << ": error emitting InterfacesRemoved signal for object path "
            << characteristic->getObjectPath() << " with name '" << e.getName()
            << "' and message '" << e.getMessage() << "'";
      }
    }
    unregisterAdaptor();
  }

  bool AddCharacteristic(const Uuid &service_uuid,
                         const Uuid &characteristic_uuid,
                         api::ble_v2::GattCharacteristic::Permission permission,
                         api::ble_v2::GattCharacteristic::Property property)
      ABSL_LOCKS_EXCLUDED(characterstics_mutex_);
  std::shared_ptr<GattCharacteristicServer> GetCharacteristic(const Uuid &uuid)
      ABSL_LOCKS_EXCLUDED(characterstics_mutex_);

 private:
  // Properties
  std::string UUID() override { return uuid_; }
  bool Primary() override { return primary_; }
  sdbus::ObjectPath Device() override { return "/"; }
  std::vector<sdbus::ObjectPath> Includes() override { return {}; }

  absl::Mutex characterstics_mutex_;
  absl::flat_hash_map<Uuid, std::shared_ptr<GattCharacteristicServer>>
      characteristics_ ABSL_GUARDED_BY(characterstics_mutex_);

  std::shared_ptr<BluetoothDevices> devices_;
  std::shared_ptr<api::ble_v2::ServerGattConnectionCallback> server_cb_;

  const std::string uuid_;
  const bool primary_;
};
}  // namespace bluez
}  // namespace linux
}  // namespace nearby

#endif
