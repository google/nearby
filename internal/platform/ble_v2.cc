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

#include "internal/platform/ble_v2.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/uuid.h"

namespace nearby {

namespace {
using ::nearby::api::ble_v2::BleAdvertisementData;
using ::nearby::api::ble_v2::GattCharacteristic;
using ::nearby::api::ble_v2::TxPowerLevel;
using ReadValueCallback =
    ::nearby::api::ble_v2::ServerGattConnectionCallback::ReadValueCallback;
using WriteValueCallback =
    ::nearby::api::ble_v2::ServerGattConnectionCallback::WriteValueCallback;
}  // namespace

bool BleV2Medium::StartAdvertising(
    const BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_parameters) {
  return impl_->StartAdvertising(advertising_data, advertise_parameters);
}

bool BleV2Medium::StopAdvertising() { return impl_->StopAdvertising(); }

std::unique_ptr<api::ble_v2::BleMedium::AdvertisingSession>
BleV2Medium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_set_parameters,
    api::ble_v2::BleMedium::AdvertisingCallback callback) {
  return impl_->StartAdvertising(advertising_data, advertise_set_parameters,
                                 std::move(callback));
}

bool BleV2Medium::StartScanning(const Uuid& service_uuid,
                                TxPowerLevel tx_power_level,
                                ScanCallback callback) {
  MutexLock lock(&mutex_);
  if (scanning_enabled_) {
    LOG(INFO) << "Ble Scanning already enabled; impl=" << GetImpl();
    return false;
  }
  bool success = impl_->StartScanning(
      service_uuid, tx_power_level,
      api::ble_v2::BleMedium::ScanCallback{
          .advertisement_found_cb =
              [this](api::ble_v2::BlePeripheral& peripheral,
                     BleAdvertisementData advertisement_data) {
                MutexLock lock(&mutex_);
                if (!peripherals_.contains(&peripheral)) {
                  LOG(INFO) << "Peripheral impl=" << &peripheral
                            << " does not exist; add it to the map.";
                  peripherals_.insert(&peripheral);
                }

                BleV2Peripheral proxy(*this, peripheral.GetUniqueId());
                if (!scanning_enabled_) return;
                scan_callback_.advertisement_found_cb(std::move(proxy),
                                                      advertisement_data);
              },
      });
  if (success) {
    scan_callback_ = std::move(callback);
    // Clear the `peripherals_` after succeeded in StartScanning and before the
    // advertisement_found callback has been reached. This prevents deleting the
    // existing `peripherals_` if the scanning is not started successfully. If
    // scanning is started successfully, we need to clear `peripherals_` to
    // prevent the stale data in cache.
    peripherals_.clear();
    scanning_enabled_ = true;
    LOG(INFO) << "Ble Scanning enabled; impl=" << GetImpl();
  }
  return success;
}

bool BleV2Medium::StartMultipleServicesScanning(
    const std::vector<Uuid>& service_uuids,
    api::ble_v2::TxPowerLevel tx_power_level, ScanCallback callback) {
  MutexLock lock(&mutex_);
  if (scanning_enabled_) {
    LOG(INFO) << "Ble Scanning already enabled; impl=" << GetImpl();
    return false;
  }
  bool success = impl_->StartMultipleServicesScanning(
      service_uuids, tx_power_level,
      api::ble_v2::BleMedium::ScanCallback{
          .advertisement_found_cb =
              [this](api::ble_v2::BlePeripheral& peripheral,
                     BleAdvertisementData advertisement_data) {
                MutexLock lock(&mutex_);
                if (!peripherals_.contains(&peripheral)) {
                  LOG(INFO) << "Peripheral impl=" << &peripheral
                            << " does not exist; add it to the map.";
                  peripherals_.insert(&peripheral);
                }

                BleV2Peripheral proxy(*this, peripheral.GetUniqueId());
                if (!scanning_enabled_) return;
                scan_callback_.advertisement_found_cb(std::move(proxy),
                                                      advertisement_data);
              },
      });
  if (success) {
    scan_callback_ = std::move(callback);
    peripherals_.clear();
    scanning_enabled_ = true;
    LOG(INFO) << "Ble Scanning enabled; impl=" << GetImpl();
  }
  return success;
}

BleV2Medium::~BleV2Medium() { StopScanning(); }

bool BleV2Medium::StopScanning() {
  MutexLock lock(&mutex_);
  if (!scanning_enabled_) {
    return true;
  }
  scanning_enabled_ = false;
  peripherals_.clear();
  scan_callback_ = {};
  LOG(INFO) << "Ble Scanning disabled: impl=" << GetImpl();
  return impl_->StopScanning();
}
bool BleV2Medium::PauseMediumScanning() {
  MutexLock lock(&mutex_);
  if (!scanning_enabled_) {
    return true;
  }
  LOG(INFO) << "Pause Medium level BLE_V2 Scanning: impl=" << GetImpl();
  return impl_->PauseMediumScanning();
}

bool BleV2Medium::ResumeMediumScanning() {
  MutexLock lock(&mutex_);
  return impl_->ResumeMediumScanning();
}

std::unique_ptr<api::ble_v2::BleMedium::ScanningSession>
BleV2Medium::StartScanning(const Uuid& service_uuid,
                           api::ble_v2::TxPowerLevel tx_power_level,
                           api::ble_v2::BleMedium::ScanningCallback callback) {
  LOG(INFO) << "platform mutex: " << &mutex_;
  return impl_->StartScanning(
      service_uuid, tx_power_level,
      api::ble_v2::BleMedium::ScanningCallback{
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

std::unique_ptr<GattServer> BleV2Medium::StartGattServer(
    ServerGattConnectionCallback callback) {
  {
    MutexLock lock(&mutex_);
    server_gatt_connection_callback_ = std::move(callback);
  }

  std::unique_ptr<api::ble_v2::GattServer> api_gatt_server =
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
              [this](
                  const api::ble_v2::BlePeripheral::UniqueId remote_device_id,
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
              [this](
                  const api::ble_v2::BlePeripheral::UniqueId remote_device_id,
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

std::unique_ptr<GattClient> BleV2Medium::ConnectToGattServer(
    BleV2Peripheral peripheral, TxPowerLevel tx_power_level,
    ClientGattConnectionCallback callback) {
  std::optional<api::ble_v2::BlePeripheral::UniqueId> id =
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

BleV2ServerSocket BleV2Medium::OpenServerSocket(const std::string& service_id) {
  return BleV2ServerSocket(*this, impl_->OpenServerSocket(service_id));
}

BleL2capServerSocket BleV2Medium::OpenL2capServerSocket(
    const std::string& service_id) {
  return BleL2capServerSocket(*this, impl_->OpenL2capServerSocket(service_id));
}

BleV2Socket BleV2Medium::Connect(const std::string& service_id,
                                 TxPowerLevel tx_power_level,
                                 const BleV2Peripheral& peripheral,
                                 CancellationFlag* cancellation_flag) {
  BleV2Socket socket;
  api::ble_v2::BlePeripheral* device = peripheral.GetImpl();
  if (device != nullptr) {
    socket = BleV2Socket(
        peripheral,
        impl_->Connect(service_id, tx_power_level, *device, cancellation_flag));
  };
  return socket;
}

BleL2capSocket BleV2Medium::ConnectOverL2cap(
    const std::string& service_id, TxPowerLevel tx_power_level,
    const BleV2Peripheral& peripheral, CancellationFlag* cancellation_flag) {
  BleL2capSocket socket;
  api::ble_v2::BlePeripheral* device = peripheral.GetImpl();
  if (device != nullptr) {
    socket = BleL2capSocket(
        peripheral,
        impl_->ConnectOverL2cap(peripheral.GetPsm(), service_id, tx_power_level,
                                device->GetUniqueId(), cancellation_flag));
  };
  return socket;
}

bool BleV2Medium::IsExtendedAdvertisementsAvailable() {
  return IsValid() && impl_->IsExtendedAdvertisementsAvailable();
}

bool BleV2Peripheral::IsValid() const { return GetImpl() != nullptr; }

api::ble_v2::BlePeripheral* BleV2Peripheral::GetImpl() const {
  if (!unique_id_.has_value()) return nullptr;
  api::ble_v2::BlePeripheral* result = nullptr;
  if (!medium_->GetImpl()->GetRemotePeripheral(
          unique_id_.value(),
          [&](api::ble_v2::BlePeripheral& device) { result = &device; })) {
    return nullptr;
  }
  return result;
}

}  // namespace nearby
