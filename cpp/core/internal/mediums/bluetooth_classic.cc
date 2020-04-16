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

#include "core/internal/mediums/bluetooth_classic.h"

#include <utility>

#include "core/internal/mediums/uuid.h"
#include "platform/synchronized.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
const std::int32_t BluetoothClassic<Platform>::kMaxConcurrentAcceptLoops = 5;

template <typename Platform>
BluetoothClassic<Platform>::BluetoothClassic(
    Ptr<BluetoothRadio<Platform>> bluetooth_radio)
    : lock_(Platform::createLock()),
      bluetooth_radio_(bluetooth_radio),
      bluetooth_adapter_(Platform::createBluetoothAdapter()),
      bluetooth_classic_medium_(Platform::createBluetoothClassicMedium()),
      scan_info_(),
      original_scan_mode_(BluetoothAdapter::ScanMode::UNKNOWN),
      original_device_name_(),
      accept_loops_thread_pool_(
          Platform::createMultiThreadExecutor(kMaxConcurrentAcceptLoops)),
      bluetooth_server_sockets_() {}

template <typename Platform>
BluetoothClassic<Platform>::~BluetoothClassic() {
  stopDiscovery();
  for (BluetoothServerSocketMap::iterator it =
           bluetooth_server_sockets_.begin();
       it != bluetooth_server_sockets_.end(); ++it) {
    stopAcceptingConnections(it->first);
  }
  turnOffDiscoverability();

  // All the AcceptLoopRunnable objects in here should already have gotten an
  // opportunity to shut themselves down cleanly in the calls to
  // stopAcceptingConnections() above.
  accept_loops_thread_pool_->shutdown();

  original_device_name_.destroy();
  scan_info_.destroy();
}

template <typename Platform>
bool BluetoothClassic<Platform>::isAvailable() {
  Synchronized s(lock_.get());

  return !bluetooth_classic_medium_.isNull() && !bluetooth_adapter_.isNull();
}

template <typename Platform>
bool BluetoothClassic<Platform>::turnOnDiscoverability(
    const string& device_name) {
  Synchronized s(lock_.get());

  if (device_name.empty()) {
    // TODO(ahlee): logger.atSevere().log("Refusing to turn on Bluetooth
    // discoverability because a null deviceName was passed in.");
    return false;
  }

  if (!bluetooth_radio_->isEnabled()) {
    // TODO(reznor): log.atSevere().log("Can't turn on Bluetooth discoverability
    // because Bluetooth isn't enabled.");
    return false;
  }

  if (!isAvailable()) {
    // TODO(reznor): log.atSevere().log("Can't turn on Bluetooth discoverability
    // because Bluetooth isn't available.");
    return false;
  }

  if (isDiscoverable()) {
    // TODO(reznor): log.atSevere().log("Refusing to turn on Bluetooth
    // discoverability with device name %s because we're already discoverable
    // with device name %s.", deviceName, bluetoothAdapter.getName());
    return false;
  }

  if (!modifyDeviceName(device_name)) {
    // TODO(reznor): log.atSevere().log("Failed to turn on Bluetooth
    // discoverability because we couldn't set the device name to %s",
    // deviceName);
    return false;
  }

  if (!modifyScanMode(BluetoothAdapter::ScanMode::CONNECTABLE_DISCOVERABLE)) {
    // TODO(reznor): log.atSevere().log("Failed to turn on Bluetooth
    // discoverability because we couldn't set the scan mode to %d",
    // BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE);

    // Don't forget to perform this rollback of the partial state changes we've
    // made til now.
    restoreDeviceName();
    return false;
  }

  // TODO(reznor): log.atVerbose().log("Turned on Bluetooth discoverability with
  // deviceName %s", deviceName);
  return true;
}

template <typename Platform>
void BluetoothClassic<Platform>::turnOffDiscoverability() {
  Synchronized s(lock_.get());

  if (!isDiscoverable()) {
    // TODO(reznor): log.atDebug().log("Can't turn off Bluetooth discoverability
    // because it was never turned on.");
    return;
  }

  restoreScanMode();
  restoreDeviceName();

  // TODO(reznor): log.atVerbose().log("Turned Bluetooth discoverability off");
}

template <typename Platform>
bool BluetoothClassic<Platform>::isDiscoverable() const {
  return ((!original_device_name_.isNull()) &&
          (BluetoothAdapter::ScanMode::CONNECTABLE_DISCOVERABLE ==
           bluetooth_adapter_->getScanMode()));
}

template <typename Platform>
bool BluetoothClassic<Platform>::modifyDeviceName(const string& device_name) {
  original_device_name_ = bluetooth_adapter_->getName();

  if (!bluetooth_adapter_->setName(device_name)) {
    original_device_name_.destroy();
    return false;
  }

  return true;
}

template <typename Platform>
bool BluetoothClassic<Platform>::modifyScanMode(
    BluetoothAdapter::ScanMode::Value scan_mode) {
  original_scan_mode_ = bluetooth_adapter_->getScanMode();

  if (!bluetooth_adapter_->setScanMode(scan_mode)) {
    original_scan_mode_ = BluetoothAdapter::ScanMode::UNKNOWN;
    return false;
  }

  return true;
}

template <typename Platform>
void BluetoothClassic<Platform>::restoreScanMode() {
  if (!bluetooth_adapter_->setScanMode(original_scan_mode_)) {
    // TODO(reznor): log.atWarning().log("Failed to restore original Bluetooth
    // scan mode to %d", originalScanMode);
  }

  // Regardless of whether or not we could actually restore the Bluetooth scan
  // mode, reset our relevant state.
  original_scan_mode_ = BluetoothAdapter::ScanMode::UNKNOWN;
}

template <typename Platform>
void BluetoothClassic<Platform>::restoreDeviceName() {
  if (!bluetooth_adapter_->setName(*original_device_name_)) {
    // TODO(reznor): log.atWarning().log("Failed to restore original Bluetooth
    // device name to %s", originalDeviceName);
  }

  // Regardless of whether or not we could actually restore the Bluetooth device
  // name, reset the marker that opens us up for business for the next time
  // 'round.
  original_device_name_.destroy();
}

template <typename Platform>
bool BluetoothClassic<Platform>::startDiscovery(
    Ptr<DiscoveredDeviceCallback> discovered_device_callback) {
  Synchronized s(lock_.get());

  if (discovered_device_callback.isNull()) {
    // TODO(reznor): log.atSevere().log("Refusing to start discovery of
    // Bluetooth devices because a null discoveredDeviceCallback was passed
    // in.");
    return false;
  }
  // Avoid leaks.
  ScopedPtr<Ptr<DiscoveredDeviceCallback>> scoped_discovered_device_callback(
      discovered_device_callback);

  if (!bluetooth_radio_->isEnabled()) {
    // TODO(reznor): log.atSevere().log("Can't discover Bluetooth devices
    // because Bluetooth isn't enabled.");
    return false;
  }

  if (!isAvailable()) {
    // TODO(reznor): log.atSevere().log("Can't discover Bluetooth devices
    // because Bluetooth isn't available.");
    return false;
  }

  if (isDiscovering()) {
    // TODO(reznor): log.atSevere().log("Refusing to start discovery of
    // Bluetooth devices because another discovery is already in-progress.");
    return false;
  }

  // Avoid leaks.
  ScopedPtr<Ptr<BluetoothDiscoveryCallback>>
      scoped_bluetooth_discovery_callback(new BluetoothDiscoveryCallback(
          scoped_discovered_device_callback.get()));

  if (!bluetooth_classic_medium_->startDiscovery(
          scoped_bluetooth_discovery_callback.get())) {
    // TODO(reznor): log.atSevere().log("Failed to start discovery of Bluetooth
    // devices.");
    return false;
  }

  // Mark the fact that we're currently performing a Bluetooth scan.
  scan_info_ =
      MakePtr(new ScanInfo(scoped_discovered_device_callback.release(),
                           scoped_bluetooth_discovery_callback.release()));
  return true;
}

template <typename Platform>
void BluetoothClassic<Platform>::stopDiscovery() {
  Synchronized s(lock_.get());

  if (!isDiscovering()) {
    // TODO(reznor): log.atDebug().log("Can't stop discovery of Bluetooth
    // devices because it never started.");
    return;
  }

  if (!bluetooth_classic_medium_->stopDiscovery()) {
    // TODO(reznor): log.atWarning().log("Failed to stop discovery of Bluetooth
    // devices.");
  }
  // Regardless of whether or not stopDiscovery() succeeded, destroy scan_info_
  // to:
  //
  // a) Avoid a leak.
  // b) Mark the fact that we're no longer performing a Bluetooth discovery.
  scan_info_.destroy();
}

template <typename Platform>
bool BluetoothClassic<Platform>::isDiscovering() const {
  return !scan_info_.isNull();
}

template <typename Platform>
class AcceptLoopRunnable : public Runnable {
 public:
  AcceptLoopRunnable(
      Ptr<typename BluetoothClassic<Platform>::AcceptedConnectionCallback>
          accepted_connection_callback,
      Ptr<BluetoothServerSocket> listening_socket, const string& service_name)
      : accepted_connection_callback_(accepted_connection_callback),
        listening_socket_(listening_socket),
        service_name_(service_name) {}

  void run() override {
    while (true) {
      ExceptionOr<Ptr<BluetoothSocket>> bluetooth_socket =
          listening_socket_->accept();
      if (!bluetooth_socket.ok()) {
        if (Exception::IO == bluetooth_socket.exception()) {
          Utils::closeSocket(listening_socket_, "Bluetooth", service_name_);
        }
        break;
      }

      accepted_connection_callback_->onConnectionAccepted(
          bluetooth_socket.result());
    }
  }

 private:
  ScopedPtr<
      Ptr<typename BluetoothClassic<Platform>::AcceptedConnectionCallback>>
      accepted_connection_callback_;
  Ptr<BluetoothServerSocket> listening_socket_;
  const string service_name_;
};

template <typename Platform>
bool BluetoothClassic<Platform>::startAcceptingConnections(
    const string& service_name,
    Ptr<AcceptedConnectionCallback> accepted_connection_callback) {
  Synchronized s(lock_.get());

  // Avoid leaks.
  ScopedPtr<Ptr<AcceptedConnectionCallback>>
      scoped_accepted_connection_callback(accepted_connection_callback);
  if (scoped_accepted_connection_callback.isNull() || service_name.empty()) {
    // TODO(reznor): log.atSevere().log("Refusing to start accepting Bluetooth
    // connections because at least one of serviceName or
    // acceptedConnectionCallback is null.");
    return false;
  }

  if (!bluetooth_radio_->isEnabled()) {
    // TODO(reznor): log.atSevere().log("Can't create Bluetooth server socket
    // for %s because Bluetooth isn't enabled.", serviceName);
    return false;
  }

  if (!isAvailable()) {
    // TODO(reznor): log.atSevere().log("Can't start accepting BLuetooth
    // connections for %s because Bluetooth isn't available.", serviceName);
    return false;
  }

  if (isAcceptingConnections(service_name)) {
    // TODO(reznor): log.atSevere().log("Refusing to start accepting Bluetooth
    // connections for %s because a Bluetooth server is already in-progress for
    // that service name.", serviceName);
    return false;
  }

  ExceptionOr<Ptr<BluetoothServerSocket>> listening_socket =
      bluetooth_classic_medium_->listenForService(
          service_name, generateUUIDFromString(service_name));
  if (!listening_socket.ok()) {
    if (Exception::IO == listening_socket.exception()) {
      // TODO(reznor): log.atSevere().withCause(e).log("Failed to start
      // accepting Bluetooth connections for %s.", serviceName);
      return false;
    }
  }

  // Start the accept loop on a dedicated thread - this stays alive and
  // listening for new incoming connections until stopAcceptingConnections() is
  // invoked.
  accept_loops_thread_pool_->execute(MakePtr(new AcceptLoopRunnable<Platform>(
      scoped_accepted_connection_callback.release(), listening_socket.result(),
      service_name)));

  // Mark the fact that there's an in-progress Bluetooth server accepting
  // connections.
  bluetooth_server_sockets_.insert(
      std::make_pair(service_name, listening_socket.result()));
  return true;
}

template <typename Platform>
bool BluetoothClassic<Platform>::isAcceptingConnections(
    const string& service_name) {
  Synchronized s(lock_.get());

  return bluetooth_server_sockets_.find(service_name) !=
         bluetooth_server_sockets_.end();
}

template <typename Platform>
void BluetoothClassic<Platform>::stopAcceptingConnections(
    const string& service_name) {
  Synchronized s(lock_.get());

  if (service_name.empty()) {
    // TODO(ahlee): logger.atSevere().log("Unable to stop accepting Bluetooth
    // connections because the serviceName is empty.");
    return;
  }

  if (!isAcceptingConnections(service_name)) {
    // TODO(reznor): log.atDebug().log("Can't stop accepting Bluetooth
    // connections for %s because it was never started.", serviceName);
    return;
  }

  // Closing the BluetoothServerSocket will kick off the suicide of the thread
  // in accept_loops_thread_pool_ that blocks on BluetoothServerSocket.accept().
  // That may take some time to complete, but there's no particular reason to
  // wait around for it.
  BluetoothServerSocketMap::iterator listening_socket_iter =
      bluetooth_server_sockets_.find(service_name);

  // Store a handle to the BluetoothServerSocket, so we can use it after
  // removing the entry from bluetooth_server_sockets_; making it scoped
  // is a bonus that takes care of deallocation before we leave this method.
  ScopedPtr<Ptr<BluetoothServerSocket>> scoped_listening_socket(
      listening_socket_iter->second);

  // Regardless of whether or not we fail to close the existing
  // BluetoothServerSocket, remove it from bluetooth_server_sockets_ so that it
  // frees up this service for another round.
  bluetooth_server_sockets_.erase(listening_socket_iter);

  // Finally, close the BluetoothServerSocket.
  Exception::Value e = scoped_listening_socket->close();
  if (Exception::NONE != e) {
    if (Exception::IO == e) {
      // TODO(reznor): log.atSevere().withCause(e).log("Failed to close
      // Bluetooth server socket for %s.", serviceName);
    }
  }
}

template <typename Platform>
Ptr<BluetoothSocket> BluetoothClassic<Platform>::connect(
    Ptr<BluetoothDevice> bluetooth_device, const string& service_name) {
  Synchronized s(lock_.get());

  if (bluetooth_device.isNull() || service_name.empty()) {
    // TODO(reznor): log.atSevere().log("Refusing to create client Bluetooth
    // socket because at least one of bluetoothDevice or serviceName is null.");
    return Ptr<BluetoothSocket>();
  }

  if (!bluetooth_radio_->isEnabled()) {
    // TODO(reznor): log.atSevere().log("Can't create client Bluetooth socket to
    // %s because Bluetooth isn't enabled.", bluetoothSocketName);
    return Ptr<BluetoothSocket>();
  }

  if (!isAvailable()) {
    // TODO(reznor): log.atSevere().log("Can't create client Bluetooth socket to
    // %s because Bluetooth isn't available.", bluetoothSocketName);
    return Ptr<BluetoothSocket>();
  }

  //                      WARNING WARNING WARNING
  //
  // This block deviates from the corresponding Java code.
  //
  // In Java, we pause an in-progress discovery before attempting this
  // connection, and then resume it after, but the memory management of the
  // DiscoveredDeviceCallback is complicated in C++, and would need a severe
  // deviation from the Java code, so we're choosing the lesser of 2 evils, and
  // introducing this (simplifying) deviation instead -- also, this deviation is
  // fairly inconsequential since we don't yet have a use-case that needs a
  // device that:
  //
  // a) uses the C++ code,
  // b) has Bluetooth Classic support, and
  // c) plays the role of Discoverer.
  ExceptionOr<Ptr<BluetoothSocket>> bluetooth_socket =
      bluetooth_classic_medium_->connectToService(
          bluetooth_device, generateUUIDFromString(service_name));
  if (!bluetooth_socket.ok()) {
    if (Exception::IO == bluetooth_socket.exception()) {
      // TODO(reznor): log.atSevere().log("Failed to connect via Bluetooth
      // socket to %s.", bluetoothSocketName);
    }
    return Ptr<BluetoothSocket>();
  }

  return bluetooth_socket.result();
}

template <typename Platform>
string BluetoothClassic<Platform>::generateUUIDFromString(const string& data) {
  return UUID<Platform>(data).str();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
