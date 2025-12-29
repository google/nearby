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

#include <algorithm>
#include <map>
#include <string>
#include <variant>

#include <sdbus-c++/Types.h>

#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/ble_gatt_client.h"
#include "internal/platform/implementation/linux/bluez_gatt_characteristic_client.h"
#include "internal/platform/implementation/linux/bluez_gatt_service_client.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/gatt_characteristic_client.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/gatt_service_client.h"
#include "internal/platform/implementation/linux/utils.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
bool GattClient::DiscoverServiceAndCharacteristics(
    const Uuid &service_uuid, const std::vector<Uuid> &characteristic_uuids) {
  return gatt_discovery_->DiscoverServiceAndCharacteristics(
      peripheral_object_path_, service_uuid, characteristic_uuids,
      discovery_cancel_);
}

absl::optional<api::ble_v2::GattCharacteristic> GattClient::GetCharacteristic(
    const Uuid &service_uuid, const Uuid &characteristic_uuid) {
  auto chr_proxy = gatt_discovery_->GetCharacteristic(
      peripheral_object_path_, service_uuid, characteristic_uuid);
  if (chr_proxy == nullptr) return std::nullopt;

  api::ble_v2::GattCharacteristic chr;
  chr.service_uuid = service_uuid;
  chr.uuid = characteristic_uuid;
  chr.property = api::ble_v2::GattCharacteristic::Property::kNone;
  chr.permission = api::ble_v2::GattCharacteristic::Permission::kNone;

  std::vector<std::string> flags;
  try {
    flags = chr_proxy->Flags();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(chr_proxy, "Flags", e);
    return std::nullopt;
  }

  for (const auto &flag : flags) {
    if (flag == "read") {
      chr.property |= api::ble_v2::GattCharacteristic::Property::kRead;
      chr.permission |= api::ble_v2::GattCharacteristic::Permission::kRead;
    } else if (flag == "write") {
      chr.property |= api::ble_v2::GattCharacteristic::Property::kWrite;
      chr.permission |= api::ble_v2::GattCharacteristic::Permission::kWrite;
    } else if (flag == "notify") {
      chr.property |= api::ble_v2::GattCharacteristic::Property::kNotify;
    } else if (flag == "indicate") {
      chr.property |= api::ble_v2::GattCharacteristic::Property::kIndicate;
    }
  }

  absl::MutexLock lock(&characteristics_mutex_);
  characteristics_.emplace(chr, std::move(chr_proxy));

  return chr;
}

absl::optional<std::string> GattClient::ReadCharacteristic(
    const api::ble_v2::GattCharacteristic &characteristic) {
  absl::ReaderMutexLock lock(&characteristics_mutex_);
  if (characteristics_.count(characteristic) == 0) {
    LOG(ERROR) << __func__ << ": Unknown characteristic '"
                       << absl::Substitute("$0", characteristic) << "'";
    return std::nullopt;
  }

  return std::visit(
      [](auto &&chr) {
        try {
          auto value_bytes = chr->ReadValue({});
          return std::optional<std::string>{
              std::string(value_bytes.begin(), value_bytes.end())};
        } catch (const sdbus::Error &e) {
          DBUS_LOG_METHOD_CALL_ERROR(chr, "ReadValue", e);
          return std::optional<std::string>();
        }
      },
      characteristics_[characteristic]);
}

bool GattClient::WriteCharacteristic(
    const api::ble_v2::GattCharacteristic &characteristic,
    absl::string_view value, WriteType type) {
  absl::ReaderMutexLock lock(&characteristics_mutex_);
  if (characteristics_.count(characteristic) == 0) {
    LOG(ERROR) << __func__ << ": Unknown characteristic '"
                       << absl::Substitute("$0", characteristic) << "'";
    return false;
  }

  return std::visit(
      [value, type](auto &&chr) {
        std::vector<uint8_t> value_bytes(value.begin(), value.end());
        try {
          chr->WriteValue(
              value_bytes,
              {{"type",
                type == api::ble_v2::GattClient::WriteType::kWithResponse
                    ? "request"
                    : "command"}});
          return true;
        } catch (const sdbus::Error &e) {
          DBUS_LOG_METHOD_CALL_ERROR(chr, "WriteValue", e);
          return false;
        }
      },
      characteristics_[characteristic]);
}

bool GattClient::SetCharacteristicSubscription(
    const api::ble_v2::GattCharacteristic &characteristic, bool enable,
    absl::AnyInvocable<void(absl::string_view value)>
        on_characteristic_changed_cb) {
  absl::MutexLock lock(&characteristics_mutex_);
  if (characteristics_.count(characteristic) == 0) {
    LOG(ERROR) << __func__ << ": Unknown characteristic '"
                       << absl::Substitute("$0", characteristic) << "'";
    return false;
  }

  if (enable) {
    auto subbed_chr = gatt_discovery_->GetSubscribedCharacteristic(
        peripheral_object_path_, characteristic.service_uuid,
        characteristic.uuid, std::move(on_characteristic_changed_cb));
    if (subbed_chr == nullptr) return false;
    try {
      subbed_chr->StartNotify();
    } catch (const sdbus::Error &e) {
      DBUS_LOG_METHOD_CALL_ERROR(subbed_chr, "StartNotify", e);
      return false;
    }
    characteristics_[characteristic] = std::move(subbed_chr);
  } else if (std::holds_alternative<
                 std::unique_ptr<bluez::SubscribedGattCharacteristicClient>>(
                 characteristics_[characteristic])) {
    auto chr = gatt_discovery_->GetCharacteristic(peripheral_object_path_,
                                                  characteristic.service_uuid,
                                                  characteristic.uuid);
    if (chr == nullptr) return false;
    try {
      chr->StopNotify();
    } catch (const sdbus::Error &e) {
      DBUS_LOG_METHOD_CALL_ERROR(chr, "StopNotify", e);
      return false;
    }

    characteristics_[characteristic] = std::move(chr);
  }
  return true;
}

void GattClient::Disconnect() {
  absl::MutexLock lock(&disconnected_callback_mutex_);
  if (!discovery_cancel_.Cancelled()) {
    discovery_cancel_.Cancel();
    if (*disconnected_callback_it_ != nullptr) (*disconnected_callback_it_)();
    gatt_discovery_->RemovePeripheralConnection(peripheral_object_path_,
                                                disconnected_callback_it_);
  }
}

void BluezGattDiscovery::Shutdown() {
  auto no_discovery = [&]() {
    mutex_.AssertReaderHeld();
    return pending_discovery_ == 0;
  };

  mutex_.Lock();
  shutdown_ = true;
  mutex_.Await(absl::Condition(&no_discovery));
  mutex_.Unlock();
}

bool BluezGattDiscovery::InitializeKnownServices() {
  std::map<sdbus::ObjectPath,
           std::map<std::string, std::map<std::string, sdbus::Variant>>>
      objects;
  try {
    objects = GetManagedObjects();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(this, "GetManagedObjects", e);
    return false;
  }

  absl::flat_hash_map<sdbus::ObjectPath, GattServiceClient> cached_services;
  absl::MutexLock lock(&mutex_);
  auto chr_it = std::find_if(
      objects.cbegin(), objects.cend(),
      [](std::pair<sdbus::ObjectPath,
                   std::map<std::string, std::map<std::string, sdbus::Variant>>>
             object) {
        return object.second.count(
                   org::bluez::GattCharacteristic1_proxy::INTERFACE_NAME) == 1;
      });

  for (; chr_it != objects.cend(); chr_it++) {
    const auto &[path, ifaces] = *chr_it;
    const auto &properties =
        ifaces.at(org::bluez::GattCharacteristic1_proxy::INTERFACE_NAME);
    auto maybe_props = characteristicProperties(path, properties);
    if (!maybe_props.has_value()) continue;
    auto [chr_uuid, service_uuid, device_path] = *maybe_props;

    discovered_characteristics_.emplace(
        std::make_tuple(chr_uuid, service_uuid, device_path), path);
    characteristics_properties_.emplace(
        path, std::make_tuple(chr_uuid, service_uuid, device_path));
  }

  return true;
}

BluezGattDiscovery::CallbackIter BluezGattDiscovery::AddPeripheralConnection(
    const sdbus::ObjectPath &device_object_path,
    absl::AnyInvocable<void()> disconnected_callback_) {
  absl::MutexLock lock(&peripheral_disconnected_callbacks_mutex_);
  if (peripheral_disconnected_callbacks_.count(device_object_path) == 0)
    peripheral_disconnected_callbacks_.emplace(
        device_object_path, std::list<absl::AnyInvocable<void()>>{});
  auto &list = peripheral_disconnected_callbacks_[device_object_path];
  list.push_back(std::move(disconnected_callback_));
  return list.begin();
}

void BluezGattDiscovery::RemovePeripheralConnection(
    const sdbus::ObjectPath &device_object_path,
    BluezGattDiscovery::CallbackIter cb) {
  absl::MutexLock lock(&peripheral_disconnected_callbacks_mutex_);
  auto it = peripheral_disconnected_callbacks_.find(device_object_path);
  if (it != peripheral_disconnected_callbacks_.end()) {
    it->second.erase(cb);
    if (it->second.empty())
      peripheral_disconnected_callbacks_.erase(device_object_path);
  }
}

bool BluezGattDiscovery::DiscoverServiceAndCharacteristics(
    const sdbus::ObjectPath &device_object_path, const Uuid &service_uuid,
    const std::vector<Uuid> &characteristic_uuids, CancellationFlag &cancel) {
  CancellationFlagListener cancel_listen(&cancel, [&]() {
    mutex_.Lock();
    mutex_.Unlock();
  });

  auto discovered = [this, device_object_path, service_uuid,
                     characteristic_uuids, &cancel]() {
    mutex_.AssertReaderHeld();
    return cancel.Cancelled() ||
           std::all_of(
               characteristic_uuids.cbegin(), characteristic_uuids.cend(),
               [this, service_uuid, device_object_path](auto &chr_uuid) {
                 mutex_.AssertReaderHeld();
                 return discovered_characteristics_.count(
                            {service_uuid, chr_uuid, device_object_path}) == 1;
               });
  };

  absl::ReaderMutexLock lock(&mutex_, absl::Condition(&discovered));
  return !cancel.Cancelled();
}

std::unique_ptr<bluez::GattCharacteristicClient>
BluezGattDiscovery::GetCharacteristic(
    const sdbus::ObjectPath &device_object_path, const Uuid &service_uuid,
    const Uuid &characteristic_uuid) {
  auto key =
      std::make_tuple(service_uuid, characteristic_uuid, device_object_path);

  absl::ReaderMutexLock lock(&mutex_);
  auto path_it = discovered_characteristics_.find(key);
  if (path_it == discovered_characteristics_.end()) {
    LOG(ERROR) << __func__ << ": No characteristic known for device "
                       << device_object_path << " with service "
                       << std::string{service_uuid} << " and UUID "
                       << std::string{characteristic_uuid};
    return nullptr;
  }

  return std::make_unique<bluez::GattCharacteristicClient>(system_bus_,
                                                           path_it->second);
}

std::unique_ptr<bluez::GattCharacteristicClient>
BluezGattDiscovery::GetSubscribedCharacteristic(
    const sdbus::ObjectPath &device_object_path, const Uuid &service_uuid,
    const Uuid &characteristic_uuid,
    absl::AnyInvocable<void(absl::string_view value)>
        on_characteristic_changed_cb) {
  auto key =
      std::make_tuple(service_uuid, characteristic_uuid, device_object_path);

  absl::ReaderMutexLock lock(&mutex_);
  auto path_it = discovered_characteristics_.find(key);
  if (path_it == discovered_characteristics_.end()) {
    LOG(ERROR) << __func__ << ": No characteristic known for device "
                       << device_object_path << " with service "
                       << std::string{service_uuid} << " and UUID "
                       << std::string{characteristic_uuid};
    return nullptr;
  }

  return std::make_unique<bluez::SubscribedGattCharacteristicClient>(
      system_bus_, device_object_path, std::move(on_characteristic_changed_cb));
}

std::optional<std::tuple<Uuid, Uuid, sdbus::ObjectPath>>
BluezGattDiscovery::characteristicProperties(
    const sdbus::ObjectPath &path,
    const std::map<std::string, sdbus::Variant> &properties) {
  mutex_.AssertHeld();

  const std::string &chr_uuid_str = properties.at("UUID");
  auto chr_uuid = UuidFromString(chr_uuid_str);
  if (!chr_uuid.has_value()) {
    LOG(ERROR) << ": Couldn't parse UUID '" << chr_uuid_str
                       << "' in characteristic " << path;
    return std::nullopt;
  }

  const sdbus::ObjectPath &service_path = properties.at("Service");
  if (cached_services_.count(service_path) == 0) {
    cached_services_.emplace(
        path, std::make_unique<GattServiceClient>(system_bus_, path));
  }

  auto &service = cached_services_.at(service_path);
  nearby::Uuid service_uuid;
  try {
    const std::string &service_uuid_str = service->UUID();
    auto service_uuid_maybe = UuidFromString(service_uuid_str);
    if (!service_uuid_maybe.has_value()) {
      LOG(ERROR) << ": Couldn't parse UUID '" << service_uuid_str
                         << "' in service " << service_path;
      return std::nullopt;
    }
    service_uuid = *service_uuid_maybe;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(service, "UUID", e);
    return std::nullopt;
  }

  sdbus::ObjectPath device_path;
  try {
    device_path = service->Device();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(service, "Device", e);
    return std::nullopt;
  }

  return std::make_tuple(*chr_uuid, service_uuid, device_path);
}

void BluezGattDiscovery::onInterfacesAdded(
    const sdbus::ObjectPath &objectPath,
    const std::map<std::string, std::map<std::string, sdbus::Variant>>
        &interfacesAndProperties) {
  if (interfacesAndProperties.count(
          org::bluez::GattCharacteristic1_proxy::INTERFACE_NAME) == 0)
    return;

  const auto &properties = interfacesAndProperties.at(
      org::bluez::GattCharacteristic1_proxy::INTERFACE_NAME);

  absl::MutexLock lock(&mutex_);
  auto maybe_props = characteristicProperties(objectPath, properties);
  if (!maybe_props.has_value()) return;
  auto [chr_uuid, service_uuid, device_path] = *maybe_props;

  discovered_characteristics_.emplace(
      std::make_tuple(chr_uuid, service_uuid, device_path), objectPath);
  characteristics_properties_.emplace(
      objectPath, std::make_tuple(chr_uuid, service_uuid, device_path));
}

void BluezGattDiscovery::onInterfacesRemoved(
    const sdbus::ObjectPath &objectPath,
    const std::vector<std::string> &interfaces) {
  auto begin = interfaces.cbegin();
  auto end = interfaces.cend();

  auto service_it =
      std::find(begin, end, org::bluez::GattService1_proxy::INTERFACE_NAME);
  if (service_it != end) {
    absl::MutexLock lock(&mutex_);
    cached_services_.erase(objectPath);
    return;
  }

  auto chr_it = std::find(
      begin, end, org::bluez::GattCharacteristic1_proxy::INTERFACE_NAME);
  if (chr_it != end) {
    absl::MutexLock lock(&mutex_);
    {
      auto &props = characteristics_properties_.at(objectPath);
      discovered_characteristics_.erase(props);
    }
    characteristics_properties_.erase(objectPath);
  }
}

}  // namespace linux
}  // namespace nearby
