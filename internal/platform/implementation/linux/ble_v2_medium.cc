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
#include <cassert>

#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/ble_gatt_client.h"
#include "internal/platform/implementation/linux/ble_gatt_server.h"
#include "internal/platform/implementation/linux/ble_v2_medium.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_devices.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/bluez_advertisement_monitor.h"
#include "internal/platform/implementation/linux/bluez_advertisement_monitor_manager.h"
#include "internal/platform/implementation/linux/bluez_le_advertisement.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/advertisement_monitor_server.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/le_advertisement_manager_client.h"

namespace nearby {
namespace linux {
BleV2Medium::BleV2Medium(BluetoothAdapter &adapter)
    : system_bus_(adapter.GetConnection()),
      adapter_(adapter),
      devices_(std::make_unique<BluetoothDevices>(
          system_bus_, adapter_.GetObjectPath(), observers_)),
      gatt_discovery_(std::make_shared<BluezGattDiscovery>(system_bus_)),
      root_object_manager_(std::make_unique<RootObjectManager>(*system_bus_)),
      adv_monitor_manager_(
          bluez::AdvertisementMonitorManager::
              DiscoverAdvertisementMonitorManager(*system_bus_, adapter_)),
      adv_manager_(std::make_unique<bluez::LEAdvertisementManager>(*system_bus_,
                                                                   adapter)),
      cur_adv_(nullptr) {
  if (adv_monitor_manager_) {
    LOG(INFO)
        << __func__
        << ": Registering path / with AdvertisementMonitorManager at "
        << adv_monitor_manager_->getObjectPath();
    try {
      adv_monitor_manager_->RegisterMonitor("/");
    } catch (const sdbus::Error &e) {
      DBUS_LOG_METHOD_CALL_ERROR(adv_monitor_manager_, "RegisterMonitor", e);
    }
  }
  if (gatt_discovery_->InitializeKnownServices()) {
    LOG(ERROR) << __func__
                       << ": Could not initialize known GATT services";
  }
}

bool BleV2Medium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData &advertising_data,
    api::ble_v2::AdvertiseParameters advertise_set_parameters) {
  if (!adapter_.IsEnabled()) {
    LOG(WARNING) << "BLE cannot start advertising because the "
                            "bluetooth adapter is not enabled.";
    return false;
  }

  if (advertising_data.service_data.empty()) {
    LOG(WARNING)
        << "BLE cannot start to advertise due to invalid service data.";
    return false;
  }

  absl::MutexLock lock(&cur_adv_mutex_);
  if (cur_adv_ != nullptr) {
    LOG(ERROR) << __func__
                       << "Advertising is already enabled for this medium.";
    return false;
  }

  cur_adv_ = bluez::LEAdvertisement::CreateLEAdvertisement(
      *system_bus_, advertising_data, advertise_set_parameters);

  LOG(INFO) << __func__ << ": Registering advertisement "
                    << cur_adv_->getObjectPath() << " on bluetooth adapter "
                    << adapter_.GetObjectPath();

  try {
    adv_manager_->RegisterAdvertisement(cur_adv_->getObjectPath(), {});
  } catch (const sdbus::Error &e) {
    cur_adv_ = nullptr;
    DBUS_LOG_METHOD_CALL_ERROR(adv_manager_, "RegisterAdvertisement", e);
    return false;
  }

  return true;
}

bool BleV2Medium::StopAdvertising() {
  absl::MutexLock lock(&cur_adv_mutex_);
  if (cur_adv_ == nullptr) {
    LOG(ERROR) << __func__ << ": Advertising is not enabled.";
    return false;
  }
  LOG(INFO) << __func__ << "Unregistering advertisement object "
                       << cur_adv_->getObjectPath();

  try {
    adv_manager_->UnregisterAdvertisement(cur_adv_->getObjectPath());
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(adv_manager_, "UnregisterAdvertisement", e);
    return false;
  }

  cur_adv_ = nullptr;
  return true;
}

std::unique_ptr<api::ble_v2::BleMedium::AdvertisingSession>
BleV2Medium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData &advertising_data,
    api::ble_v2::AdvertiseParameters advertise_set_parameters,
    AdvertisingCallback callback) {
  if (!adapter_.IsEnabled()) {
    LOG(WARNING) << ": BLE cannot start advertising because the "
                            "bluetooth adapter is not enabled.";
    return nullptr;
  }

  if (advertising_data.service_data.empty()) {
    LOG(WARNING)
        << ": BLE cannot start to advertise due to invalid service data.";
    return nullptr;
  }

  std::shared_ptr<sdbus::IProxy> proxy =
      sdbus::createProxy(*system_bus_, "org.bluez", adapter_.GetObjectPath());
  proxy->finishRegistration();

  std::shared_ptr<AdvertisingCallback> shared_cb =
      std::make_shared<AdvertisingCallback>(std::move(callback));

  absl::MutexLock lock(&advs_mutex_);
  advs_.push_front(bluez::LEAdvertisement::CreateLEAdvertisement(
      *system_bus_, advertising_data, advertise_set_parameters));
  auto adv_it = advs_.begin();

  auto pending_call =
      proxy->callMethodAsync("RegisterAdvertisement")
          .onInterface(org::bluez::LEAdvertisingManager1_proxy::INTERFACE_NAME)
          .withArguments((*adv_it)->getObjectPath(),
                         std::map<std::string, sdbus::Variant>{})
          .uponReplyInvoke(
              [this, proxy, shared_cb, adv_it](const sdbus::Error *error) {
                if (error != nullptr && error->isValid()) {
                  {
                    absl::MutexLock lock(&advs_mutex_);
                    advs_.erase(adv_it);
                  }
                  DBUS_LOG_METHOD_CALL_ERROR(adv_manager_,
                                             "RegisterAdvertisement", *error);
                  auto name = error->getName();
                  std::string msg = error->getMessage();
                  absl::Status status;

                  if (name == "org.bluez.Error.InvalidArguments" ||
                      name == "org.bluez.Error.InvalidLength") {
                    status = absl::InvalidArgumentError(msg);
                  } else if (name == "org.bluez.Error.AlreadyExists") {
                    status = absl::AlreadyExistsError(msg);
                  } else if (name == "org.bluez.Error.NotPermitted") {
                    status = absl::ResourceExhaustedError(msg);
                  } else {
                    status = absl::UnknownError(msg);
                  }
                  shared_cb->start_advertising_result(std::move(status));
                } else {
                  shared_cb->start_advertising_result(absl::OkStatus());
                }
              });

  absl::AnyInvocable<absl::Status()> stop_adv = [&, adv_it]() {
    LOG(INFO) << __func__ << ": Unregistering advertisement object "
                         << (*adv_it)->getObjectPath();
    absl::MutexLock lock(&advs_mutex_);
    try {
      adv_manager_->UnregisterAdvertisement((*adv_it)->getObjectPath());
    } catch (const sdbus::Error &e) {
      DBUS_LOG_METHOD_CALL_ERROR(adv_manager_, "UnregisterAdvertisement", e);
      return absl::UnknownError(e.getMessage());
    }
    advs_.erase(adv_it);
    return absl::OkStatus();
  };

  return std::make_unique<api::ble_v2::BleMedium::AdvertisingSession>(
      api::ble_v2::BleMedium::AdvertisingSession{std::move(stop_adv)});
}

std::unique_ptr<api::ble_v2::GattServer> BleV2Medium::StartGattServer(
    api::ble_v2::ServerGattConnectionCallback callback) {
  return std::make_unique<GattServer>(*system_bus_, adapter_, devices_,
                                      std::move(callback));
}

std::unique_ptr<api::ble_v2::GattClient> BleV2Medium::ConnectToGattServer(
    api::ble_v2::BlePeripheral &peripheral,
    api::ble_v2::TxPowerLevel tx_power_level,
    api::ble_v2::ClientGattConnectionCallback callback) {
  auto path = bluez::device_object_path(adapter_.GetObjectPath(),
                                        peripheral.GetAddress());

  return std::make_unique<GattClient>(system_bus_, path, gatt_discovery_,
                                      std::move(callback.disconnected_cb));
}

bool BleV2Medium::IsExtendedAdvertisementsAvailable() {
  try {
    auto supported_channels = adv_manager_->SupportedSecondaryChannels();
    return !supported_channels.empty();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(adv_manager_, "SupportedSecondaryChannels", e);
    return false;
  }
}

bool BleV2Medium::StartLEDiscovery() {
  std::map<std::string, sdbus::Variant> filter;
  filter["Transport"] = "auto";
  auto &adapter = adapter_.GetBluezAdapterObject();

  try {
    adapter.SetDiscoveryFilter(filter);
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(&adapter, "SetDiscoveryFilter", e);
    return false;
  }

  try {
    LOG(INFO) << __func__ << ": Starting LE discovery on "
                      << adapter.getObjectPath();
    adapter.StartDiscovery();
  } catch (const sdbus::Error &e) {
    if (e.getName() != "org.bluez.Error.InProgress") {
      DBUS_LOG_METHOD_CALL_ERROR(&adapter, "StartDiscovery", e);
      return false;
    }
  }

  return true;
}

bool BleV2Medium::StartScanning(const Uuid &service_uuid,
                                api::ble_v2::TxPowerLevel tx_power_level,
                                ScanCallback callback) {
  if (cur_monitored_service_uuid_.has_value()) {
    LOG(ERROR) << __func__
                       << ": A sync scanning session is already active for "
                       << std::string{*cur_monitored_service_uuid_};
    return false;
  }

  if (adv_monitor_manager_ == nullptr) {
    LOG(WARNING) << __func__
                         << ": Advertising monitor not supported by BlueZ";
    // TODO: Implement manual monitoring.
    return false;
  }

  if (!MonitorManagerSupportsOr()) {
    LOG(WARNING)
        << __func__
        << ": \"or_patterns\" not supported by AdvertisementMonitorManager";
    // TODO: Implement manual monitoring.
    return false;
  }

  absl::MutexLock lock(&active_adv_monitors_mutex_);
  if (active_adv_monitors_.count(service_uuid) == 1) {
    LOG(ERROR) << __func__ << ": an advertising session for service "
                       << std::string{service_uuid} << " already exists";
    return false;
  }

  auto monitor = std::make_unique<bluez::AdvertisementMonitor>(
      *system_bus_, service_uuid, tx_power_level, "or_patterns", devices_,
      std::move(callback));
  try {
    monitor->emitInterfacesAddedSignal(
        {org::bluez::AdvertisementMonitor1_adaptor::INTERFACE_NAME});
  } catch (const sdbus::Error &e) {
    LOG(ERROR)
        << __func__
        << ": error emitting InterfacesAdded signal for object path "
        << monitor->getObjectPath() << " with name '" << e.getName()
        << "' and message '" << e.getMessage() << "'";
    return false;
  }

  auto device_watcher = std::make_unique<DeviceWatcher>(
      *system_bus_, adapter_.GetObjectPath(), devices_);
  if (!StartLEDiscovery()) {
    LOG(ERROR) << __func__
                       << ": Could not start LE discovery on adapter "
                       << adapter_.GetObjectPath();
    device_watcher = nullptr;
    try {
      monitor->emitInterfacesRemovedSignal(
          {org::bluez::AdvertisementMonitor1_adaptor::INTERFACE_NAME});
    } catch (const sdbus::Error &e) {
      LOG(ERROR)
          << __func__
          << ": error emitting InterfacesRemoved signal for object path "
          << monitor->getObjectPath() << " with name '" << e.getName()
          << "' and message '" << e.getMessage() << "'";
    }
    return false;
  }

  active_adv_monitors_[service_uuid] =
      std::make_pair(std::move(monitor), std::move(device_watcher));
  cur_monitored_service_uuid_ = service_uuid;
  return true;
}

bool BleV2Medium::StopScanning() {
  if (!cur_monitored_service_uuid_.has_value()) {
    LOG(ERROR) << __func__
                       << ": No sync scanning session is currently active.";
    return false;
  }

  if (adv_monitor_manager_ == nullptr) {
    // TODO: Implement manual monitoring.
    return false;
  }

  auto &adapter = adapter_.GetBluezAdapterObject();
  LOG(INFO) << __func__ << ": Stopping discovery for adapter "
                       << adapter.getObjectPath();
  try {
    adapter.StopDiscovery();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(&adapter, "StopDiscovery", e);
  }

  absl::MutexLock lock(&active_adv_monitors_mutex_);
  auto monitor_it = active_adv_monitors_.find(*cur_monitored_service_uuid_);
  assert(monitor_it != active_adv_monitors_.end());
  {
    auto &[_uuid, session] = *monitor_it;
    auto &[adv_monitor, _watcher] = session;

    LOG(INFO) << __func__ << ": Removing advertising monitor "
                         << adv_monitor->getObjectPath();
    adv_monitor->emitInterfacesRemovedSignal(
        {org::bluez::AdvertisementMonitor1_adaptor::INTERFACE_NAME});
  }
  active_adv_monitors_.erase(monitor_it);
  cur_monitored_service_uuid_ = std::nullopt;

  return true;
}

std::unique_ptr<api::ble_v2::BleMedium::ScanningSession>
BleV2Medium::StartScanning(const Uuid &service_uuid,
                           api::ble_v2::TxPowerLevel tx_power_level,
                           ScanningCallback callback) {
  if (adv_monitor_manager_ == nullptr) {
    // TODO: Implement manual monitoring.
    return nullptr;
  }

  absl::MutexLock lock(&active_adv_monitors_mutex_);
  if (active_adv_monitors_.count(service_uuid) == 1) {
    LOG(ERROR) << __func__ << ": Service " << std::string{service_uuid}
                       << " is already being advertised";
    return nullptr;
  }

  auto monitor = std::make_unique<bluez::AdvertisementMonitor>(
      *system_bus_, service_uuid, tx_power_level, "or_patterns", devices_,
      std::move(callback));
  try {
    monitor->emitInterfacesAddedSignal(
        {org::bluez::AdvertisementMonitor1_adaptor::INTERFACE_NAME});
  } catch (const sdbus::Error &e) {
    LOG(ERROR)
        << __func__
        << ": error emitting InterfacesAdded signal for object path "
        << monitor->getObjectPath() << " with name '" << e.getName()
        << "' and message '" << e.getMessage() << "'";
    return nullptr;
  }

  auto device_watcher = std::make_unique<DeviceWatcher>(
      *system_bus_, adapter_.GetObjectPath(), devices_);
  if (!StartLEDiscovery()) {
    LOG(ERROR) << __func__
                       << ": Could not start LE discovery on adapter "
                       << adapter_.GetObjectPath();
    try {
      monitor->emitInterfacesRemovedSignal(
          {org::bluez::AdvertisementMonitor1_adaptor::INTERFACE_NAME});
    } catch (const sdbus::Error &e) {
      LOG(ERROR)
          << __func__
          << ": error emitting InterfacesRemoved signal for object path "
          << monitor->getObjectPath() << " with name '" << e.getName()
          << "' and message '" << e.getMessage() << "'";
    }
    return nullptr;
  }

  active_adv_monitors_[service_uuid] =
      std::make_pair(std::move(monitor), std::move(device_watcher));

  return std::make_unique<ScanningSession>(
      ScanningSession{.stop_scanning = [this, service_uuid]() {
        absl::MutexLock lock(&active_adv_monitors_mutex_);
        if (active_adv_monitors_.count(service_uuid) == 0) {
          LOG(ERROR)
              << __func__ << ": Advertising monitor for service "
              << std::string{service_uuid} << " does not exist anymore";
          return absl::NotFoundError(
              "Advertising monitor for this service does not exist");
        }

        auto &[monitor, watcher] = active_adv_monitors_[service_uuid];
        try {
          monitor->emitInterfacesRemovedSignal(
              {org::bluez::AdvertisementMonitor1_adaptor::INTERFACE_NAME});
        } catch (const sdbus::Error &e) {
          LOG(ERROR)
              << __func__
              << ": error emitting InterfacesRemoved signal for object path "
              << monitor->getObjectPath() << " with name '" << e.getName()
              << "' and message '" << e.getMessage() << "'";
        }

        auto &adapter = adapter_.GetBluezAdapterObject();
        absl::Status status;
        try {
          adapter.StopDiscovery();
          status = absl::OkStatus();
        } catch (const sdbus::Error &e) {
          DBUS_LOG_METHOD_CALL_ERROR(&adapter, "StopDiscovery", e);
          status = absl::InternalError(e.getMessage());
        }
        active_adv_monitors_.erase(service_uuid);
        return status;
      }});
}

bool BleV2Medium::GetRemotePeripheral(const std::string &mac_address,
                                      GetRemotePeripheralCallback callback) {
  auto device = devices_->get_device_by_address(mac_address);
  if (device == nullptr) return false;
  callback(*device);
  return true;
}

bool BleV2Medium::GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId id,
                                      GetRemotePeripheralCallback callback) {
  auto device = devices_->get_device_by_unique_id(id);
  if (device == nullptr) return false;
  callback(*device);
  return true;
}
}  // namespace linux
}  // namespace nearby
