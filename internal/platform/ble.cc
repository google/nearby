// Copyright 2022 Google LLC
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

#include "internal/platform/ble.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/uuid.h"

namespace nearby {

namespace {
using ::nearby::api::ble::BleAdvertisementData;
using ::nearby::api::ble::GattCharacteristic;
using ::nearby::api::ble::TxPowerLevel;
using ReadValueCallback =
    ::nearby::api::ble::ServerGattConnectionCallback::ReadValueCallback;
using WriteValueCallback =
    ::nearby::api::ble::ServerGattConnectionCallback::WriteValueCallback;
}  // namespace

bool BleMedium::StartAdvertising(
    const BleAdvertisementData& advertising_data,
    api::ble::AdvertiseParameters advertise_parameters) {
  return impl_->StartAdvertising(advertising_data, advertise_parameters);
}

bool BleMedium::StopAdvertising() { return impl_->StopAdvertising(); }

std::unique_ptr<api::ble::BleMedium::AdvertisingSession>
BleMedium::StartAdvertising(
    const api::ble::BleAdvertisementData& advertising_data,
    api::ble::AdvertiseParameters advertise_set_parameters,
    api::ble::BleMedium::AdvertisingCallback callback) {
  return impl_->StartAdvertising(advertising_data, advertise_set_parameters,
                                 std::move(callback));
}

bool BleMedium::StartScanning(const Uuid& service_uuid,
                              TxPowerLevel tx_power_level,
                              ScanCallback callback) {
  MutexLock lock(&mutex_);
  if (scanning_enabled_) {
    LOG(INFO) << "Ble Scanning already enabled";
    return false;
  }
  bool success = impl_->StartScanning(
      service_uuid, tx_power_level,
      api::ble::BleMedium::ScanCallback{
          .advertisement_found_cb =
              [this](api::ble::BlePeripheral::UniqueId peripheral_id,
                     BleAdvertisementData advertisement_data) {
                MutexLock lock(&mutex_);
                BlePeripheral proxy(*this, peripheral_id);
                if (!scanning_enabled_) return;
                scan_callback_.advertisement_found_cb(std::move(proxy),
                                                      advertisement_data);
              },
      });
  if (success) {
    scan_callback_ = std::move(callback);
    scanning_enabled_ = true;
    LOG(INFO) << "Ble Scanning enabled";
  }
  return success;
}

bool BleMedium::StartMultipleServicesScanning(
    const std::vector<Uuid>& service_uuids,
    api::ble::TxPowerLevel tx_power_level, ScanCallback callback) {
  MutexLock lock(&mutex_);
  if (scanning_enabled_) {
    LOG(INFO) << "Ble Scanning already enabled";
    return false;
  }
  bool success = impl_->StartMultipleServicesScanning(
      service_uuids, tx_power_level,
      api::ble::BleMedium::ScanCallback{
          .advertisement_found_cb =
              [this](api::ble::BlePeripheral::UniqueId peripheral_id,
                     BleAdvertisementData advertisement_data) {
                MutexLock lock(&mutex_);
                BlePeripheral proxy(*this, peripheral_id);
                if (!scanning_enabled_) return;
                scan_callback_.advertisement_found_cb(std::move(proxy),
                                                      advertisement_data);
              },
      });
  if (success) {
    scan_callback_ = std::move(callback);
    scanning_enabled_ = true;
    LOG(INFO) << "Ble Scanning enabled";
  }
  return success;
}

BleMedium::~BleMedium() { StopScanning(); }

bool BleMedium::StopScanning() {
  MutexLock lock(&mutex_);
  if (!scanning_enabled_) {
    return true;
  }
  scanning_enabled_ = false;
  scan_callback_ = {};
  LOG(INFO) << "Ble Scanning disabled";
  return impl_->StopScanning();
}
bool BleMedium::PauseMediumScanning() {
  MutexLock lock(&mutex_);
  if (!scanning_enabled_) {
    return true;
  }
  LOG(INFO) << "Pause Medium level BLE Scanning";
  return impl_->PauseMediumScanning();
}

bool BleMedium::ResumeMediumScanning() {
  MutexLock lock(&mutex_);
  return impl_->ResumeMediumScanning();
}

std::unique_ptr<api::ble::BleMedium::ScanningSession> BleMedium::StartScanning(
    const Uuid& service_uuid, api::ble::TxPowerLevel tx_power_level,
    api::ble::BleMedium::ScanningCallback callback) {
  LOG(INFO) << "platform mutex: " << &mutex_;
  return impl_->StartScanning(
      service_uuid, tx_power_level,
      api::ble::BleMedium::ScanningCallback{
          .start_scanning_result =
              [this, start_scanning_result =
                         std::move(callback.start_scanning_result)](
                  absl::Status status) mutable {
                {
                  MutexLock lock(&mutex_);
                  if (status.ok()) {
                    scanning_enabled_ = true;
                  }
                }
                start_scanning_result(status);
              },
          .advertisement_found_cb = std::move(callback.advertisement_found_cb),
          .advertisement_lost_cb = std::move(callback.advertisement_lost_cb),
      });
}

std::unique_ptr<GattServer> BleMedium::StartGattServer(
    ServerGattConnectionCallback callback) {
  {
    MutexLock lock(&mutex_);
    server_gatt_connection_callback_ = std::move(callback);
  }

  std::unique_ptr<api::ble::GattServer> api_gatt_server =
      impl_->StartGattServer({
          .characteristic_subscription_cb =
              [this](const GattCharacteristic& characteristic) {
                MutexLock lock(&mutex_);
                server_gatt_connection_callback_.characteristic_subscription_cb(
                    characteristic);
              },
          .characteristic_unsubscription_cb =
              [this](const GattCharacteristic& characteristic) {
                MutexLock lock(&mutex_);
                server_gatt_connection_callback_
                    .characteristic_unsubscription_cb(characteristic);
              },
          .on_characteristic_read_cb =
              [this](const api::ble::BlePeripheral::UniqueId remote_device_id,
                     const GattCharacteristic& characteristic, int offset,
                     ReadValueCallback callback) {
                MutexLock lock(&mutex_);
                if (server_gatt_connection_callback_
                        .on_characteristic_read_cb) {
                  server_gatt_connection_callback_.on_characteristic_read_cb(
                      remote_device_id, characteristic, offset,
                      std::move(callback));
                } else {
                  callback(absl::FailedPreconditionError(
                      "Read characteristic callback is not set"));
                }
              },
          .on_characteristic_write_cb =
              [this](const api::ble::BlePeripheral::UniqueId remote_device_id,
                     const GattCharacteristic& characteristic, int offset,
                     absl::string_view data, WriteValueCallback callback) {
                MutexLock lock(&mutex_);
                if (server_gatt_connection_callback_
                        .on_characteristic_write_cb) {
                  server_gatt_connection_callback_.on_characteristic_write_cb(
                      remote_device_id, characteristic, offset, data,
                      std::move(callback));
                } else {
                  callback(absl::FailedPreconditionError(
                      "Write characteristic callback is not set"));
                }
              },
      });
  return std::make_unique<GattServer>(std::move(api_gatt_server));
}

std::unique_ptr<GattClient> BleMedium::ConnectToGattServer(
    BlePeripheral peripheral, TxPowerLevel tx_power_level,
    ClientGattConnectionCallback callback) {
  std::optional<api::ble::BlePeripheral::UniqueId> id =
      peripheral.GetUniqueId();
  if (!id.has_value()) {
    LOG(ERROR) << "Failed to connect to GattServer, invalid peripheral";
    return nullptr;
  }
  return std::make_unique<GattClient>(impl_->ConnectToGattServer(
      *id, tx_power_level,
      {
          .disconnected_cb =
              [callback = std::move(callback)]() mutable {
                callback.disconnected_cb();
              },
      }));
}

BleServerSocket BleMedium::OpenServerSocket(const std::string& service_id) {
  return BleServerSocket(*this, impl_->OpenServerSocket(service_id));
}

BleL2capServerSocket BleMedium::OpenL2capServerSocket(
    const std::string& service_id) {
  return BleL2capServerSocket(*this, impl_->OpenL2capServerSocket(service_id));
}

BleSocket BleMedium::Connect(const std::string& service_id,
                             TxPowerLevel tx_power_level,
                             const BlePeripheral& peripheral,
                             CancellationFlag* cancellation_flag) {
  std::optional<api::ble::BlePeripheral::UniqueId> id =
      peripheral.GetUniqueId();
  if (!id.has_value()) {
    LOG(ERROR) << "Failed to connect, invalid peripheral";
    return {};
  }
  return BleSocket(peripheral, impl_->Connect(service_id, tx_power_level, *id,
                                              cancellation_flag));
}

BleL2capSocket BleMedium::ConnectOverL2cap(
    const std::string& service_id, TxPowerLevel tx_power_level,
    const BlePeripheral& peripheral, CancellationFlag* cancellation_flag) {
  std::optional<api::ble::BlePeripheral::UniqueId> id =
      peripheral.GetUniqueId();
  if (!id.has_value()) {
    LOG(ERROR) << "Failed to connect over L2cap, invalid peripheral";
    return {};
  }
  return BleL2capSocket(
      peripheral,
      impl_->ConnectOverL2cap(peripheral.GetPsm(), service_id, tx_power_level,
                              *id, cancellation_flag));
}

bool BleMedium::IsExtendedAdvertisementsAvailable() {
  return IsValid() && impl_->IsExtendedAdvertisementsAvailable();
}

bool BlePeripheral::IsValid() const { return unique_id_.has_value(); }

std::optional<BlePeripheral> BleMedium::RetrieveBlePeripheralFromNativeId(
    const std::string& ble_peripheral_native_id) {
  if (!IsValid()) {
    return std::nullopt;
  }

  std::optional<api::ble::BlePeripheral::UniqueId> id =
      impl_->RetrieveBlePeripheralIdFromNativeId(ble_peripheral_native_id);
  if (id.has_value()) {
    return BlePeripheral(*this, id.value());
  }

  return std::nullopt;
}

}  // namespace nearby
