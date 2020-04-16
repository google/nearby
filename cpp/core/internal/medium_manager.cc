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

#include "core/internal/medium_manager.h"

#include "platform/synchronized.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
MediumManager<Platform>::MediumManager()
    : mediums_(new Mediums<Platform>()),
      bluetooth_classic_lock_(Platform::createLock()),
      ble_lock_(Platform::createLock()) {}

template <typename Platform>
MediumManager<Platform>::~MediumManager() {
  // TODO(reznor): log.atDebug().log("Initiating shutdown of MediumManager.");
  Synchronized s1(bluetooth_classic_lock_.get());
  Synchronized s2(ble_lock_.get());

  mediums_.destroy();
  // TODO(reznor): log.atDebug().log("MediumManager has shut down.");
}

// ~~~~~~~~~~~~~~~~~~~~~~~~ BLUETOOTH ~~~~~~~~~~~~~~~~~~~~~~~~

template <typename Platform>
bool MediumManager<Platform>::isBluetoothAvailable() {
  Synchronized s(bluetooth_classic_lock_.get());

  return mediums_->bluetoothClassic()->isAvailable();
}

template <typename Platform>
bool MediumManager<Platform>::turnOnBluetoothDiscoverability(
    const string& device_name) {
  Synchronized s(bluetooth_classic_lock_.get());

  return mediums_->bluetoothRadio()->enable() &&
         mediums_->bluetoothClassic()->turnOnDiscoverability(device_name);
}

template <typename Platform>
void MediumManager<Platform>::turnOffBluetoothDiscoverability() {
  Synchronized s(bluetooth_classic_lock_.get());

  mediums_->bluetoothClassic()->turnOffDiscoverability();
}

template <typename Platform>
class DiscoveredDeviceCallback
    : public BluetoothClassic<Platform>::DiscoveredDeviceCallback {
 public:
  typedef typename MediumManager<Platform>::FoundBluetoothDeviceProcessor
      FoundBluetoothDeviceProcessor;

  explicit DiscoveredDeviceCallback(
      Ptr<FoundBluetoothDeviceProcessor> found_bluetooth_device_processor)
      : found_bluetooth_device_processor_(found_bluetooth_device_processor) {}

  void onDeviceDiscovered(Ptr<BluetoothDevice> device) override {
    found_bluetooth_device_processor_->onFoundBluetoothDevice(device);
  }

  void onDeviceNameChanged(Ptr<BluetoothDevice> device) override {
    found_bluetooth_device_processor_->onFoundBluetoothDevice(device);
  }

  void onDeviceLost(Ptr<BluetoothDevice> device) override {
    found_bluetooth_device_processor_->onLostBluetoothDevice(device);
  }

 private:
  ScopedPtr<Ptr<FoundBluetoothDeviceProcessor> >
      found_bluetooth_device_processor_;
};

template <typename Platform>
bool MediumManager<Platform>::startScanningForBluetoothDevices(
    Ptr<FoundBluetoothDeviceProcessor> found_bluetooth_device_processor) {
  Synchronized s(bluetooth_classic_lock_.get());

  return mediums_->bluetoothRadio()->enable() &&
         mediums_->bluetoothClassic()->startDiscovery(
             MakePtr(new DiscoveredDeviceCallback<Platform>(
                 found_bluetooth_device_processor)));
}

template <typename Platform>
void MediumManager<Platform>::stopScanningForBluetoothDevices() {
  Synchronized s(bluetooth_classic_lock_.get());

  mediums_->bluetoothClassic()->stopDiscovery();
}

template <typename Platform>
bool MediumManager<Platform>::isListeningForIncomingBluetoothConnections(
    const string& service_name) {
  Synchronized s(bluetooth_classic_lock_.get());

  return mediums_->bluetoothClassic()->isAcceptingConnections(service_name);
}

template <typename Platform>
class BluetoothAcceptedConnectionCallback
    : public BluetoothClassic<Platform>::AcceptedConnectionCallback {
 public:
  typedef typename MediumManager<Platform>::IncomingBluetoothConnectionProcessor
      IncomingBluetoothConnectionProcessor;

  explicit BluetoothAcceptedConnectionCallback(
      Ptr<IncomingBluetoothConnectionProcessor>
          incoming_bluetooth_connection_processor)
      : incoming_bluetooth_connection_processor_(
            incoming_bluetooth_connection_processor) {}

  void onConnectionAccepted(Ptr<BluetoothSocket> socket) override {
    incoming_bluetooth_connection_processor_->onIncomingBluetoothConnection(
        socket);
  }

 private:
  ScopedPtr<Ptr<IncomingBluetoothConnectionProcessor> >
      incoming_bluetooth_connection_processor_;
};

template <typename Platform>
bool MediumManager<Platform>::startListeningForIncomingBluetoothConnections(
    const string& service_name, Ptr<IncomingBluetoothConnectionProcessor>
                                    incoming_bluetooth_connection_processor) {
  Synchronized s(bluetooth_classic_lock_.get());

  return mediums_->bluetoothRadio()->enable() &&
         mediums_->bluetoothClassic()->startAcceptingConnections(
             service_name,
             MakePtr(new BluetoothAcceptedConnectionCallback<Platform>(
                 incoming_bluetooth_connection_processor)));
}

template <typename Platform>
void MediumManager<Platform>::stopListeningForIncomingBluetoothConnections(
    const string& service_name) {
  Synchronized s(bluetooth_classic_lock_.get());

  mediums_->bluetoothClassic()->stopAcceptingConnections(service_name);
}

template <typename Platform>
Ptr<BluetoothSocket> MediumManager<Platform>::connectToBluetoothDevice(
    Ptr<BluetoothDevice> bluetooth_device, const string& service_name) {
  Synchronized s(bluetooth_classic_lock_.get());

  if (!mediums_->bluetoothRadio()->enable()) {
    return Ptr<BluetoothSocket>();
  }

  return mediums_->bluetoothClassic()->connect(bluetooth_device, service_name);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~ BLE ~~~~~~~~~~~~~~~~~~~~~~~~
template <typename Platform>
bool MediumManager<Platform>::isBleAvailable() {
  Synchronized s(ble_lock_.get());

#if BLE_V2_IMPLEMENTED
  return mediums_->bleV2()->isAvailable();
#else
  return mediums_->ble()->isAvailable();
#endif
}

// TODO(ahlee): Add nearbyNotificationsBeaconData for phase 2 of implementation.
// TODO(ahlee): Add fast_advertisement_service_uuid and power_level to
// AdvertisingOptions and pass it through.
template <typename Platform>
bool MediumManager<Platform>::startBleAdvertising(
    const string& service_id, ConstPtr<ByteArray> advertisement_data) {
  Synchronized s(ble_lock_.get());

  return mediums_->bluetoothRadio()->enable() &&
#if BLE_V2_IMPLEMENTED
         mediums_->bleV2()->startAdvertising(
             service_id, advertisement_data, BLEMediumV2::PowerMode::HIGH,
             /* fast_advertisement_service_uuid= */ "");
#else
         mediums_->ble()->startAdvertising(service_id, advertisement_data);
#endif
}

template <typename Platform>
void MediumManager<Platform>::stopBleAdvertising(const string& service_id) {
  Synchronized s(ble_lock_.get());

#if BLE_V2_IMPLEMENTED
  mediums_->bleV2()->stopAdvertising();
#else
  mediums_->ble()->stopAdvertising();
#endif
}

#if BLE_V2_IMPLEMENTED
template <typename Platform>
class BLEAcceptedConnectionCallback
    : public mediums::BLEV2<Platform>::AcceptedConnectionCallback {
 public:
  BLEAcceptedConnectionCallback() {}
};
#else
template <typename Platform>
class BLEAcceptedConnectionCallback
    : public BLE<Platform>::AcceptedConnectionCallback {
 public:
  typedef typename MediumManager<Platform>::IncomingBleConnectionProcessor
      IncomingBleConnectionProcessor;

  explicit BLEAcceptedConnectionCallback(
      Ptr<IncomingBleConnectionProcessor> incoming_ble_connection_processor)
      : incoming_ble_connection_processor_(incoming_ble_connection_processor) {}

  void onConnectionAccepted(Ptr<BLESocket> socket,
                            const string& service_id) override {
    incoming_ble_connection_processor_->onIncomingBleConnection(socket,
                                                                service_id);
  }

 private:
  ScopedPtr<Ptr<IncomingBleConnectionProcessor> >
      incoming_ble_connection_processor_;
};
#endif

template <typename Platform>
bool MediumManager<Platform>::isListeningForIncomingBleConnections(
    const string& service_id) {
  Synchronized s(ble_lock_.get());

#if BLE_V2_IMPLEMENTED
  return mediums_->bleV2()->isAcceptingConnections();
#else
  return mediums_->ble()->isAcceptingConnections();
#endif
}

template <typename Platform>
bool MediumManager<Platform>::startListeningForIncomingBleConnections(
    const string& service_id,
    Ptr<IncomingBleConnectionProcessor> incoming_ble_connection_processor) {
  Synchronized s(ble_lock_.get());

  return mediums_->bluetoothRadio()->enable() &&
#if BLE_V2_IMPLEMENTED
         mediums_->bleV2()->startAcceptingConnections(
             service_id,
             MakePtr(new BLEAcceptedConnectionCallback<Platform>()));
#else
         mediums_->ble()->startAcceptingConnections(
             service_id, MakePtr(new BLEAcceptedConnectionCallback<Platform>(
                             incoming_ble_connection_processor)));
#endif
}

template <typename Platform>
void MediumManager<Platform>::stopListeningForIncomingBleConnections(
    const string& service_id) {
  Synchronized s(ble_lock_.get());

#if BLE_V2_IMPLEMENTED
  mediums_->bleV2()->stopAcceptingConnections();
#else
  mediums_->ble()->stopAcceptingConnections();
#endif
}

template <typename Platform>
class DiscoveredPeripheralCallback : public DISCOVERED_PERIPHERAL_CALLBACK {
 public:
  typedef typename MediumManager<Platform>::FoundBlePeripheralProcessor
      FoundBlePeripheralProcessor;

  explicit DiscoveredPeripheralCallback(
      Ptr<FoundBlePeripheralProcessor> found_ble_peripheral_processor)
      : found_ble_peripheral_processor_(found_ble_peripheral_processor) {}

  void onPeripheralDiscovered(Ptr<BLE_PERIPHERAL> ble_peripheral,
                              const string& service_id,
#if BLE_V2_IMPLEMENTED
                              ConstPtr<ByteArray> advertisement_data,
                              // TODO(ahlee): Add is_fast_advertisement to
                              // FoundBlePeripheralProcessor.
                              bool is_fast_advertisement) override {
#else
                              ConstPtr<ByteArray> advertisement_data) {
#endif
    found_ble_peripheral_processor_->onFoundBlePeripheral(
        ble_peripheral, service_id, advertisement_data);
  }

  void onPeripheralLost(Ptr<BLE_PERIPHERAL> ble_peripheral,
                        const string& service_id) override {
    found_ble_peripheral_processor_->onLostBlePeripheral(ble_peripheral,
                                                         service_id);
  }

 private:
  ScopedPtr<Ptr<FoundBlePeripheralProcessor> > found_ble_peripheral_processor_;
};

// TODO(ahlee): Add fast_advertisement_service_uuid and power_level to
// DiscoveryOptions and pass it through.
template <typename Platform>
bool MediumManager<Platform>::startBleScanning(
    const string& service_id,
    Ptr<FoundBlePeripheralProcessor> found_ble_peripheral_processor) {
  Synchronized s(ble_lock_.get());

  return mediums_->bluetoothRadio()->enable() &&
#if BLE_V2_IMPLEMENTED
         mediums_->bleV2()->startScanning(
             service_id,
             MakePtr(new DiscoveredPeripheralCallback<Platform>(
                 found_ble_peripheral_processor)),
             BLEMediumV2::PowerMode::HIGH,
             /* fast_advertisement_service_uuid= */ "");
#else
         mediums_->ble()->startScanning(
             service_id, MakePtr(new DiscoveredPeripheralCallback<Platform>(
                             found_ble_peripheral_processor)));
#endif
}

template <typename Platform>
void MediumManager<Platform>::stopBleScanning(const string& service_id) {
  Synchronized s(ble_lock_.get());

#if BLE_V2_IMPLEMENTED
  mediums_->bleV2()->stopScanning();
#else
  mediums_->ble()->stopScanning();
#endif
}

template <typename Platform>
Ptr<BLESocket> MediumManager<Platform>::connectToBlePeripheral(
    Ptr<BLE_PERIPHERAL> ble_peripheral, const string& service_id) {
  Synchronized s(ble_lock_.get());

  if (!mediums_->bluetoothRadio()->enable()) {
    return Ptr<BLESocket>();
  }

#if BLE_V2_IMPLEMENTED
  // TODO(ahlee): Replace when connecting logic is implemented.
  return Ptr<BLESocket>();
#else
  return mediums_->ble()->connect(ble_peripheral, service_id);
#endif
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
