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
#include <string>
#include <utility>

#include "absl/status/status.h"
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
    NEARBY_LOGS(INFO) << "Ble Scanning already enabled; impl=" << GetImpl();
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
                  NEARBY_LOGS(INFO) << "Peripheral impl=" << &peripheral
                                    << " does not exist; add it to the map.";
                  peripherals_.insert(&peripheral);
                }

                BleV2Peripheral proxy(*this, peripheral);
                NEARBY_LOGS(INFO)
                    << "New peripheral imp=" << &peripheral
                    << ", callback the proxy peripheral=" << &proxy;
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
    NEARBY_LOGS(INFO) << "Ble Scanning enabled; impl=" << GetImpl();
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
  NEARBY_LOGS(INFO) << "Ble Scanning disabled: impl=" << GetImpl();
  return impl_->StopScanning();
}

std::unique_ptr<api::ble_v2::BleMedium::ScanningSession>
BleV2Medium::StartScanning(const Uuid& service_uuid,
                           api::ble_v2::TxPowerLevel tx_power_level,
                           api::ble_v2::BleMedium::ScanningCallback callback) {
  NEARBY_LOGS(INFO) << "platform mutex: " << &mutex_;
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
              [this](const api::ble_v2::BlePeripheral& remote_device,
                     const GattCharacteristic& characteristic, int offset,
                     ReadValueCallback callback) {
                MutexLock lock(&mutex_);
                if (server_gatt_connection_callback_
                        .on_characteristic_read_cb) {
                  server_gatt_connection_callback_.on_characteristic_read_cb(
                      remote_device, characteristic, offset,
                      std::move(callback));
                } else {
                  callback(absl::FailedPreconditionError(
                      "Read characteristic callback is not set"));
                }
              },
          .on_characteristic_write_cb =
              [this](const api::ble_v2::BlePeripheral& remote_device,
                     const GattCharacteristic& characteristic, int offset,
                     absl::string_view data, WriteValueCallback callback) {
                MutexLock lock(&mutex_);
                if (server_gatt_connection_callback_
                        .on_characteristic_write_cb) {
                  server_gatt_connection_callback_.on_characteristic_write_cb(
                      remote_device, characteristic, offset, data,
                      std::move(callback));
                } else {
                  callback(absl::FailedPreconditionError(
                      "Write characteristic callback is not set"));
                }
              },
      });
  return std::make_unique<GattServer>(*this, std::move(api_gatt_server));
}

std::unique_ptr<GattClient> BleV2Medium::ConnectToGattServer(
    BleV2Peripheral peripheral, TxPowerLevel tx_power_level,
    ClientGattConnectionCallback callback) {
  std::unique_ptr<api::ble_v2::GattClient> api_gatt_client;
  peripheral.GetImpl([&](api::ble_v2::BlePeripheral& device) {
    api_gatt_client = impl_->ConnectToGattServer(
        device, tx_power_level,
        {
            .disconnected_cb =
                [callback = std::move(callback)]() mutable {
                  callback.disconnected_cb();
                },
        });
  });

  return std::make_unique<GattClient>(std::move(api_gatt_client));
}

BleV2ServerSocket BleV2Medium::OpenServerSocket(const std::string& service_id) {
  return BleV2ServerSocket(*this, impl_->OpenServerSocket(service_id));
}

BleV2Socket BleV2Medium::Connect(const std::string& service_id,
                                 TxPowerLevel tx_power_level,
                                 const BleV2Peripheral& peripheral,
                                 CancellationFlag* cancellation_flag) {
  BleV2Socket socket;
  peripheral.GetImpl([&](api::ble_v2::BlePeripheral& device) {
    socket = BleV2Socket(peripheral, impl_->Connect(service_id, tx_power_level,
                                                    device, cancellation_flag));
  });
  return socket;
}

bool BleV2Medium::IsExtendedAdvertisementsAvailable() {
  return IsValid() && impl_->IsExtendedAdvertisementsAvailable();
}

BleV2Peripheral BleV2Medium::GetRemotePeripheral(
    const std::string& mac_address) {
  BleV2Peripheral peripheral;
  impl_->GetRemotePeripheral(mac_address,
                             [&](api::ble_v2::BlePeripheral& device) {
                               peripheral = BleV2Peripheral(*this, device);
                             });
  return peripheral;
}

absl::optional<std::string> BleV2Peripheral::GetAddress() const {
  absl::optional<std::string> address;
  GetImpl([&](api::ble_v2::BlePeripheral& device) {
    address = device.GetAddress();
  });
  return address;
}

bool BleV2Peripheral::IsValid() const {
  return GetImpl([&](api::ble_v2::BlePeripheral& device) {});
}

bool BleV2Peripheral::GetImpl(
    absl::AnyInvocable<void(api::ble_v2::BlePeripheral& device)> callback)
    const {
  if (!unique_id_.has_value()) return false;
  return medium_->GetImpl()->GetRemotePeripheral(unique_id_.value(),
                                                 std::move(callback));
}

}  // namespace nearby
