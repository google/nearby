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

#include "platform/impl/g3/wifi_lan.h"

#include <iostream>
#include <memory>
#include <string>

#include "platform/api/wifi_lan.h"
#include "platform/base/logging.h"
#include "platform/base/medium_environment.h"
#include "platform/base/nsd_service_info.h"
#include "platform/base/prng.h"
#include "absl/synchronization/mutex.h"

namespace location {
namespace nearby {
namespace g3 {

WifiLanSocket::~WifiLanSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

void WifiLanSocket::Connect(WifiLanSocket& other) {
  absl::MutexLock lock(&mutex_);
  remote_socket_ = &other;
  input_ = other.output_;
}

InputStream& WifiLanSocket::GetInputStream() {
  auto* remote_socket = GetRemoteSocket();
  CHECK(remote_socket != nullptr);
  return remote_socket->GetLocalInputStream();
}

OutputStream& WifiLanSocket::GetOutputStream() {
  return GetLocalOutputStream();
}

WifiLanSocket* WifiLanSocket::GetRemoteSocket() {
  absl::MutexLock lock(&mutex_);
  return remote_socket_;
}

bool WifiLanSocket::IsConnected() const {
  absl::MutexLock lock(&mutex_);
  return IsConnectedLocked();
}

bool WifiLanSocket::IsClosed() const {
  absl::MutexLock lock(&mutex_);
  return closed_;
}

Exception WifiLanSocket::Close() {
  absl::MutexLock lock(&mutex_);
  DoClose();
  return {Exception::kSuccess};
}

WifiLanService* WifiLanSocket::GetRemoteWifiLanService() {
  absl::MutexLock lock(&mutex_);
  return wifi_lan_service_;
}

void WifiLanSocket::DoClose() {
  if (!closed_) {
    remote_socket_ = nullptr;
    output_->GetOutputStream().Close();
    output_->GetInputStream().Close();
    if (IsConnectedLocked()) {
      input_->GetOutputStream().Close();
      input_->GetInputStream().Close();
    }
    closed_ = true;
  }
}

bool WifiLanSocket::IsConnectedLocked() const { return input_ != nullptr; }

InputStream& WifiLanSocket::GetLocalInputStream() {
  absl::MutexLock lock(&mutex_);
  return output_->GetInputStream();
}

OutputStream& WifiLanSocket::GetLocalOutputStream() {
  absl::MutexLock lock(&mutex_);
  return output_->GetOutputStream();
}

std::unique_ptr<api::WifiLanSocket> WifiLanServerSocket::Accept(
    WifiLanService* wifi_lan_service) {
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
  auto local_socket = std::make_unique<WifiLanSocket>(wifi_lan_service);
  local_socket->Connect(*remote_socket);
  remote_socket->Connect(*local_socket);
  cond_.SignalAll();
  return local_socket;
}

bool WifiLanServerSocket::Connect(WifiLanSocket& socket) {
  absl::MutexLock lock(&mutex_);
  if (closed_) return false;
  if (socket.IsConnected()) {
    NEARBY_LOG(ERROR,
               "Failed to connect to WifiLan server socket: already connected");
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

void WifiLanServerSocket::SetCloseNotifier(std::function<void()> notifier) {
  absl::MutexLock lock(&mutex_);
  close_notifier_ = std::move(notifier);
}

WifiLanServerSocket::~WifiLanServerSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

Exception WifiLanServerSocket::Close() {
  absl::MutexLock lock(&mutex_);
  return DoClose();
}

Exception WifiLanServerSocket::DoClose() {
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

WifiLanMedium::WifiLanMedium() {
  wifi_lan_service_.SetMedium(this);
  auto& env = MediumEnvironment::Instance();
  env.RegisterWifiLanMedium(*this);
}

WifiLanMedium::~WifiLanMedium() {
  wifi_lan_service_.SetMedium(nullptr);
  auto& env = MediumEnvironment::Instance();
  env.UnregisterWifiLanMedium(*this);

  StopAdvertising(advertising_info_.service_id);
  StopDiscovery(discovering_info_.service_id);

  NEARBY_LOG(INFO, "WifiLanMedium dtor advertising_accept_thread_running_ = %d",
             acceptance_thread_running_.load());
  // If acceptance thread is still running, wait to finish.
  if (acceptance_thread_running_) {
    while (acceptance_thread_running_) {
      CountDownLatch latch(1);
      close_accept_loops_runner_.Execute([&latch]() { latch.CountDown(); });
      latch.Await();
    }
  }
}

bool WifiLanMedium::StartAdvertising(const std::string& service_id,
                                     const NsdServiceInfo& nsd_service_info) {
  NEARBY_LOG(INFO,
             "G3 WifiLan StartAdvertising: service_id=%s, nsd_service_info=%p, "
             "service_info_name=%s",
             service_id.c_str(), &nsd_service_info,
             nsd_service_info.GetServiceInfoName().c_str());
  auto& env = MediumEnvironment::Instance();
  NsdServiceInfo local_nsd_service_info{nsd_service_info};
  SetWifiLanService(nsd_service_info);
  env.UpdateWifiLanMediumForAdvertising(*this, wifi_lan_service_, service_id,
                                        true);

  absl::MutexLock lock(&mutex_);
  if (server_socket_ != nullptr) server_socket_.release();
  server_socket_ = std::make_unique<WifiLanServerSocket>();

  acceptance_thread_running_.exchange(true);
  accept_loops_runner_.Execute([&env, this, service_id]() mutable {
    if (!accept_loops_runner_.InShutdown()) {
      while (true) {
        auto client_socket = server_socket_->Accept(&wifi_lan_service_);
        if (client_socket == nullptr) break;
        env.CallWifiLanAcceptedConnectionCallback(
            *this, *(client_socket.release()), service_id);
      }
    }
    acceptance_thread_running_.exchange(false);
  });
  advertising_info_.service_id = service_id;
  return true;
}

bool WifiLanMedium::StopAdvertising(const std::string& service_id) {
  NEARBY_LOG(INFO, "G3 WifiLan StopAdvertising: service_id=%s",
             service_id.c_str());
  {
    absl::MutexLock lock(&mutex_);
    if (advertising_info_.Empty()) {
      NEARBY_LOG(INFO,
                 "G3 WifiLan StopAdvertising: Can't stop advertising because "
                 "we never started advertising.");
      return false;
    }
    advertising_info_.Clear();
  }

  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumForAdvertising(*this, wifi_lan_service_, service_id,
                                        false);
  accept_loops_runner_.Shutdown();
  if (server_socket_ == nullptr) {
    NEARBY_LOGS(ERROR) << "G3 WifiLan StopAdvertising: failed to find WifiLan "
                          "Server socket: service_id="
                       << service_id;
    // Fall through for server socket not found.
    return true;
  }

  if (!server_socket_->Close().Ok()) {
    NEARBY_LOG(INFO,
               "G3 WifiLan StopAdvertising: Failed to close WifiLan server "
               "socket for %s.",
               service_id.c_str());
    return false;
  }

  return true;
}

bool WifiLanMedium::StartDiscovery(const std::string& service_id,
                                   DiscoveredServiceCallback callback) {
  NEARBY_LOG(INFO, "G3 WifiLan StartDiscovery: service_id=%s",
             service_id.c_str());
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumForDiscovery(*this, service_id, std::move(callback),
                                      true);
  {
    absl::MutexLock lock(&mutex_);
    discovering_info_.service_id = service_id;
  }
  return true;
}

bool WifiLanMedium::StopDiscovery(const std::string& service_id) {
  NEARBY_LOG(INFO, "G3 WifiLan StopDiscovery: service_id=%s",
             service_id.c_str());
  {
    absl::MutexLock lock(&mutex_);
    if (discovering_info_.Empty()) {
      NEARBY_LOG(INFO,
                 "G3 WifiLan StopDiscovery: Can't stop discovering because we "
                 "never started discovering.");
      return false;
    }
    discovering_info_.Clear();
  }

  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumForDiscovery(*this, service_id, {}, false);
  return true;
}

bool WifiLanMedium::StartAcceptingConnections(
    const std::string& service_id, AcceptedConnectionCallback callback) {
  NEARBY_LOG(INFO, "G3 WifiLan StartAcceptingConnections: service_id=%s",
             service_id.c_str());
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumForAcceptedConnection(*this, service_id, callback);
  return true;
}

bool WifiLanMedium::StopAcceptingConnections(const std::string& service_id) {
  NEARBY_LOG(INFO, "G3 WifiLan StopAcceptingConnections: service_id=%s",
             service_id.c_str());
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumForAcceptedConnection(*this, service_id, {});
  return true;
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::Connect(
    api::WifiLanService& remote_wifi_lan_service, const std::string& service_id,
    CancellationFlag* cancellation_flag) {
  NEARBY_LOG(
      INFO,
      "G3 WifiLan Connect: medium=%p, wifi_lan_service=%p, "
      "service_info_name=%s, service_id=%s",
      this, &wifi_lan_service_,
      remote_wifi_lan_service.GetServiceInfo().GetServiceInfoName().c_str(),
      service_id.c_str());
  // First, find an instance of remote medium, that exposed this service.
  auto* remote_medium =
      static_cast<WifiLanService&>(remote_wifi_lan_service).GetMedium();

  if (!remote_medium) return {};  // Can't find medium. Bail out.

  WifiLanServerSocket* remote_server_socket = nullptr;
  NEARBY_LOG(
      INFO,
      "G3 WifiLan Connect [peer]: remote_wifi_lan_service=%p, "
      "remote_service_info_name=%s, service_id=%s",
      &remote_wifi_lan_service,
      remote_wifi_lan_service.GetServiceInfo().GetServiceInfoName().c_str(),
      service_id.c_str());
  // Then, find our server socket context in this medium.
  {
    absl::MutexLock medium_lock(&remote_medium->mutex_);
    remote_server_socket = remote_medium->server_socket_.get();
    if (remote_server_socket == nullptr) {
      NEARBY_LOGS(ERROR)
          << "G3 WifiLan Connect: Failed to find remote WifiLan Server socket: "
             "service_id="
          << service_id;
      // Fall through for server socket not found.
      return {};
    }
  }

  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(INFO) << "G3 WifiLan Connect: Has been cancelled: "
                         "service_id="
                      << service_id;
    return {};
  }

  WifiLanService wifi_lan_service =
      static_cast<WifiLanService&>(remote_wifi_lan_service);
  auto socket = std::make_unique<WifiLanSocket>(&wifi_lan_service);
  // Finally, Request to connect to this socket.
  if (!remote_server_socket->Connect(*socket)) {
    NEARBY_LOG(ERROR,
               "G3 WifiLan Connect: Failed to connect to existing WifiLan "
               "Server socket: service_id=%s",
               service_id.c_str());
    return {};
  }

  NEARBY_LOG(INFO, "G3 WifiLan Connect: connected: socket=%p", socket.get());
  return socket;
}

api::WifiLanService* WifiLanMedium::GetRemoteService(
    const std::string& ip_address, int port) {
  auto& env = MediumEnvironment::Instance();
  return env.GetWifiLanService(ip_address, port);
}

std::pair<std::string, int> WifiLanMedium::GetServiceAddress(
    const std::string& service_id) {
  NEARBY_LOGS(INFO) << "G3 WifiLan GetServiceAddress: service_id="
                    << service_id;
  return wifi_lan_service_.GetServiceInfo().GetServiceAddress();
}

void WifiLanMedium::SetWifiLanService(const NsdServiceInfo& nsd_service_info) {
  NsdServiceInfo local_nsd_service_info{nsd_service_info};
  auto service_address = GetFakeServiceAddress();
  local_nsd_service_info.SetServiceAddress(service_address.first,
                                           service_address.second);
  wifi_lan_service_.SetServiceInfo(local_nsd_service_info);
}

std::pair<std::string, int> WifiLanMedium::GetFakeServiceAddress() const {
  std::string ip_address;
  ip_address.resize(4);
  uint32_t raw_ip_addr = Prng().NextUint32();
  uint16_t port = Prng().NextUint32();
  ip_address[0] = static_cast<char>(raw_ip_addr >> 24);
  ip_address[1] = static_cast<char>(raw_ip_addr >> 16);
  ip_address[2] = static_cast<char>(raw_ip_addr >> 8);
  ip_address[3] = static_cast<char>(raw_ip_addr >> 0);

  return std::make_pair(ip_address, port);
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
