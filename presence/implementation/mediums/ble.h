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

#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/uuid.h"
#include "presence/power_mode.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

/** Presence advertisement service data uuid. */
ABSL_CONST_INIT const location::nearby::Uuid kPresenceServiceUuid(
    0x0000fcf100001000, 0x800000805f9b34fb);

using ScanningSession =
    ::location::nearby::api::ble_v2::BleMedium::ScanningSession;
using ScanningCallback =
    ::location::nearby::api::ble_v2::BleMedium::ScanningCallback;
using ::location::nearby::api::ble_v2::TxPowerLevel;

/*
 * This Ble class utilizes platform/ble_v2 BleV2Medium, provides ble functions
 * for presence logic layer to invoke.
 * This class would have states like if ble is available or not, if it's doing
 * broadcast/scan.
 */
template <typename Medium>
// since we are using template for test, then we need to keep functions defs in
// the header, more details: go/cstyle#Self_contained_Headers.
class Ble {
 public:
  explicit Ble(location::nearby::BluetoothAdapter& bluetooth_adapter)
      : adapter_(bluetooth_adapter),
        medium_(std::make_unique<Medium>(bluetooth_adapter)) {}
  ~Ble() = default;

  bool IsAvailable() const { return medium_->IsValid(); }

  std::unique_ptr<ScanningSession> StartScanning(ScanRequest scan_request,
                                                 ScanningCallback callback) {
    return medium_->StartScanning(
        kPresenceServiceUuid,
        ConvertPowerModeToPowerLevel(scan_request.power_mode), callback);
  }

 private:
  friend class BleTest;
  location::nearby::BluetoothAdapter& adapter_;
  std::unique_ptr<Medium> medium_;

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
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MEDIUMS_BLE_H_
