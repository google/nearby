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

#include "third_party/nearby/cpp/cal/base/ble_types.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace location {
namespace nearby {

using BleAdvertisementData = ::nearby::cal::BleAdvertisementData;
using PowerMode = ::nearby::cal::PowerMode;

bool BleV2Medium::StartAdvertising(
    const BleAdvertisementData& advertising_data,
    const BleAdvertisementData& scan_response_data, PowerMode power_mode) {
  return impl_->StartAdvertising(advertising_data, scan_response_data,
                                 power_mode);
}

bool BleV2Medium::StopAdvertising() { return impl_->StopAdvertising(); }

bool BleV2Medium::StartScanning(const std::vector<std::string>& service_uuids,
                                PowerMode power_mode, ScanCallback callback) {
  {
    MutexLock lock(&mutex_);
    scan_callback_ = std::move(callback);
  }

  return impl_->StartScanning(
      service_uuids, power_mode,
      {
          .advertisement_found_cb =
              [this](const ::nearby::cal::BleAdvertisementData&
                         advertisement_data) {
                MutexLock lock(&mutex_);
                scan_callback_.advertisement_found_cb(advertisement_data);
              },
      });
}

bool BleV2Medium::StopScanning() {
  {
    MutexLock lock(&mutex_);
    scan_callback_ = {};
    NEARBY_LOG(INFO, "Ble Scanning disabled: impl=%p", &GetImpl());
  }
  return impl_->StopScanning();
}

GattServer BleV2Medium::StartGattServer(ServerGattConnectionCallback callback) {
  {
    MutexLock lock(&mutex_);
    server_gatt_connection_callback_ = std::move(callback);
  }

  return GattServer(impl_->StartGattServer({
      .characteristic_subscription_cb =
          [this](const ::nearby::cal::api::ServerGattConnection& connection,
                 ::nearby::cal::api::GattCharacteristic& characteristic) {
            MutexLock lock(&mutex_);
            GattCharacteristic gatt_characeristic(&characteristic);
            server_gatt_connection_callback_.characteristic_subscription_cb(
                connection, std::move(gatt_characeristic));
          },
      .characteristic_unsubscription_cb =
          [this](const ::nearby::cal::api::ServerGattConnection& connection,
                 ::nearby::cal::api::GattCharacteristic& characteristic) {
            MutexLock lock(&mutex_);
            GattCharacteristic gatt_characeristic(&characteristic);
            server_gatt_connection_callback_.characteristic_unsubscription_cb(
                connection, std::move(gatt_characeristic));
          },
  }));
}

ClientGattConnection BleV2Medium::ConnectToGattServer(
    BlePeripheralV2& peripheral, int mtu, ::nearby::cal::PowerMode power_mode,
    ClientGattConnectionCallback callback) {
  {
    MutexLock lock(&mutex_);
    client_gatt_connection_callback_ = std::move(callback);
  }
  return ClientGattConnection(impl_->ConnectToGattServer(
      peripheral.GetImpl(), mtu, power_mode,
      {.disconnected_cb =
           [this](::nearby::cal::api::ClientGattConnection& connection) {
             MutexLock lock(&mutex_);
             ClientGattConnection client_gatt_connection =
                 ClientGattConnection(&connection);
             client_gatt_connection_callback_.disconnected_cb(
                 client_gatt_connection);
           }}));
}

}  // namespace nearby
}  // namespace location
