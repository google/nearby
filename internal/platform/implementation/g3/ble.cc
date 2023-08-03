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

#include "internal/platform/implementation/g3/ble.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/g3/bluetooth_adapter.h"
#include "internal/platform/implementation/shared/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace g3 {

BlePeripheral* BleSocket::GetRemotePeripheral() {
  absl::MutexLock lock(&mutex_);
  return peripheral_;
}

std::unique_ptr<api::BleSocket> BleServerSocket::Accept(
    BlePeripheral* peripheral) {
  absl::MutexLock lock(&mutex_);
  if (closed_) return {};
  while (pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
    if (closed_) break;
  }
  if (closed_) return {};
  auto* remote_socket =
      pending_sockets_.extract(pending_sockets_.begin()).value();
  CHECK(remote_socket);
  auto local_socket = std::make_unique<BleSocket>(peripheral);
  local_socket->Connect(*remote_socket);
  remote_socket->Connect(*local_socket);
  cond_.SignalAll();
  return local_socket;
}

bool BleServerSocket::Connect(BleSocket& socket) {
  absl::MutexLock lock(&mutex_);
  if (closed_) return false;
  if (socket.IsConnected()) {
    NEARBY_LOG(ERROR,
               "Failed to connect to Ble server socket: already connected");
    return true;  // already connected.
  }
  // add client socket to the pending list
  pending_sockets_.emplace(&socket);
  cond_.SignalAll();
  while (!socket.IsConnected()) {
    cond_.Wait(&mutex_);
    if (closed_) return false;
  }
  return true;
}

void BleServerSocket::SetCloseNotifier(absl::AnyInvocable<void()> notifier) {
  absl::MutexLock lock(&mutex_);
  close_notifier_ = std::move(notifier);
}

BleServerSocket::~BleServerSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

Exception BleServerSocket::Close() {
  absl::MutexLock lock(&mutex_);
  return DoClose();
}

Exception BleServerSocket::DoClose() {
  bool should_notify = !closed_;
  closed_ = true;
  if (should_notify) {
    cond_.SignalAll();
    if (close_notifier_) {
      auto notifier = std::move(close_notifier_);
      mutex_.Unlock();
      // Notifier may contain calls to public API, and may cause deadlock, if
      // mutex_ is held during the call.
      notifier();
      mutex_.Lock();
    }
  }
  return {Exception::kSuccess};
}

BleMedium::BleMedium(api::BluetoothAdapter& adapter)
    : adapter_(static_cast<BluetoothAdapter*>(&adapter)) {
  adapter_->SetBleMedium(this);
  auto& env = MediumEnvironment::Instance();
  env.RegisterBleMedium(*this);
}

BleMedium::~BleMedium() {
  adapter_->SetBleMedium(nullptr);
  auto& env = MediumEnvironment::Instance();
  env.UnregisterBleMedium(*this);

  StopAdvertising(advertising_info_.service_id);
  StopScanning(scanning_info_.service_id);

  accept_loops_runner_.Shutdown();
  NEARBY_LOG(INFO, "BleMedium dtor advertising_accept_thread_running_ = %d",
             acceptance_thread_running_.load());
  // If acceptance thread is still running, wait to finish.
  if (acceptance_thread_running_) {
    while (acceptance_thread_running_) {
      shared::CountDownLatch latch(1);
      close_accept_loops_runner_.Execute([&latch]() { latch.CountDown(); });
      latch.Await();
    }
  }
}

bool BleMedium::StartAdvertising(
    const std::string& service_id, const ByteArray& advertisement_bytes,
    const std::string& fast_advertisement_service_uuid) {
  NEARBY_LOGS(INFO) << "G3 Ble StartAdvertising: service_id=" << service_id
                    << ", advertisement bytes=" << advertisement_bytes.data()
                    << "(" << advertisement_bytes.size() << "),"
                    << " fast advertisement service uuid="
                    << fast_advertisement_service_uuid;
  auto& env = MediumEnvironment::Instance();
  auto& peripheral = adapter_->GetPeripheral();
  peripheral.SetAdvertisementBytes(service_id, advertisement_bytes);
  bool fast_advertisement = !fast_advertisement_service_uuid.empty();
  env.UpdateBleMediumForAdvertising(*this, peripheral, service_id,
                                    fast_advertisement, true);

  absl::MutexLock lock(&mutex_);
  if (server_socket_ != nullptr) server_socket_.release();
  server_socket_ = std::make_unique<BleServerSocket>();

  acceptance_thread_running_.exchange(true);
  accept_loops_runner_.Execute([&env, this, service_id]() mutable {
    if (!accept_loops_runner_.InShutdown()) {
      while (true) {
        auto client_socket =
            server_socket_->Accept(&(this->adapter_->GetPeripheral()));
        if (client_socket == nullptr) break;
        env.CallBleAcceptedConnectionCallback(*this, *(client_socket.release()),
                                              service_id);
      }
    }
    acceptance_thread_running_.exchange(false);
  });
  advertising_info_.service_id = service_id;
  return true;
}

bool BleMedium::StopAdvertising(const std::string& service_id) {
  NEARBY_LOGS(INFO) << "G3 Ble StopAdvertising: service_id=" << service_id;
  {
    absl::MutexLock lock(&mutex_);
    if (advertising_info_.Empty()) {
      NEARBY_LOGS(INFO) << "G3 Ble StopAdvertising: Can't stop advertising "
                           "because we never started advertising.";
      return false;
    }
    advertising_info_.Clear();
  }

  auto& env = MediumEnvironment::Instance();
  env.UpdateBleMediumForAdvertising(*this, adapter_->GetPeripheral(),
                                    service_id, /*fast_advertisement=*/false,
                                    /*enabled=*/false);
  accept_loops_runner_.Shutdown();
  if (server_socket_ == nullptr) {
    NEARBY_LOGS(ERROR) << "G3 Ble StopAdvertising: Failed to find Ble Server "
                          "socket: service_id="
                       << service_id;
    // Fall through for server socket not found.
    return true;
  }

  if (!server_socket_->Close().Ok()) {
    NEARBY_LOGS(INFO)
        << "G3 Ble StopAdvertising: Failed to close Ble server socket for "
        << service_id;
    return false;
  }
  return true;
}

bool BleMedium::StartScanning(
    const std::string& service_id,
    const std::string& fast_advertisement_service_uuid,
    DiscoveredPeripheralCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Ble StartScanning: service_id=" << service_id;
  auto& env = MediumEnvironment::Instance();
  env.UpdateBleMediumForScanning(*this, service_id,
                                 fast_advertisement_service_uuid,
                                 std::move(callback), true);
  {
    absl::MutexLock lock(&mutex_);
    scanning_info_.service_id = service_id;
  }
  return true;
}

bool BleMedium::StopScanning(const std::string& service_id) {
  NEARBY_LOGS(INFO) << "G3 Ble StopScanning: service_id=" << service_id;
  {
    absl::MutexLock lock(&mutex_);
    if (scanning_info_.Empty()) {
      NEARBY_LOGS(INFO) << "G3 Ble StopDiscovery: Can't stop scanning because "
                           "we never started scanning.";
      return false;
    }
    scanning_info_.Clear();
  }

  auto& env = MediumEnvironment::Instance();
  env.UpdateBleMediumForScanning(*this, service_id, {}, {}, false);
  return true;
}

bool BleMedium::StartAcceptingConnections(const std::string& service_id,
                                          AcceptedConnectionCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Ble StartAcceptingConnections: service_id="
                    << service_id;
  auto& env = MediumEnvironment::Instance();
  env.UpdateBleMediumForAcceptedConnection(*this, service_id,
                                           std::move(callback));
  return true;
}

bool BleMedium::StopAcceptingConnections(const std::string& service_id) {
  NEARBY_LOGS(INFO) << "G3 Ble StopAcceptingConnections: service_id="
                    << service_id;
  auto& env = MediumEnvironment::Instance();
  env.UpdateBleMediumForAcceptedConnection(*this, service_id, {});
  return true;
}

std::unique_ptr<api::BleSocket> BleMedium::Connect(
    api::BlePeripheral& remote_peripheral, const std::string& service_id,
    CancellationFlag* cancellation_flag) {
  NEARBY_LOG(INFO,
             "G3 Ble Connect [self]: medium=%p, adapter=%p, peripheral=%p, "
             "service_id=%s",
             this, &GetAdapter(), &GetAdapter().GetPeripheral(),
             service_id.c_str());
  // First, find an instance of remote medium, that exposed this peripheral.
  auto& adapter = static_cast<BlePeripheral&>(remote_peripheral).GetAdapter();
  auto* medium = static_cast<BleMedium*>(adapter.GetBleMedium());

  if (!medium) return {};  // Can't find medium. Bail out.

  BleServerSocket* remote_server_socket = nullptr;
  NEARBY_LOG(INFO,
             "G3 Ble Connect [peer]: medium=%p, adapter=%p, peripheral=%p, "
             "service_id=%s",
             medium, &adapter, &remote_peripheral, service_id.c_str());
  // Then, find our server socket context in this medium.
  {
    absl::MutexLock medium_lock(&medium->mutex_);
    remote_server_socket = medium->server_socket_.get();
    if (remote_server_socket == nullptr) {
      NEARBY_LOGS(ERROR)
          << "G3 Ble Connect: Failed to find Ble Server socket: service_id="
          << service_id;
      return {};
    }
  }

  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(ERROR) << "G3 BLE Connect: Has been cancelled: "
                          "service_id="
                       << service_id;
    return {};
  }

  CancellationFlagListener listener(cancellation_flag, [this]() {
    NEARBY_LOGS(INFO) << "G3 BLE Cancel Connect.";
    if (server_socket_ != nullptr) server_socket_->Close();
  });

  BlePeripheral peripheral = static_cast<BlePeripheral&>(remote_peripheral);
  auto socket = std::make_unique<BleSocket>(&peripheral);
  // Finally, Request to connect to this socket.
  if (!remote_server_socket->Connect(*socket)) {
    NEARBY_LOGS(ERROR) << "G3 Ble Connect: Failed to connect to existing Ble "
                          "Server socket: service_id="
                       << service_id;
    return {};
  }

  NEARBY_LOG(INFO, "G3 Ble Connect: connected: socket=%p", socket.get());
  return socket;
}

}  // namespace g3
}  // namespace nearby
