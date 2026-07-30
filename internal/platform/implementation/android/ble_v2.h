// Copyright 2024 Google LLC
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

#ifndef PLATFORM_IMPL_ANDROID_BLE_V2_H_
#define PLATFORM_IMPL_ANDROID_BLE_V2_H_

#include <memory>
#include <string>

#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace android {

// BlePeripheral implementation.
class BleV2Peripheral : public api::ble_v2::BlePeripheral {
 public:
  std::string GetAddress() const override;
  api::ble_v2::BlePeripheral::UniqueId GetUniqueId() const override;
};

class BleV2Socket : public api::ble_v2::BleSocket {
 public:
  ~BleV2Socket() override = default;

  InputStream& GetInputStream() override;

  OutputStream& GetOutputStream() override;

  Exception Close() override;

  api::ble_v2::BlePeripheral* GetRemotePeripheral() override;
};

class BleV2ServerSocket : public api::ble_v2::BleServerSocket {
 public:
  ~BleV2ServerSocket() override = default;

  std::unique_ptr<api::ble_v2::BleSocket> Accept() override;

  Exception Close() override;
};

// Container of operations that can be performed over the BLE medium.
class BleV2Medium : public api::ble_v2::BleMedium {
 public:
  ~BleV2Medium() override = default;

  bool StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertise_set_parameters) override;

  std::unique_ptr<AdvertisingSession> StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertise_set_parameters,
      AdvertisingCallback callback) override;

  bool StopAdvertising() override;

  bool StartScanning(const Uuid& service_uuid,
                     api::ble_v2::TxPowerLevel tx_power_level,
                     ScanCallback callback) override;

  bool StopScanning() override;

  std::unique_ptr<ScanningSession> StartScanning(
      const Uuid& service_uuid, api::ble_v2::TxPowerLevel tx_power_level,
      ScanningCallback callback) override;

  std::unique_ptr<api::ble_v2::GattServer> StartGattServer(
      api::ble_v2::ServerGattConnectionCallback callback) override;

  std::unique_ptr<api::ble_v2::GattClient> ConnectToGattServer(
      api::ble_v2::BlePeripheral& peripheral,
      api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::ClientGattConnectionCallback callback) override;

  std::unique_ptr<api::ble_v2::BleServerSocket> OpenServerSocket(
      const std::string& service_id) override;

  std::unique_ptr<api::ble_v2::BleSocket> Connect(
      const std::string& service_id, api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::BlePeripheral& peripheral,
      CancellationFlag* cancellation_flag) override;

  // Requests if support extended advertisement.
  bool IsExtendedAdvertisementsAvailable() override;

  // Calls `callback` and returns true if `mac_address` is a valid BLE address.
  // Otherwise, does not call the callback and returns false.
  //
  // This method is not available on Apple platforms and will always return
  // false, ignoring the callback.
  bool GetRemotePeripheral(const std::string& mac_address,
                           GetRemotePeripheralCallback callback) override;

  // Calls `callback` and returns true if `id` refers to a known BLE peripheral.
  // Otherwise, does not call the callback and returns false.
  bool GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId id,
                           GetRemotePeripheralCallback callback) override;
};

}  // namespace android
}  // namespace nearby

#endif  // PLATFORM_IMPL_ANDROID_BLE_V2_H_
