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

#include "internal/platform/implementation/linux/ble_gatt_server.h"
#include "absl/strings/substitute.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/bluez_gatt_characteristic_server.h"
#include "internal/platform/implementation/linux/bluez_gatt_manager.h"
#include "internal/platform/implementation/linux/bluez_gatt_service_server.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/gatt_service_server.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace linux {
absl::optional<api::ble_v2::GattCharacteristic>
GattServer::CreateCharacteristic(
    const Uuid& service_uuid, const Uuid& characteristic_uuid,
    api::ble_v2::GattCharacteristic::Permission permission,
    api::ble_v2::GattCharacteristic::Property property) {
  absl::MutexLock lock(&services_mutex_);
  if (services_.count(service_uuid) == 1) {
    if (services_[service_uuid]->AddCharacteristic(
            service_uuid, characteristic_uuid, permission, property)) {
      api::ble_v2::GattCharacteristic characteristic{
          characteristic_uuid, service_uuid, permission, property};
      return characteristic;
    }
    return std::nullopt;
  }

  auto count = services_.size();
  auto service = std::make_unique<bluez::GattServiceServer>(
      system_bus_, count, service_uuid, server_cb_, devices_);
  try {
    service->emitInterfacesAddedSignal(
        {org::bluez::GattService1_adaptor::INTERFACE_NAME});
  } catch (const sdbus::Error& e) {
    LOG(ERROR)
        << __func__
        << ": error emitting InterfacesAdded signal for object path "
        << service->getObjectPath() << " with name '" << e.getName()
        << "' and message '" << e.getMessage() << "'";
    return std::nullopt;
  }
  auto profile = std::make_unique<bluez::GattProfile> (system_bus_, bluez::gatt_profile_object_path(
    std::string(service_uuid)), std::string(service_uuid));
  profile -> emitInterfacesAddedSignal();

  if (service->AddCharacteristic(service_uuid, characteristic_uuid, permission,
                                 property)) {
  try {
    LOG(INFO)<< __func__ << ": Registering service on gattmanager";
    gatt_manager_ -> RegisterApplication(gatt_service_root_object_manager -> getObjectPath(), {});
  } catch (const sdbus::Error& e) {
    LOG(ERROR)
        << __func__
        << ": error calling RegisterAplication for GattManager with object path "
        << gatt_manager_->getObjectPath() << " with name '" << e.getName()
        << "' and message '" << e.getMessage() << "'";
    return std::nullopt;
  }


    services_.insert({service_uuid, std::move(service)});

    api::ble_v2::GattCharacteristic characteristic{
        characteristic_uuid, service_uuid, permission, property};
    return characteristic;
  }

  return std::nullopt;
}

bool GattServer::UpdateCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    const nearby::ByteArray& value) {
  std::shared_ptr<bluez::GattCharacteristicServer> chr = nullptr;
  {
    absl::ReaderMutexLock lock(&services_mutex_);
    if (services_.count(characteristic.service_uuid) == 0) {
      LOG(ERROR) << __func__ << ": GATT Service "
                         << std::string{characteristic.service_uuid}
                         << " doesn't exist";
      return false;
    }
    chr = services_[characteristic.service_uuid]->GetCharacteristic(
        characteristic.uuid);
  }
  if (chr == nullptr) {
    LOG(ERROR) << __func__ << ": Characteristic "
                       << std::string{characteristic.uuid}
                       << " does not exist under service "
                       << std::string{characteristic.service_uuid};
    return false;
  }
  assert(chr != nullptr);
  chr->Update(value);
  return true;
}

absl::Status GattServer::NotifyCharacteristicChanged(
    const api::ble_v2::GattCharacteristic& characteristic, bool confirm,
    const ByteArray& new_value) {
  std::shared_ptr<bluez::GattCharacteristicServer> chr = nullptr;
  {
    absl::ReaderMutexLock lock(&services_mutex_);
    if (services_.count(characteristic.service_uuid) == 0) {
      return absl::NotFoundError(
          absl::Substitute("Service $0 doesn't exist",
                           std::string{characteristic.service_uuid}));
    }
    chr = services_[characteristic.service_uuid]->GetCharacteristic(
        characteristic.uuid);
  }
  if (chr == nullptr) {
    return absl::NotFoundError(
        absl::Substitute("characteristic $0 doesn't exist under service $1",
                         std::string{characteristic.uuid},
                         std::string{characteristic.service_uuid}));
  }

  return chr->NotifyChanged(confirm, new_value);
}

void GattServer::Stop() {
  bluez::GattManager manager(system_bus_, adapter_.GetObjectPath());
  absl::MutexLock lock(&services_mutex_);
  for (auto& [uuid, service] : services_) {
    LOG(INFO) << __func__ << ": Unregistering service "
                         << service->getObjectPath();
    try {
      manager.UnregisterApplication("/");
    } catch (const sdbus::Error& e) {
      DBUS_LOG_METHOD_CALL_ERROR(&manager, "UnregisterApplication", e);
    }
  }
  // services_.clear();
}

}  // namespace linux
}  // namespace nearby
