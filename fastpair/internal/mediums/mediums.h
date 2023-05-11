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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_MEDIUMS_MEDIUMS_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_MEDIUMS_MEDIUMS_H_

#include "fastpair/internal/mediums/ble.h"
#include "fastpair/internal/mediums/ble_v2.h"
#include "fastpair/internal/mediums/bluetooth_classic.h"
#include "fastpair/internal/mediums/bluetooth_radio.h"

namespace nearby {
namespace fastpair {

// Facilitates convenient and reliable usage of various wireless mediums.
class Mediums {
 public:
  Mediums() = default;
  ~Mediums() = default;

  // Returns a handle to the Bluetooth radio.
  BluetoothRadio& GetBluetoothRadio() { return bluetooth_radio_; }

  // Returns a handle to the Ble medium.
  Ble& GetBle() { return ble_; }

  // Returns a handle to the Ble medium.
  BleV2& GetBleV2() { return ble_v2_; }

  BluetoothClassic& GetBluetoothClassic() { return bluetooth_classic_; }

 private:
  // The order of declaration is critical for both construction and
  // destruction.
  //
  // 1) Construction: The individual mediums have a dependency on the
  // corresponding radio, so the radio must be initialized first.
  //
  // 2) Destruction: The individual mediums should be shut down before the
  // corresponding radio.
  BluetoothRadio bluetooth_radio_;
  Ble ble_{bluetooth_radio_};
  BleV2 ble_v2_{bluetooth_radio_};
  BluetoothClassic bluetooth_classic_{bluetooth_radio_};
};

}  // namespace fastpair
}  // namespace nearby
#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_MEDIUMS_MEDIUMS_H_
