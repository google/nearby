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

#include "core/internal/mediums/mediums.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
Mediums<Platform>::Mediums()
    : bluetooth_radio_(new BluetoothRadio<Platform>()),
      bluetooth_classic_(
          new BluetoothClassic<Platform>(bluetooth_radio_.get())),
      ble_(new BLE<Platform>(bluetooth_radio_.get())),
      ble_v2_(new mediums::BLEV2<Platform>(bluetooth_radio_.get())),
      wifi_lan_(new mediums::WifiLan<Platform>()) {}

template <typename Platform>
Mediums<Platform>::~Mediums() {
  // Nothing to do.
}

template <typename Platform>
Ptr<BluetoothRadio<Platform> > Mediums<Platform>::bluetoothRadio() const {
  return bluetooth_radio_.get();
}

template <typename Platform>
Ptr<BluetoothClassic<Platform> > Mediums<Platform>::bluetoothClassic() const {
  return bluetooth_classic_.get();
}

template <typename Platform>
Ptr<BLE<Platform> > Mediums<Platform>::ble() const {
  return ble_.get();
}

template <typename Platform>
Ptr<mediums::BLEV2<Platform> > Mediums<Platform>::bleV2() const {
  return ble_v2_.get();
}

template <typename Platform>
Ptr<mediums::WifiLan<Platform> > Mediums<Platform>::wifi_lan() const {
  return wifi_lan_.get();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
