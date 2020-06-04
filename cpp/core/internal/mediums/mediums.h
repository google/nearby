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

#ifndef CORE_INTERNAL_MEDIUMS_MEDIUMS_H_
#define CORE_INTERNAL_MEDIUMS_MEDIUMS_H_

#include "core/internal/mediums/ble.h"
#include "core/internal/mediums/ble_v2.h"
#include "core/internal/mediums/bluetooth_classic.h"
#include "core/internal/mediums/bluetooth_radio.h"
#include "core/internal/mediums/wifi_lan.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

// Facilitates convenient and reliable usage of various wireless mediums.
template <typename Platform>
class Mediums {
 public:
  Mediums();
  // Reverts all the mediums to their original state.
  ~Mediums();

  // Returns a handle to the Bluetooth radio.
  Ptr<BluetoothRadio<Platform> > bluetoothRadio() const;
  // Returns a handle to the Bluetooth Classic medium.
  Ptr<BluetoothClassic<Platform> > bluetoothClassic() const;
  // Returns a handle to the Bluetooth Low Energy (BLE) medium.
  Ptr<BLE<Platform> > ble() const;
  // Returns a handle to V2 of the Bluetooth Low Energy (BLE) medium.
  Ptr<mediums::BLEV2<Platform> > bleV2() const;
  // Returns a handle to the Wifi-Lan medium.
  Ptr<mediums::WifiLan<Platform> > wifi_lan() const;

 private:
  // The order of declaration is critical for both construction and
  // destruction.
  //
  // 1) Construction: The individual mediums have a dependency on the
  // corresponding radio, so the radio must be initialized first.
  //
  // 2) Destruction: The individual mediums should be shut down before the
  // corresponding radio.
  ScopedPtr<Ptr<BluetoothRadio<Platform> > > bluetooth_radio_;
  ScopedPtr<Ptr<BluetoothClassic<Platform> > > bluetooth_classic_;
  ScopedPtr<Ptr<BLE<Platform> > > ble_;
  ScopedPtr<Ptr<mediums::BLEV2<Platform> > > ble_v2_;
  ScopedPtr<Ptr<mediums::WifiLan<Platform> > > wifi_lan_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/mediums/mediums.cc"

#endif  // CORE_INTERNAL_MEDIUMS_MEDIUMS_H_
