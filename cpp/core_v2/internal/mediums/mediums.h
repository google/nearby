#ifndef CORE_V2_INTERNAL_MEDIUMS_MEDIUMS_H_
#define CORE_V2_INTERNAL_MEDIUMS_MEDIUMS_H_

#include "core_v2/internal/mediums/bluetooth_classic.h"
#include "core_v2/internal/mediums/bluetooth_radio.h"
#include "core_v2/internal/mediums/wifi_lan.h"


namespace location {
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

  // Returns a handle to the Wifi-Lan medium.
  WifiLan& GetWifiLan();

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
  WifiLan wifi_lan_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_MEDIUMS_H_
