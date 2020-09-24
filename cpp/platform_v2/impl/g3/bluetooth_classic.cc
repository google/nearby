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

#include "platform_v2/impl/g3/bluetooth_classic.h"

#include <memory>
#include <string>

#include "platform_v2/api/bluetooth_classic.h"
#include "platform_v2/base/logging.h"
#include "platform_v2/base/medium_environment.h"
#include "platform_v2/impl/g3/bluetooth_adapter.h"
#include "absl/synchronization/mutex.h"

namespace location {
namespace nearby {
namespace g3 {

BluetoothSocket::~BluetoothSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

void BluetoothSocket::Connect(BluetoothSocket& other) {
  absl::MutexLock lock(&mutex_);
  remote_socket_ = &other;
  input_ = other.output_;
}

bool BluetoothSocket::IsConnected() const {
  absl::MutexLock lock(&mutex_);
  return IsConnectedLocked();
}

bool BluetoothSocket::IsClosed() const {
  absl::MutexLock lock(&mutex_);
  return closed_;
}

bool BluetoothSocket::IsConnectedLocked() const { return input_ != nullptr; }

InputStream& BluetoothSocket::GetInputStream() {
  auto* remote_socket = GetRemoteSocket();
  CHECK(remote_socket != nullptr);
  return remote_socket->GetLocalInputStream();
}

OutputStream& BluetoothSocket::GetOutputStream() {
  return GetLocalOutputStream();
}

InputStream& BluetoothSocket::GetLocalInputStream() {
  absl::MutexLock lock(&mutex_);
  return output_->GetInputStream();
}

OutputStream& BluetoothSocket::GetLocalOutputStream() {
  absl::MutexLock lock(&mutex_);
  return output_->GetOutputStream();
}

Exception BluetoothSocket::Close() {
  absl::MutexLock lock(&mutex_);
  DoClose();
  return {Exception::kSuccess};
}

void BluetoothSocket::DoClose() {
  if (!closed_) {
    remote_socket_ = nullptr;
    output_->GetOutputStream().Close();
    output_->GetInputStream().Close();
    input_->GetOutputStream().Close();
    input_->GetInputStream().Close();
    closed_ = true;
  }
}

BluetoothSocket* BluetoothSocket::GetRemoteSocket() {
  absl::MutexLock lock(&mutex_);
  return remote_socket_;
}

BluetoothDevice* BluetoothSocket::GetRemoteDevice() {
  BluetoothAdapter* remote_adapter = nullptr;
  {
    absl::MutexLock lock(&mutex_);
    if (remote_socket_ == nullptr || remote_socket_->adapter_ == nullptr) {
      return nullptr;
    }
    remote_adapter = remote_socket_->adapter_;
  }
  return remote_adapter ? &remote_adapter->GetDevice() : nullptr;
}

std::unique_ptr<api::BluetoothSocket> BluetoothServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  while (pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
    if (closed_) break;
  }
  // whether or not we were running in the wait loop, return early if closed.
  if (closed_) return {};
  auto* remote_socket =
      pending_sockets_.extract(pending_sockets_.begin()).value();
  CHECK(remote_socket);
  auto local_socket = std::make_unique<BluetoothSocket>(adapter_);
  local_socket->Connect(*remote_socket);
  remote_socket->Connect(*local_socket);
  cond_.SignalAll();
  return local_socket;
}

bool BluetoothServerSocket::Connect(BluetoothSocket& socket) {
  absl::MutexLock lock(&mutex_);
  if (closed_) return false;
  if (socket.IsConnected()) {
    NEARBY_LOG(ERROR,
               "Failed to connect to BT server socket: already connected");
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

void BluetoothServerSocket::SetCloseNotifier(std::function<void()> notifier) {
  absl::MutexLock lock(&mutex_);
  close_notifier_ = std::move(notifier);
}

BluetoothServerSocket::~BluetoothServerSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

Exception BluetoothServerSocket::Close() {
  absl::MutexLock lock(&mutex_);
  return DoClose();
}

Exception BluetoothServerSocket::DoClose() {
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

BluetoothClassicMedium::BluetoothClassicMedium(api::BluetoothAdapter& adapter)
    // TODO(apolyudov): implement and use downcast<> with static assertions.
    : adapter_(static_cast<BluetoothAdapter*>(&adapter)) {
  adapter_->SetBluetoothClassicMedium(this);
  auto& env = MediumEnvironment::Instance();
  env.RegisterBluetoothMedium(*this, GetAdapter());
}

BluetoothClassicMedium::~BluetoothClassicMedium() {
  adapter_->SetBluetoothClassicMedium(nullptr);
  auto& env = MediumEnvironment::Instance();
  env.UnregisterBluetoothMedium(*this);
}

bool BluetoothClassicMedium::StartDiscovery(DiscoveryCallback callback) {
  auto& env = MediumEnvironment::Instance();
  env.UpdateBluetoothMedium(*this, std::move(callback));
  return true;
}

bool BluetoothClassicMedium::StopDiscovery() {
  auto& env = MediumEnvironment::Instance();
  env.UpdateBluetoothMedium(*this, {});
  return true;
}

std::unique_ptr<api::BluetoothSocket> BluetoothClassicMedium::ConnectToService(
    api::BluetoothDevice& remote_device, const std::string& service_uuid) {
  NEARBY_LOG(INFO,
             "G3 ConnectToService [self]: medium=%p, adapter=%p, device=%p",
             this, &GetAdapter(), &GetAdapter().GetDevice());
  // First, find an instance of remote medium, that exposed this device.
  auto& adapter = static_cast<BluetoothDevice&>(remote_device).GetAdapter();
  auto* medium =
      static_cast<BluetoothClassicMedium*>(adapter.GetBluetoothClassicMedium());

  if (!medium) return {};  // Adapter is not bound to medium. Bail out.

  BluetoothServerSocket* server_socket = nullptr;
  NEARBY_LOG(
      INFO,
      "G3 ConnectToService [peer]: medium=%p, adapter=%p, device=%p, uuid=%s",
      medium, &adapter, &remote_device, service_uuid.c_str());
  // Then, find our server socket context in this medium.
  {
    absl::MutexLock medium_lock(&medium->mutex_);
    auto item = medium->sockets_.find(service_uuid);
    server_socket = item != sockets_.end() ? item->second : nullptr;
    if (server_socket == nullptr) {
      NEARBY_LOG(ERROR, "Failed to find BT Server socket: uuid=%s",
                 service_uuid.c_str());
      return {};
    }
  }

  auto socket = std::make_unique<BluetoothSocket>(&GetAdapter());
  // Finally, Request to connect to this socket.
  if (!server_socket->Connect(*socket)) {
    NEARBY_LOG(ERROR, "Failed to connect to existing BT Server socket: uuid=%s",
               service_uuid.c_str());
    return {};
  }

  NEARBY_LOG(INFO, "G3 ConnectToService: connected: socket=%p", socket.get());
  return socket;
}

std::unique_ptr<api::BluetoothServerSocket>
BluetoothClassicMedium::ListenForService(const std::string& service_name,
                                         const std::string& service_uuid) {
  auto socket = std::make_unique<BluetoothServerSocket>(GetAdapter());
  socket->SetCloseNotifier([this, uuid = service_uuid]() {
    absl::MutexLock lock(&mutex_);
    sockets_.erase(uuid);
  });
  NEARBY_LOG(INFO, "Adding service: medium=%p, uuid=%s", this,
             service_uuid.c_str());
  absl::MutexLock lock(&mutex_);
  sockets_.emplace(service_uuid, socket.get());
  return socket;
}

api::BluetoothDevice* BluetoothClassicMedium::GetRemoteDevice(
    const std::string& mac_address) {
  auto& env = MediumEnvironment::Instance();
  return env.FindBluetoothDevice(mac_address);
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
