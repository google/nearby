#include "core_v2/internal/mediums/mediums.h"

namespace location {
namespace nearby {
namespace connections {

BluetoothRadio& Mediums::GetBluetoothRadio() {
  return bluetooth_radio_;
}

BluetoothClassic& Mediums::GetBluetoothClassic() {
  return bluetooth_classic_;
}

Ble& Mediums::GetBle() { return ble_; }

WifiLan& Mediums::GetWifiLan() {
  return wifi_lan_;
}

mediums::WebRtc& Mediums::GetWebRtc() { return webrtc_; }

}  // namespace connections
}  // namespace nearby
}  // namespace location
