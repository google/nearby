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

#include "core_v2/internal/mediums/bluetooth_classic.h"

#include <memory>
#include <string>
#include <utility>

#include "core_v2/internal/mediums/uuid.h"
#include "platform_v2/public/logging.h"
#include "platform_v2/public/mutex_lock.h"

namespace location {
namespace nearby {
namespace connections {

BluetoothClassic::BluetoothClassic(BluetoothRadio& radio) : radio_(radio) {}

BluetoothClassic::~BluetoothClassic() {
  // Destructor is not taking locks, but methods it is calling are.
  StopDiscovery();
  while (!server_sockets_.empty()) {
    StopAcceptingConnections(server_sockets_.begin()->first);
  }
  TurnOffDiscoverability();

  // All the AcceptLoopRunnable objects in here should already have gotten an
  // opportunity to shut themselves down cleanly in the calls to
  // StopAcceptingConnections() above.
  accept_loops_runner_.Shutdown();
}

bool BluetoothClassic::IsAvailable() const {
  MutexLock lock(&mutex_);

  return IsAvailableLocked();
}

bool BluetoothClassic::IsAvailableLocked() const {
  return medium_.IsValid() && adapter_.IsValid();
}

bool BluetoothClassic::TurnOnDiscoverability(const std::string& device_name) {
  MutexLock lock(&mutex_);

  if (device_name.empty()) {
    NEARBY_LOG(INFO,
               "Refusing to turn on BT discoverability. Empty device name.");
    return false;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOG(INFO, "Can't turn on BT discoverability. BT is off.");
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOG(INFO, "Can't turn on BT discoverability. BT is not available.");
    return false;
  }

  if (IsDiscoverable()) {
    NEARBY_LOG(INFO,
               "Refusing to turn on BT discoverability; new name='%s'; "
               "current name='%s'",
               device_name.c_str(), adapter_.GetName().c_str());
    return false;
  }

  if (!ModifyDeviceName(device_name)) {
    NEARBY_LOG(INFO,
               "Failed to turn on BT discoverability; "
               "failed to set name to %s",
               device_name.c_str());
    return false;
  }

  if (!ModifyScanMode(ScanMode::kConnectableDiscoverable)) {
    NEARBY_LOG(INFO,
               "Failed to turn on BT discoverability; "
               "failed to set scan_mode to %d",
               ScanMode::kConnectableDiscoverable);

    // Don't forget to perform this rollback of the partial state changes we've
    // made til now.
    RestoreDeviceName();
    return false;
  }

  NEARBY_LOG(INFO, "Turned on BT discoverability with device_name=%s",
             device_name.c_str());
  return true;
}

bool BluetoothClassic::TurnOffDiscoverability() {
  MutexLock lock(&mutex_);

  if (!IsDiscoverable()) {
    NEARBY_LOG(INFO, "Can't turn off BT discoverability; it is already off");
    return false;
  }

  RestoreScanMode();
  RestoreDeviceName();

  NEARBY_LOG(INFO, "Turned Bluetooth discoverability off");
  return true;
}

bool BluetoothClassic::IsDiscoverable() const {
  return (!original_device_name_.empty() &&
          (adapter_.GetScanMode() == ScanMode::kConnectableDiscoverable));
}

bool BluetoothClassic::ModifyDeviceName(const std::string& device_name) {
  if (original_device_name_.empty()) {
    original_device_name_ = adapter_.GetName();
  }

  return adapter_.SetName(device_name);
}

bool BluetoothClassic::ModifyScanMode(ScanMode scan_mode) {
  if (original_scan_mode_ == ScanMode::kUnknown) {
    original_scan_mode_ = adapter_.GetScanMode();
  }

  if (!adapter_.SetScanMode(scan_mode)) {
    original_scan_mode_ = ScanMode::kUnknown;
    return false;
  }

  return true;
}

bool BluetoothClassic::RestoreScanMode() {
  if (original_scan_mode_ == ScanMode::kUnknown ||
      !adapter_.SetScanMode(original_scan_mode_)) {
    NEARBY_LOG(INFO, "Failed to restore original Bluetooth scan mode to %d",
               original_scan_mode_);
    return false;
  }

  // Regardless of whether or not we could actually restore the Bluetooth scan
  // mode, reset our relevant state.
  original_scan_mode_ = ScanMode::kUnknown;
  return true;
}

bool BluetoothClassic::RestoreDeviceName() {
  if (original_device_name_.empty() ||
      !adapter_.SetName(original_device_name_)) {
    NEARBY_LOG(INFO, "Failed to restore original Bluetooth device name to %s",
               original_device_name_.c_str());
    return false;
  }
  original_device_name_.clear();
  return true;
}

bool BluetoothClassic::StartDiscovery(DiscoveredDeviceCallback callback) {
  MutexLock lock(&mutex_);

  if (!radio_.IsEnabled()) {
    NEARBY_LOG(INFO, "Can't discover BT devices because BT isn't enabled.");
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOG(INFO, "Can't discover BT devices because BT isn't available.");
    return false;
  }

  if (IsDiscovering()) {
    NEARBY_LOG(INFO,
               "Refusing to start discovery of BT devices because another "
               "discovery is already in-progress.");
    return false;
  }

  if (!medium_.StartDiscovery(callback)) {
    NEARBY_LOG(INFO, "Failed to start discovery of BT devices.");
    return false;
  }

  // Mark the fact that we're currently performing a Bluetooth scan.
  scan_info_.valid = true;

  return true;
}

bool BluetoothClassic::StopDiscovery() {
  MutexLock lock(&mutex_);

  if (!IsDiscovering()) {
    NEARBY_LOG(INFO,
               "Can't stop discovery of BT devices because it never started.");
    return false;
  }

  if (!medium_.StopDiscovery()) {
    NEARBY_LOG(INFO, "Failed to stop discovery of Bluetooth devices.");
    return false;
  }

  scan_info_.valid = false;
  return true;
}

bool BluetoothClassic::IsDiscovering() const { return scan_info_.valid; }

bool BluetoothClassic::StartAcceptingConnections(
    const std::string& service_name, AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);

  if (service_name.empty()) {
    NEARBY_LOG(
        INFO,
        "Refusing to start accepting BT connections; service name is empty.");
    return false;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOG(INFO,
               "Can't create BT server socket [service=%s]; BT is disabled.",
               service_name.c_str());
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOG(
        INFO,
        "Can't start accepting BT connections [service=%s]; BT not available.",
        service_name.c_str());
    return false;
  }

  if (IsAcceptingConnectionsLocked(service_name)) {
    NEARBY_LOG(INFO,
               "Refusing to start accepting BT connections [service=%s]; BT "
               "server is already in-progress with the same name.",
               service_name.c_str());
    return false;
  }

  BluetoothServerSocket socket = medium_.ListenForService(
      service_name, GenerateUuidFromString(service_name));
  if (!socket.IsValid()) {
    NEARBY_LOG(INFO, "Failed to start accepting Bluetooth connections for %s.",
               service_name.c_str());
    return false;
  }

  // Mark the fact that there's an in-progress Bluetooth server accepting
  // connections.
  auto owned_socket =
      server_sockets_.emplace(service_name, std::move(socket)).first->second;

  // Start the accept loop on a dedicated thread - this stays alive and
  // listening for new incoming connections until StopAcceptingConnections() is
  // invoked.
  accept_loops_runner_.Execute([callback = std::move(callback),
                                server_socket = std::move(owned_socket),
                                service_name]() mutable {
    while (true) {
      BluetoothSocket client_socket = server_socket.Accept();
      if (!client_socket.IsValid()) {
        server_socket.Close();
        break;
      }

      callback.accepted_cb(std::move(client_socket));
    }
  });

  return true;
}

bool BluetoothClassic::IsAcceptingConnections(const std::string& service_name) {
  MutexLock lock(&mutex_);

  return IsAcceptingConnectionsLocked(service_name);
}

bool BluetoothClassic::IsAcceptingConnectionsLocked(
    const std::string& service_name) {
  return server_sockets_.find(service_name) != server_sockets_.end();
}

bool BluetoothClassic::StopAcceptingConnections(
    const std::string& service_name) {
  MutexLock lock(&mutex_);

  if (service_name.empty()) {
    NEARBY_LOG(INFO,
               "Unable to stop accepting BT connections because the "
               "service_name is empty.");
    return false;
  }

  const auto& it = server_sockets_.find(service_name);
  if (it == server_sockets_.end()) {
    NEARBY_LOG(INFO,
               "Can't stop accepting BT connections for %s because it was "
               "never started.",
               service_name.c_str());
    return false;
  }

  // Closing the BluetoothServerSocket will kick off the suicide of the thread
  // in accept_loops_thread_pool_ that blocks on BluetoothServerSocket.accept().
  // That may take some time to complete, but there's no particular reason to
  // wait around for it.
  auto item = server_sockets_.extract(it);

  // Store a handle to the BluetoothServerSocket, so we can use it after
  // removing the entry from server_sockets_; making it scoped
  // is a bonus that takes care of deallocation before we leave this method.
  BluetoothServerSocket& listening_socket = item.mapped();

  // Regardless of whether or not we fail to close the existing
  // BluetoothServerSocket, remove it from server_sockets_ so that it
  // frees up this service for another round.

  // Finally, close the BluetoothServerSocket.
  if (!listening_socket.Close().Ok()) {
    NEARBY_LOG(INFO, "Failed to close BT server socket for %s.",
               service_name.c_str());
    return false;
  }

  return true;
}

BluetoothSocket BluetoothClassic::Connect(BluetoothDevice& bluetooth_device,
                                          const std::string& service_name) {
  MutexLock lock(&mutex_);
  NEARBY_LOG(INFO, "BluetoothClassic::Connect: device=%p", &bluetooth_device);
  // Socket to return. To allow for NRVO to work, it has to be a single object.
  BluetoothSocket socket;

  if (service_name.empty()) {
    NEARBY_LOG(
        INFO,
        "Refusing to create client BT socket because service_name is empty.");
    return socket;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOG(INFO,
               "Can't create client BT socket [service=%s]: BT isn't enabled.",
               service_name.c_str());
    return socket;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOG(
        INFO, "Can't create client BT socket [service=%s]; BT isn't available.",
        service_name.c_str());
    return socket;
  }

  socket = medium_.ConnectToService(bluetooth_device,
                                    GenerateUuidFromString(service_name));
  if (!socket.IsValid()) {
    NEARBY_LOG(INFO, "Failed to Connect via BT [service=%s]",
               service_name.c_str());
  }

  return socket;
}

BluetoothDevice BluetoothClassic::GetRemoteDevice(
    const std::string& mac_address) {
  MutexLock lock(&mutex_);
  return medium_.GetRemoteDevice(mac_address);
}

std::string BluetoothClassic::GetMacAddress() const {
  MutexLock lock(&mutex_);
  return medium_.GetMacAddress();
}

std::string BluetoothClassic::GenerateUuidFromString(const std::string& data) {
  return std::string(Uuid(data));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
