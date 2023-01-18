// Copyright 2020 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MEDIUMS_BLE_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MEDIUMS_BLE_H_

#include <memory>
#include <string>
#include <utility>

#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/uuid.h"
#include "presence/implementation/mediums/advertisement_data.h"
#include "presence/power_mode.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

/** Presence advertisement service data uuid. */
ABSL_CONST_INIT const nearby::Uuid kPresenceServiceUuid(0x0000fcf100001000,
                                                        0x800000805f9b34fb);

/*
 * This Ble class utilizes platform/ble_v2 BleV2Medium, provides ble functions
 * for presence logic layer to invoke.
 * This class would have states like if ble is available or not, if it's doing
 * broadcast/scan.
 */
class Ble {
 public:
  using TxPowerLevel = ::nearby::api::ble_v2::TxPowerLevel;
  using ScanningSession = ::nearby::api::ble_v2::BleMedium::ScanningSession;
  using ScanningCallback = ::nearby::api::ble_v2::BleMedium::ScanningCallback;
  using AdvertiseParameters = ::nearby::api::ble_v2::AdvertiseParameters;
  using AdvertisingSession =
      ::nearby::api::ble_v2::BleMedium::AdvertisingSession;
  using AdvertisingCallback =
      ::nearby::api::ble_v2::BleMedium::AdvertisingCallback;
  using BleAdvertisementData = ::nearby::api::ble_v2::BleAdvertisementData;
  using BleMedium = ::nearby::api::ble_v2::BleMedium;

  explicit Ble(nearby::BluetoothAdapter& bluetooth_adapter)
      : medium_(bluetooth_adapter) {}

  bool IsAvailable() const { return medium_.IsValid(); }

  // Starts broadcasting NP advertisement in `payload`. The caller should use
  // the returned `AdvertisingSession` to stop the broadcast.
  std::unique_ptr<AdvertisingSession> StartAdvertising(
      const AdvertisementData& payload, PowerMode power_mode,
      AdvertisingCallback callback) {
    BleAdvertisementData advertising_data = {
        .is_extended_advertisement = payload.is_extended_advertisement};
    advertising_data.service_data.insert(
        {kPresenceServiceUuid, nearby::ByteArray(payload.content)});
    AdvertiseParameters advertise_set_parameters = {
        .tx_power_level = ConvertPowerModeToPowerLevel(power_mode),
        .is_connectable = true,
    };
    return medium_.StartAdvertising(advertising_data, advertise_set_parameters,
                                    std::move(callback));
  }

  // Starts scanning for NP advertisements. The caller should use the returned
  // `ScanningSession` to stop scanning.
  std::unique_ptr<ScanningSession> StartScanning(ScanRequest scan_request,
                                                 ScanningCallback callback) {
    return medium_.StartScanning(
        kPresenceServiceUuid,
        ConvertPowerModeToPowerLevel(scan_request.power_mode),
        std::move(callback));
  }

  // Provides access to platform implementation. It's used in tests.
  BleMedium* GetImpl() const { return medium_.GetImpl(); }

 private:
  TxPowerLevel ConvertPowerModeToPowerLevel(PowerMode power_mode) {
    switch (power_mode) {
      case PowerMode::kNoPower:
        return TxPowerLevel::kUnknown;
      case PowerMode::kLowPower:
        return TxPowerLevel::kLow;
      case PowerMode::kBalanced:
        return TxPowerLevel::kMedium;
      case PowerMode::kLowLatency:
        return TxPowerLevel::kHigh;
    }
    return TxPowerLevel::kUnknown;
  }

  nearby::BleV2Medium medium_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MEDIUMS_BLE_H_
