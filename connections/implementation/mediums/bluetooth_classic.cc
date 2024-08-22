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

#include "connections/implementation/mediums/bluetooth_classic.h"

#include <memory>
#include <string>
#include <utility>

#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/implementation/mediums/multiplex/multiplex_socket.h"
#include "connections/medium_selector.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/socket.h"
#include "internal/platform/types.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace connections {

namespace {
std::string ScanModeToString(BluetoothAdapter::ScanMode mode) {
  switch (mode) {
    case BluetoothAdapter::ScanMode::kUnknown:
      return "Unknown";
    case BluetoothAdapter::ScanMode::kNone:
      return "None";
    case BluetoothAdapter::ScanMode::kConnectable:
      return "Connectable";
    case BluetoothAdapter::ScanMode::kConnectableDiscoverable:
      return "ConnectableDiscoverable";
  }
}
}  // namespace

using MultiplexSocket = mediums::multiplex::MultiplexSocket;

BluetoothClassic::BluetoothClassic(BluetoothRadio& radio)
    : BluetoothClassic(radio, std::make_unique<BluetoothClassicMedium>(
                                  radio.GetBluetoothAdapter())) {}

BluetoothClassic::BluetoothClassic(
    BluetoothRadio& radio, std::unique_ptr<BluetoothClassicMedium> medium)
    : radio_(radio),
      adapter_(radio_.GetBluetoothAdapter()),
      medium_(std::move(medium)) {
  is_multiplex_enabled_ = NearbyFlags::GetInstance().GetBoolFlag(
      config_package_nearby::nearby_connections_feature::kEnableMultiplex);
}

BluetoothClassic::~BluetoothClassic() {
  // Destructor is not taking locks, but methods it is calling are.
  StopAllDiscovery();
  while (!server_sockets_.empty()) {
    StopAcceptingConnections(server_sockets_.begin()->first);
  }
  TurnOffDiscoverability();

  {
    MutexLock lock(&mutex_);
    NEARBY_LOGS(INFO) << "Closing multiplex sockets for "
                      << multiplex_sockets_.size() << " devices";
    if (is_multiplex_enabled_) {
      for (auto& [bt_mac, multiplex_socket] : multiplex_sockets_) {
        NEARBY_LOGS(INFO) << "Closing multiplex sockets for "
                          << GetRemoteDevice(bt_mac).GetName();
        multiplex_socket->Shutdown();
      }
    }
    multiplex_sockets_.clear();
  }

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
  return medium_->IsValid() && adapter_.IsValid() && adapter_.IsEnabled();
}

bool BluetoothClassic::TurnOnDiscoverability(const std::string& device_name) {
  NEARBY_LOGS(INFO) << "Turning on BT discoverability with device_name="
                    << device_name;
  MutexLock lock(&mutex_);

  if (device_name.empty()) {
    NEARBY_LOGS(INFO)
        << "Refusing to turn on BT discoverability. Empty device name.";
    return false;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO) << "Can't turn on BT discoverability. BT is off.";
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO)
        << "Can't turn on BT discoverability. BT is not available.";
    return false;
  }

  if (IsDiscoverable()) {
    NEARBY_LOGS(INFO) << "Refusing to turn on BT discoverability; new name='"
                      << device_name << "'; current name='"
                      << adapter_.GetName() << "'";
    return false;
  }

  if (!ModifyDeviceName(device_name)) {
    NEARBY_LOGS(INFO)
        << "Failed to turn on BT discoverability; failed to set name to "
        << device_name;
    return false;
  }

  if (!ModifyScanMode(ScanMode::kConnectableDiscoverable)) {
    NEARBY_LOGS(INFO) << "Failed to turn on BT discoverability; failed to set "
                         "scan_mode to "
                      << ScanModeToString(ScanMode::kConnectableDiscoverable);

    // Don't forget to perform this rollback of the partial state changes we've
    // made til now.
    RestoreDeviceName();
    return false;
  }

  NEARBY_LOGS(INFO) << "Turned on BT discoverability with device_name="
                    << device_name;
  return true;
}

bool BluetoothClassic::TurnOffDiscoverability() {
  NEARBY_LOGS(INFO) << "Turning off Bluetooth discoverability.";
  MutexLock lock(&mutex_);

  if (!IsDiscoverable()) {
    NEARBY_LOGS(INFO) << "Can't turn off BT discoverability; it is already off";
    return false;
  }

  RestoreScanMode();
  RestoreDeviceName();

  NEARBY_LOGS(INFO) << "Turned Bluetooth discoverability off.";
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
  return adapter_.SetName(device_name,
                          /* persist= */ false);
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
    NEARBY_LOGS(INFO) << "Failed to restore original Bluetooth scan mode to "
                      << ScanModeToString(original_scan_mode_);
    return false;
  }

  // Regardless of whether or not we could actually restore the Bluetooth scan
  // mode, reset our relevant state.
  original_scan_mode_ = ScanMode::kUnknown;
  return true;
}

bool BluetoothClassic::RestoreDeviceName() {
  if (original_device_name_.empty() || !adapter_.SetName(original_device_name_,
                                                         /* persis= */ true)) {
    NEARBY_LOGS(INFO) << "Failed to restore original Bluetooth device name to "
                      << original_device_name_;
    return false;
  }
  original_device_name_.clear();
  return true;
}

bool BluetoothClassic::StartDiscovery(const std::string& serviceId,
                                      DiscoveredDeviceCallback callback) {
  MutexLock lock(&mutex_);

  if (serviceId.empty()) {
    NEARBY_LOGS(INFO) << "Refusing to start discovery; service ID is empty.";
    return false;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO) << "Can't discover BT devices because BT isn't enabled.";
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO)
        << "Can't discover BT devices because BT isn't available.";
    return false;
  }

  if (IsDiscoveringLocked(serviceId)) {
    NEARBY_LOGS(INFO)
        << "Refusing to start discovery of BT devices because another "
           "discovery is already in-progress for service_id="
        << serviceId;
    return false;
  }

  if (!HasDiscoveryCallbacks()) {
    BluetoothClassicMedium::DiscoveryCallback medium_callback{
        .device_discovered_cb =
            [this](BluetoothDevice& device) {
              MutexLock lock(&discovery_callbacks_mutex_);
              for (auto& [service_id, callback] : discovery_callbacks_) {
                if (callback.device_discovered_cb) {
                  callback.device_discovered_cb(device);
                }
              }
            },
        .device_name_changed_cb =
            [this](BluetoothDevice& device) {
              MutexLock lock(&discovery_callbacks_mutex_);
              for (auto& [service_id, callback] : discovery_callbacks_) {
                if (callback.device_name_changed_cb) {
                  callback.device_name_changed_cb(device);
                }
              }
            },
        .device_lost_cb =
            [this](BluetoothDevice& device) {
              MutexLock lock(&discovery_callbacks_mutex_);
              for (auto& [service_id, callback] : discovery_callbacks_) {
                if (callback.device_lost_cb) {
                  callback.device_lost_cb(device);
                }
              }
            }};

    AddDiscoveryCallback(serviceId, std::move(callback));

    if (!medium_->StartDiscovery(std::move(medium_callback))) {
      NEARBY_LOGS(INFO) << "Failed to start discovery of BT devices.";
      RemoveDiscoveryCallback(serviceId);
      return false;
    }
  }

  // Mark the fact that we're currently performing a Bluetooth scan.
  scan_info_.valid = true;

  return true;
}

bool BluetoothClassic::StopDiscovery(const std::string& serviceId) {
  MutexLock lock(&mutex_);

  if (!IsDiscoveringLocked(serviceId)) {
    NEARBY_LOGS(INFO)
        << "Can't stop discovery of BT devices because it never started.";
    return false;
  }

  RemoveDiscoveryCallback(serviceId);

  if (!HasDiscoveryCallbacks()) {
    if (!medium_->StopDiscovery()) {
      NEARBY_LOGS(INFO) << "Failed to stop discovery of Bluetooth devices.";
      return false;
    }

    scan_info_.valid = false;
  }
  return true;
}

bool BluetoothClassic::IsDiscoveringLocked(const std::string& serviceId) const {
  MutexLock lock(&discovery_callbacks_mutex_);
  return scan_info_.valid && discovery_callbacks_.contains(serviceId);
}

void BluetoothClassic::StopAllDiscovery() {
  MutexLock lock(&mutex_);
  if (!medium_->StopDiscovery()) {
    NEARBY_LOGS(INFO) << "Failed to stop discovery of Bluetooth devices.";
  }

  RemoveAllDiscoveryCallbacks();
  scan_info_.valid = false;
}

bool BluetoothClassic::StartAcceptingConnections(
    const std::string& service_id, AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    NEARBY_LOGS(INFO)
        << "Refusing to start accepting BT connections; service ID is empty.";
    return false;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO) << "Can't create BT server socket [service=" << service_id
                      << "]; BT is disabled.";
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't start accepting BT connections [service="
                      << service_id << "]; BT not available.";
    return false;
  }

  if (IsAcceptingConnectionsLocked(service_id)) {
    NEARBY_LOGS(INFO)
        << "Refusing to start accepting BT connections [service=" << service_id
        << "]; BT server is already in-progress with the same name.";
    return false;
  }

  BluetoothServerSocket socket =
      medium_->ListenForService(service_id, GenerateUuidFromString(service_id));
  if (!socket.IsValid()) {
    NEARBY_LOGS(INFO) << "Failed to start accepting Bluetooth connections for "
                      << service_id;
    return false;
  }

  // Mark the fact that there's an in-progress Bluetooth server accepting
  // connections.
  auto owned_socket =
      server_sockets_.emplace(service_id, std::move(socket)).first->second;

  if (is_multiplex_enabled_) {
    MultiplexSocket::ListenForIncomingConnection(
        service_id, Medium::BLUETOOTH,
        [&callback](const std::string& listening_service_id,
                    MediumSocket* virtual_socket) mutable {
          if (callback) {
            callback(listening_service_id,
                     *(down_cast<BluetoothSocket*>(virtual_socket)));
          }
        });
  }
  // Start the accept loop on a dedicated thread - this stays alive and
  // listening for new incoming connections until StopAcceptingConnections()
  // is invoked.
  accept_loops_runner_.Execute("bt-accept", [callback = std::move(callback),
                                             server_socket =
                                                 std::move(owned_socket),
                                             service_id, this]() mutable {
    while (true) {
      BluetoothSocket client_socket = server_socket.Accept();
      if (!client_socket.IsValid()) {
        NEARBY_LOGS(INFO) << "Failed to accept connection for " << service_id;
        server_socket.Close();
        break;
      }
      NEARBY_LOGS(INFO) << "Accepted connection for " << service_id;
      bool callback_called = false;
      {
        MutexLock lock(&mutex_);
        if (is_multiplex_enabled_) {
          BluetoothSocket client_socket_bak = client_socket;
          auto physical_socket_ptr =
              std::make_shared<BluetoothSocket>(client_socket_bak);
          MultiplexSocket* multiplex_socket =
              MultiplexSocket::CreateIncomingSocket(physical_socket_ptr,
                                                    service_id);

          if (multiplex_socket != nullptr &&
              multiplex_socket->GetVirtualSocket(service_id)) {
            multiplex_sockets_.emplace(
                client_socket.GetRemoteDevice().GetMacAddress(),
                multiplex_socket);
            MultiplexSocket::StopListeningForIncomingConnection(
                service_id, Medium::BLUETOOTH);
            NEARBY_LOGS(INFO) << "Multiplex virtaul socket created for "
                              << client_socket.GetRemoteDevice().GetName();
            if (callback) {
              callback(service_id,
                       *(down_cast<BluetoothSocket*>(
                           multiplex_socket->GetVirtualSocket(service_id))));
              callback_called = true;
            }
          }
        }
      }
      if (callback && !callback_called) {
        NEARBY_LOGS(INFO) << "Call back triggered for physical socket.";
        callback(service_id, std::move(client_socket));
      }
    }
  });

  return true;
}

bool BluetoothClassic::IsAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);

  return IsAcceptingConnectionsLocked(service_id);
}

bool BluetoothClassic::IsAcceptingConnectionsLocked(
    const std::string& service_id) {
  return server_sockets_.find(service_id) != server_sockets_.end();
}

bool BluetoothClassic::StopAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    NEARBY_LOGS(INFO) << "Unable to stop accepting BT connections because the "
                         "service_id is empty.";
    return false;
  }

  const auto& it = server_sockets_.find(service_id);
  if (it == server_sockets_.end()) {
    NEARBY_LOGS(INFO) << "Can't stop accepting BT connections for "
                      << service_id << " because it was never started.";
    return false;
  }
  if (is_multiplex_enabled_) {
    MultiplexSocket::StopListeningForIncomingConnection(service_id,
                                                        Medium::BLUETOOTH);
  }

  // Closing the BluetoothServerSocket will kick off the suicide of the thread
  // in accept_loops_thread_pool_ that blocks on
  // BluetoothServerSocket.accept(). That may take some time to complete, but
  // there's no particular reason to wait around for it.
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
    NEARBY_LOGS(INFO) << "Failed to close BT server socket for " << service_id;
    return false;
  }

  return true;
}

BluetoothSocket BluetoothClassic::Connect(BluetoothDevice& bluetooth_device,
                                          const std::string& service_id,
                                          CancellationFlag* cancellation_flag) {
  {
    MutexLock lock(&mutex_);
    if (is_multiplex_enabled_) {
      NEARBY_LOGS(INFO) << "multiplex_sockets_ size:"
                        << multiplex_sockets_.size();
      auto it = multiplex_sockets_.find(bluetooth_device.GetMacAddress());
      if (it != multiplex_sockets_.end()) {
        MultiplexSocket* multiplex_socket = it->second;
        if (multiplex_socket->IsEnabled()) {
          auto* virtual_socket =
              multiplex_socket->EstablishVirtualSocket(service_id);
          // Should not happen.
          auto* bluetooth_socket = down_cast<BluetoothSocket*>(virtual_socket);
          if (bluetooth_socket == nullptr) {
            NEARBY_LOGS(INFO)
                << "Failed to cast to BluetoothSocket for " << service_id
                << " with " << bluetooth_device.GetName();
            return BluetoothSocket{};
          }
          return *bluetooth_socket;
        }
      }
    }
  }
  service_id_to_connect_attempts_count_map_[service_id] = 1;
  while (service_id_to_connect_attempts_count_map_[service_id] <=
         kConnectAttemptsLimit) {
    if (cancellation_flag->Cancelled()) {
      NEARBY_LOGS(WARNING)
          << "Attempt #"
          << service_id_to_connect_attempts_count_map_[service_id]
          << ": Cannot start creating client BT socket due to cancel.";
      return BluetoothSocket{};
    }

    auto wrapper_result =
        AttemptToConnect(bluetooth_device, service_id, cancellation_flag);
    NEARBY_LOGS(INFO) << "Attempt #"
                      << service_id_to_connect_attempts_count_map_[service_id]
                      << " to connect: " << wrapper_result.IsValid();
    if (wrapper_result.IsValid()) {
      return wrapper_result;
    }

    service_id_to_connect_attempts_count_map_[service_id]++;
  }

  NEARBY_LOGS(WARNING) << "Giving up after " << kConnectAttemptsLimit
                       << " attempts";
  return BluetoothSocket{};
}

BluetoothSocket BluetoothClassic::AttemptToConnect(
    BluetoothDevice& bluetooth_device, const std::string& service_id,
    CancellationFlag* cancellation_flag) {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "BluetoothClassic::Connect: service_id=" << service_id
                    << ", device=" << &bluetooth_device;
  // Socket to return. To allow for NRVO to work, it has to be a single object.
  BluetoothSocket socket{};

  if (service_id.empty()) {
    NEARBY_LOGS(INFO)
        << "Refusing to create client BT socket because service_id is empty.";
    return socket;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO) << "Can't create client BT socket [service=" << service_id
                      << "]: BT isn't enabled.";
    return socket;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't create client BT socket [service=" << service_id
                      << "]; BT isn't available.";
    return socket;
  }

  socket = medium_->ConnectToService(
      bluetooth_device, GenerateUuidFromString(service_id), cancellation_flag);

  // If the socket isn't valid or if the cancellation flag has fired during
  // `ConnectToService`, return an empty socket. There is no need for a
  // CancellationFlagListener because the attempt logic is not asynchronous.
  if (!socket.IsValid() || cancellation_flag->Cancelled()) {
    NEARBY_LOGS(INFO) << "Failed to Connect via BT [service=" << service_id
                      << "]";
    return BluetoothSocket{};
  }

  if (is_multiplex_enabled_) {
    // New MultiplexSocket but default disabled, should be enabled after
    // negotiated
    auto physical_socket_ptr = std::make_shared<BluetoothSocket>(socket);
    MultiplexSocket* multiplex_socket = MultiplexSocket::CreateOutgoingSocket(
        std::move(physical_socket_ptr), service_id);

    auto* virtual_socket = multiplex_socket->GetVirtualSocket(service_id);
    // Should not happen.
    auto* bluetooth_socket = down_cast<BluetoothSocket*>(virtual_socket);
    if (bluetooth_socket == nullptr) {
      NEARBY_LOGS(INFO) << "Failed to cast to BluetoothSocket for "
                        << service_id << " with " << bluetooth_device.GetName();
      return BluetoothSocket{};
    }
    NEARBY_LOGS(INFO) << "Multiplex socket created for "
                      << bluetooth_device.GetName();
    multiplex_sockets_.emplace(bluetooth_device.GetMacAddress(),
                               multiplex_socket);
    return *bluetooth_socket;
  }

  return socket;
}

bool BluetoothClassic::HasDiscoveryCallbacks() const {
  MutexLock lock(&discovery_callbacks_mutex_);
  return !discovery_callbacks_.empty();
}

void BluetoothClassic::RemoveDiscoveryCallback(const std::string& service_id) {
  MutexLock lock(&discovery_callbacks_mutex_);
  if (discovery_callbacks_.contains(service_id)) {
    discovery_callbacks_.erase(service_id);
  }
}
void BluetoothClassic::AddDiscoveryCallback(const std::string& service_id,
                                            DiscoveredDeviceCallback callback) {
  MutexLock lock(&discovery_callbacks_mutex_);
  discovery_callbacks_.insert({service_id, std::move(callback)});
}

void BluetoothClassic::RemoveAllDiscoveryCallbacks() {
  MutexLock lock(&discovery_callbacks_mutex_);
  discovery_callbacks_.clear();
}

BluetoothDevice BluetoothClassic::GetRemoteDevice(
    const std::string& mac_address) {
  MutexLock lock(&mutex_);

  if (!IsAvailableLocked()) {
    return {};
  }

  return medium_->GetRemoteDevice(mac_address);
}

bool BluetoothClassic::IsDiscovering(const std::string& serviceId) const {
  MutexLock lock(&mutex_);
  return IsDiscoveringLocked(serviceId);
  ;
}

std::string BluetoothClassic::GetMacAddress() const {
  MutexLock lock(&mutex_);

  if (!IsAvailableLocked()) {
    return {};
  }

  return medium_->GetMacAddress();
}

std::string BluetoothClassic::GenerateUuidFromString(const std::string& data) {
  return std::string(Uuid(data));
}

}  // namespace connections
}  // namespace nearby
