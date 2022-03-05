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

#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace location {
namespace nearby {

using ::location::nearby::api::ble_v2::BleAdvertisementData;
using ::location::nearby::api::ble_v2::GattCharacteristic;
using ::location::nearby::api::ble_v2::PowerMode;

bool BleV2Medium::StartAdvertising(
    const BleAdvertisementData& advertising_data,
    const BleAdvertisementData& scan_response_data, PowerMode power_mode) {
  return impl_->StartAdvertising(advertising_data, scan_response_data,
                                 power_mode);
}

bool BleV2Medium::StopAdvertising() { return impl_->StopAdvertising(); }

std::unique_ptr<GattServer> BleV2Medium::StartGattServer(
    ServerGattConnectionCallback callback) {
  {
    MutexLock lock(&mutex_);
    server_gatt_connection_callback_ = std::move(callback);
  }

  std::unique_ptr<api::ble_v2::GattServer> api_gatt_server =
      impl_->StartGattServer({
          .characteristic_subscription_cb =
              [this](api::ble_v2::ServerGattConnection& connection,
                     const GattCharacteristic& characteristic) {
                MutexLock lock(&mutex_);
                ServerGattConnection server_gatt_connection(&connection);
                server_gatt_connection_callback_.characteristic_subscription_cb(
                    server_gatt_connection, characteristic);
              },
          .characteristic_unsubscription_cb =
              [this](api::ble_v2::ServerGattConnection& connection,
                     const GattCharacteristic& characteristic) {
                MutexLock lock(&mutex_);
                ServerGattConnection server_gatt_connection(&connection);
                server_gatt_connection_callback_
                    .characteristic_unsubscription_cb(server_gatt_connection,
                                                      characteristic);
              },
      });
  return std::make_unique<GattServer>(std::move(api_gatt_server));
}

}  // namespace nearby
}  // namespace location
