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

#include "internal/platform/implementation/linux/bluez_gatt_service_server.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/bluez_gatt_characteristic_server.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/gatt_characteristic_server.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace linux {
namespace bluez {
bool GattServiceServer::AddCharacteristic(
    const Uuid &service_uuid, const Uuid &characteristic_uuid,
    api::ble_v2::GattCharacteristic::Permission permission,
    api::ble_v2::GattCharacteristic::Property property) {
  absl::MutexLock lock(&characterstics_mutex_);
  api::ble_v2::GattCharacteristic characteristic{
      characteristic_uuid, service_uuid, permission, property};
  auto count = characteristics_.size();
  std::shared_ptr<GattCharacteristicServer> chr =
      std::make_shared<GattCharacteristicServer>(
          getObject().getConnection(), getObjectPath(), count, characteristic,
          server_cb_, devices_);
  try {
    chr->emitInterfacesAddedSignal(
        {org::bluez::GattCharacteristic1_adaptor::INTERFACE_NAME});
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": error emitting InterfacesAdded signal for object path "
        << chr->getObjectPath() << " with name '" << e.getName()
        << "' and message '" << e.getMessage() << "'";
    return false;
  }

  characteristics_.insert({characteristic_uuid, std::move(chr)});
  return true;
}

std::shared_ptr<GattCharacteristicServer> GattServiceServer::GetCharacteristic(
    const Uuid &uuid) {
  absl::ReaderMutexLock lock(&characterstics_mutex_);
  if (characteristics_.count(uuid) == 0) {
    return nullptr;
  }
  return characteristics_[uuid];
}
}  // namespace bluez
}  // namespace linux
}  // namespace nearby
