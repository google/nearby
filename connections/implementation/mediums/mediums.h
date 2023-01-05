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

#include "connections/implementation/mediums/ble.h"
#include "connections/implementation/mediums/ble_v2.h"
#include "connections/implementation/mediums/bluetooth_classic.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#ifdef NO_WEBRTC
#include "connections/implementation/mediums/webrtc_stub.h"
#else
#include "connections/implementation/mediums/webrtc.h"
#endif
#include "connections/implementation/mediums/wifi.h"
#include "connections/implementation/mediums/wifi_hotspot.h"
#include "connections/implementation/mediums/wifi_direct.h"
#include "connections/implementation/mediums/wifi_lan.h"

namespace nearby {
namespace connections {

// Facilitates convenient and reliable usage of various wireless mediums.
class Mediums {
 public:
  Mediums() = default;
  ~Mediums() = default;

  // Returns a handle to the Bluetooth radio.
  BluetoothRadio& GetBluetoothRadio();

  // Returns a handle to the Bluetooth Classic medium.
  BluetoothClassic& GetBluetoothClassic();

  // Returns a handle to the Ble medium.
  Ble& GetBle();

  // Returns a handle to the Ble medium.
  BleV2& GetBleV2();

  // Returns a handle to the Wifi medium.
  Wifi& GetWifi();

  // Returns a handle to the Wifi-Lan medium.
  WifiLan& GetWifiLan();

  // Returns a handle to the Wifi-Hotspot medium.
  WifiHotspot& GetWifiHotspot();

  // Returns a handle to the Wifi-Direct medium.
  WifiDirect& GetWifiDirect();

  // Returns a handle to the WebRtc medium.
  mediums::WebRtc& GetWebRtc();

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
  BluetoothClassic bluetooth_classic_{bluetooth_radio_};
  Ble ble_{bluetooth_radio_};
  BleV2 ble_v2_{bluetooth_radio_};
  Wifi wifi_;
  WifiLan wifi_lan_;
  WifiHotspot wifi_hotspot_;
  WifiDirect wifi_direct_;
  mediums::WebRtc webrtc_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_MEDIUMS_H_
