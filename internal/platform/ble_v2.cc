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

#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace location {
namespace nearby {

using ::location::nearby::api::ble_v2::BleAdvertisementData;
using ::location::nearby::api::ble_v2::GattCharacteristic;
using ::location::nearby::api::ble_v2::TxPowerLevel;

bool BleV2Medium::StartAdvertising(
    int advertisement_id, const BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_parameters) {
  return impl_->StartAdvertising(advertisement_id, advertising_data,
                                 advertise_parameters);
}

bool BleV2Medium::StopAdvertising(int advertisement_id) {
  return impl_->StopAdvertising(advertisement_id);
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
      {
          .advertisement_found_cb =
              [this](api::ble_v2::BlePeripheral& peripheral,
                     BleAdvertisementData advertisement_data) {
                MutexLock lock(&mutex_);
                if (!peripherals_.contains(&peripheral)) {
                  NEARBY_LOGS(INFO)
                      << "There is no need to callback due to peripheral impl="
                      << &peripheral << ", which already exists.";
                  peripherals_.insert(&peripheral);
                }
                BleV2Peripheral proxy(&peripheral);
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
    NEARBY_LOG(INFO, "Ble Scanning enabled; impl=%p", GetImpl());
  }
  return success;
}

bool BleV2Medium::StopScanning() {
  MutexLock lock(&mutex_);
  if (!scanning_enabled_) return true;
  scanning_enabled_ = false;
  peripherals_.clear();
  scan_callback_ = {};
  NEARBY_LOG(INFO, "Ble Scanning disabled: impl=%p", GetImpl());
  return impl_->StopScanning();
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
      });
  return std::make_unique<GattServer>(std::move(api_gatt_server));
}

std::unique_ptr<GattClient> BleV2Medium::ConnectToGattServer(
    BleV2Peripheral peripheral, TxPowerLevel tx_power_level,
    ClientGattConnectionCallback callback) {
  {
    MutexLock lock(&mutex_);
    client_gatt_connection_callback_ = std::move(callback);
  }

  std::unique_ptr<api::ble_v2::GattClient> api_gatt_client =
      impl_->ConnectToGattServer(
          peripheral.GetImpl(), tx_power_level,
          {
              .disconnected_cb =
                  [this]() {
                    MutexLock lock(&mutex_);
                    client_gatt_connection_callback_.disconnected_cb();
                  },
          });
  return std::make_unique<GattClient>(std::move(api_gatt_client));
}

BleV2ServerSocket BleV2Medium::OpenServerSocket(const std::string& service_id) {
  return BleV2ServerSocket(impl_->OpenServerSocket(service_id));
}

BleV2Socket BleV2Medium::Connect(const std::string& service_id,
                                 TxPowerLevel tx_power_level,
                                 const BleV2Peripheral& peripheral,
                                 CancellationFlag* cancellation_flag) {
  return BleV2Socket(impl_->Connect(service_id, tx_power_level,
                                    /*mutated=*/peripheral.GetImpl(),
                                    cancellation_flag));
}

bool BleV2Medium::IsExtendedAdvertisementsAvailable() {
  return impl_->IsExtendedAdvertisementsAvailable();
}

}  // namespace nearby
}  // namespace location
