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

#include "connections/implementation/mediums/mediums.h"

namespace nearby {
namespace connections {

BluetoothRadio& Mediums::GetBluetoothRadio() { return bluetooth_radio_; }

BluetoothClassic& Mediums::GetBluetoothClassic() { return bluetooth_classic_; }

Ble& Mediums::GetBle() { return ble_; }

BleV2& Mediums::GetBleV2() { return ble_v2_; }

Wifi& Mediums::GetWifi() { return wifi_; }

WifiLan& Mediums::GetWifiLan() { return wifi_lan_; }

WifiHotspot& Mediums::GetWifiHotspot() { return wifi_hotspot_; }

WifiDirect& Mediums::GetWifiDirect() { return wifi_direct_; }

mediums::WebRtc& Mediums::GetWebRtc() { return webrtc_; }

}  // namespace connections
}  // namespace nearby
