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

#include "core/internal/mediums/ble.h"

#include "platform/synchronized.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
const std::int32_t BLE<Platform>::kMaxAdvertisementLength = 512;

template <typename Platform>
BLE<Platform>::BLE(Ptr<BluetoothRadio<Platform>> bluetooth_radio)
    : lock_(Platform::createLock()),
      bluetooth_radio_(bluetooth_radio),
      bluetooth_adapter_(Platform::createBluetoothAdapter()),
      ble_medium_(Platform::createBLEMedium()),
      scanning_info_(),
      advertising_info_(),
      accepting_connections_info_() {}

template <typename Platform>
BLE<Platform>::~BLE() {
  stopAdvertising();
  stopAcceptingConnections();
  stopScanning();
}

template <typename Platform>
bool BLE<Platform>::isAvailable() {
  Synchronized s(lock_.get());

  return !ble_medium_.isNull() && !bluetooth_adapter_.isNull();
}

// TODO(ahlee): Add fastPairData for phase 2 of C++ implementation.
template <typename Platform>
bool BLE<Platform>::startAdvertising(const string& service_id,
                                     ConstPtr<ByteArray> advertisement) {
  Synchronized s(lock_.get());

  // Avoid leaks.
  ScopedPtr<ConstPtr<ByteArray>> scoped_advertisement(advertisement);
  if (scoped_advertisement.isNull() || service_id.empty()) {
    // TODO(ahlee): logger.atSevere().log("Refusing to start BLE advertising
    // because a null parameter was passed in.");
    return false;
  }

  if (scoped_advertisement->size() > kMaxAdvertisementLength) {
    // TODO(ahlee): logger.atSevere().log("Refusing to start BLE advertising
    // because the advertisement was too long. Expected at most %d bytes but
    // received %d.", kMaxAdvertisementLength, advertisement->size());
    return false;
  }

  if (isAdvertising()) {
    // TODO(ahlee): logger.atSevere().log("Failed to BLE advertise because we're
    // already advertising.");
    return false;
  }

  if (!bluetooth_radio_->isEnabled()) {
    // TODO(ahlee): logger.atSevere().log("Can't start BLE advertising because
    // Bluetooth isn't enabled.");
    return false;
  }

  if (!isAvailable()) {
    // TODO(ahlee): logger.atSevere().log("Can't start BLE advertising because
    // BLE isn't enabled.");
    return false;
  }

  if (!ble_medium_->startAdvertising(service_id,
                                     scoped_advertisement.release())) {
    // TODO(ahlee) logger.atSevere().log("Failed to start BLE advertising");
    return false;
  }

  advertising_info_ = MakePtr(new AdvertisingInfo(service_id));
  return true;
}

template <typename Platform>
void BLE<Platform>::stopAdvertising() {
  Synchronized s(lock_.get());

  if (!isAdvertising()) {
    // TODO(ahlee): logger.atDebug().log("Can't turn off BLE advertising because
    // it never started.");
    return;
  }

  ble_medium_->stopAdvertising(advertising_info_->service_id);
  // Reset our bundle of advertising state to mark that we're no longer
  // advertising.
  advertising_info_.destroy();

  // TODO(ahlee): logger.atVerbose().log("Turned BLE advertising off");
}

template <typename Platform>
bool BLE<Platform>::isAdvertising() {
  Synchronized s(lock_.get());

  return !advertising_info_.isNull();
}

template <typename Platform>
bool BLE<Platform>::startScanning(
    const string& service_id,
    Ptr<DiscoveredPeripheralCallback> discovered_peripheral_callback) {
  Synchronized s(lock_.get());

  // Avoid leaks.
  ScopedPtr<Ptr<DiscoveredPeripheralCallback>>
      scoped_discovered_peripheral_callback(discovered_peripheral_callback);
  if (scoped_discovered_peripheral_callback.isNull() || service_id.empty()) {
    // TODO(ahlee): logger.atSevere().log("Refusing to start BLE scanning
    // because a null parameter was passed in.");
    return false;
  }

  if (isScanning()) {
    // TODO(ahlee): logger.atSevere().log("Refusing to start BLE scanning
    // because we are already scanning.");
    return false;
  }

  if (!bluetooth_radio_->isEnabled()) {
    // TODO(ahlee): logger.atSevere().log("Can't start BLE scanning because
    // Bluetooth was never turned on");
    return false;
  }

  if (!isAvailable()) {
    // TODO(ahlee): logger.atSevere().log("Can't start BLE scanning because
    // BLE isn't available.");
    return false;
  }

  // Avoid leaks.
  ScopedPtr<Ptr<BLEDiscoveredPeripheralCallback>>
      scoped_ble_discovered_peripheral_callback(
          new BLEDiscoveredPeripheralCallback(
              scoped_discovered_peripheral_callback.release()));
  if (!ble_medium_->startScanning(
          service_id, scoped_ble_discovered_peripheral_callback.get())) {
    // TODO(ahlee): logger.atSevere().log("Failed to start BLE scanning.");
    return false;
  }

  scanning_info_ = MakePtr(new ScanningInfo(
      service_id, scoped_ble_discovered_peripheral_callback.release()));
  return true;
}

template <typename Platform>
void BLE<Platform>::stopScanning() {
  Synchronized s(lock_.get());

  if (!isScanning()) {
    // TODO(ahlee): logger.atDebug().log("Can't turn off BLE scanning because we
    // never started scanning.");
    return;
  }

  ble_medium_->stopScanning(scanning_info_->service_id);
  // Reset our bundle of scanning state to mark that we're no longer scanning.
  scanning_info_.destroy();
}

template <typename Platform>
bool BLE<Platform>::isScanning() {
  Synchronized s(lock_.get());

  return !scanning_info_.isNull();
}

template <typename Platform>
bool BLE<Platform>::startAcceptingConnections(
    const string& service_id,
    Ptr<AcceptedConnectionCallback> accepted_connection_callback) {
  Synchronized s(lock_.get());

  // Avoid leaks.
  ScopedPtr<Ptr<AcceptedConnectionCallback>>
      scoped_accepted_connection_callback(accepted_connection_callback);
  if (scoped_accepted_connection_callback.isNull() || service_id.empty()) {
    // TODO(ahlee): logger.atSevere().log("Refusing to start accepting BLE
    // connections because a null parameter was passed in.");
    return false;
  }

  if (isAcceptingConnections()) {
    // TODO(ahlee): logger.atSevere().log("Refusing to start accepting BLE
    // connections for %s because another BLE server socket is already
    // in-progress.", service_id);
    return false;
  }

  if (!bluetooth_radio_->isEnabled()) {
    // TODO(ahlee): logger.atSevere().log("Can't start accepting BLE connections
    // for %s because Bluetooth isn't enabled.", serviceId);
    return false;
  }

  if (!isAvailable()) {
    // TODO(ahlee): logger.atSevere().log("Can't start accepting BLE connections
    // for %s because BLE isn't available.", serviceId);
    return false;
  }

  // Avoid leaks.
  ScopedPtr<Ptr<BLEAcceptedConnectionCallback>>
      scoped_ble_accepted_connection_callback(new BLEAcceptedConnectionCallback(
          scoped_accepted_connection_callback.release()));
  if (!ble_medium_->startAcceptingConnections(
          service_id, scoped_ble_accepted_connection_callback.get())) {
    return false;
  }

  accepting_connections_info_ = MakePtr(new AcceptingConnectionsInfo(
      service_id, scoped_ble_accepted_connection_callback.release()));
  return true;
}

template <typename Platform>
void BLE<Platform>::stopAcceptingConnections() {
  Synchronized s(lock_.get());

  if (!isAcceptingConnections()) {
    // TODO(ahlee): logger.atDebug().log("Can't stop accepting BLE connections
    // because it was never started.");
    return;
  }

  ble_medium_->stopAcceptingConnections(
      accepting_connections_info_->service_id);
  // Reset our bundle of accepting connections state to mark that we're no
  // longer accepting connections.
  accepting_connections_info_.destroy();
}

template <typename Platform>
bool BLE<Platform>::isAcceptingConnections() {
  Synchronized s(lock_.get());

  return !accepting_connections_info_.isNull();
}

template <typename Platform>
Ptr<BLESocket> BLE<Platform>::connect(Ptr<BLEPeripheral> ble_peripheral,
                                      const string& service_id) {
  Synchronized s(lock_.get());

  if (ble_peripheral.isNull() || service_id.empty()) {
    // TODO(ahlee): logger.atSevere().log("Refusing to create client BLE socket
    // because at least one of blePeripheral or serviceId is null.");
    return Ptr<BLESocket>();
  }

  if (!bluetooth_radio_->isEnabled()) {
    // TODO(ahlee): logger.atSevere().log("Can't create client BLE socket to %s
    // because Bluetooth isn't enabled.", blePeripheral);
    return Ptr<BLESocket>();
  }

  if (!isAvailable()) {
    // TODO(ahlee): logger.atSevere().log("Can't create client BLE socket to %s
    // because BLE isn't available.", blePeripheral);
    return Ptr<BLESocket>();
  }

  return ble_medium_->connect(ble_peripheral, service_id);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
