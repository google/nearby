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
      ble_v2_(new mediums::BLEV2<Platform>(bluetooth_radio_.get())) {}

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

}  // namespace connections
}  // namespace nearby
}  // namespace location
