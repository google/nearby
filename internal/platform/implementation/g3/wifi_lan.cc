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

#include "internal/platform/implementation/g3/wifi_lan.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/nsd_service_info.h"

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

void WifiLanSocket::DoClose() {
  if (!closed_) {
    remote_socket_ = nullptr;
    output_->GetOutputStream().Close();
    output_->GetInputStream().Close();
    input_->GetOutputStream().Close();
    input_->GetInputStream().Close();
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

std::string WifiLanServerSocket::GetName(const std::string& ip_address,
                                         int port) {
  std::string dot_delimited_string;
  if (!ip_address.empty()) {
    for (auto byte : ip_address) {
      if (!dot_delimited_string.empty())
        absl::StrAppend(&dot_delimited_string, ".");
      absl::StrAppend(&dot_delimited_string, absl::StrFormat("%d", byte));
    }
  }
  std::string out = absl::StrCat(dot_delimited_string, ":", port);
  return out;
}

std::unique_ptr<api::WifiLanSocket> WifiLanServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  // whether or not we were running in the wait loop, return early if closed.
  if (closed_) return {};
  auto* remote_socket =
      pending_sockets_.extract(pending_sockets_.begin()).value();
  CHECK(remote_socket);

  auto local_socket = std::make_unique<WifiLanSocket>();
  local_socket->Connect(*remote_socket);
  remote_socket->Connect(*local_socket);
  cond_.SignalAll();
  return local_socket;
}

bool WifiLanServerSocket::Connect(WifiLanSocket& socket) {
  absl::MutexLock lock(&mutex_);
  if (closed_) return false;
  if (socket.IsConnected()) {
    NEARBY_LOGS(ERROR)
        << "Failed to connect to WifiLan server socket: already connected";
    return true;  // already connected.
  }
  // add client socket to the pending list
  pending_sockets_.insert(&socket);
  cond_.SignalAll();
  while (!socket.IsConnected()) {
    cond_.Wait(&mutex_);
    if (closed_) return false;
  }
  return true;
}

void WifiLanServerSocket::SetCloseNotifier(
    absl::AnyInvocable<void()> notifier) {
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
  auto& env = MediumEnvironment::Instance();
  env.RegisterWifiLanMedium(*this);
}

WifiLanMedium::~WifiLanMedium() {
  auto& env = MediumEnvironment::Instance();
  env.UnregisterWifiLanMedium(*this);
}

bool WifiLanMedium::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  std::string service_type = nsd_service_info.GetServiceType();
  NEARBY_LOGS(INFO) << "G3 WifiLan StartAdvertising: nsd_service_info="
                    << &nsd_service_info
                    << ", service_name=" << nsd_service_info.GetServiceName()
                    << ", service_type=" << service_type;
  {
    absl::MutexLock lock(&mutex_);
    if (advertising_info_.Existed(service_type)) {
      NEARBY_LOGS(INFO)
          << "G3 WifiLan StartAdvertising: Can't start advertising because "
             "service_type="
          << service_type << ", has started already.";
      return false;
    }
  }
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumForAdvertising(*this, nsd_service_info,
                                        /*enabled=*/true);
  {
    absl::MutexLock lock(&mutex_);
    advertising_info_.Add(service_type);
  }
  return true;
}

bool WifiLanMedium::StopAdvertising(const NsdServiceInfo& nsd_service_info) {
  std::string service_type = nsd_service_info.GetServiceType();
  NEARBY_LOGS(INFO) << "G3 WifiLan StopAdvertising: nsd_service_info="
                    << &nsd_service_info
                    << ", service_name=" << nsd_service_info.GetServiceName()
                    << ", service_type=" << service_type;
  {
    absl::MutexLock lock(&mutex_);
    if (!advertising_info_.Existed(service_type)) {
      NEARBY_LOGS(INFO)
          << "G3 WifiLan StopAdvertising: Can't stop advertising because "
             "we never started advertising for service_type="
          << service_type;
      return false;
    }
    advertising_info_.Remove(service_type);
  }
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumForAdvertising(*this, nsd_service_info,
                                        /*enabled=*/false);
  return true;
}

bool WifiLanMedium::StartDiscovery(const std::string& service_type,
                                   DiscoveredServiceCallback callback) {
  NEARBY_LOGS(INFO) << "G3 WifiLan StartDiscovery: service_type="
                    << service_type;
  {
    absl::MutexLock lock(&mutex_);
    if (discovering_info_.Existed(service_type)) {
      NEARBY_LOGS(INFO)
          << "G3 WifiLan StartDiscovery: Can't start discovery because "
             "service_type="
          << service_type << " has started already.";
      return false;
    }
  }
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumForDiscovery(*this, std::move(callback), service_type,
                                      true);
  {
    absl::MutexLock lock(&mutex_);
    discovering_info_.Add(service_type);
  }
  return true;
}

bool WifiLanMedium::StopDiscovery(const std::string& service_type) {
  NEARBY_LOGS(INFO) << "G3 WifiLan StopDiscovery: service_type="
                    << service_type;
  {
    absl::MutexLock lock(&mutex_);
    if (!discovering_info_.Existed(service_type)) {
      NEARBY_LOGS(INFO)
          << "G3 WifiLan StopDiscovery: Can't stop discovering because we "
             "never started discovering.";
      return false;
    }
    discovering_info_.Remove(service_type);
  }
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumForDiscovery(*this, {}, service_type, false);
  return true;
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const NsdServiceInfo& remote_service_info,
    CancellationFlag* cancellation_flag) {
  std::string service_type = remote_service_info.GetServiceType();
  NEARBY_LOGS(INFO) << "G3 WifiLan ConnectToService [self]: medium=" << this
                    << ", service_type=" << service_type;
  return ConnectToService(remote_service_info.GetIPAddress(),
                          remote_service_info.GetPort(), cancellation_flag);
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const std::string& ip_address, int port,
    CancellationFlag* cancellation_flag) {
  std::string socket_name = WifiLanServerSocket::GetName(ip_address, port);
  NEARBY_LOGS(INFO) << "G3 WifiLan ConnectToService [self]: medium=" << this
                    << ", ip address + port=" << socket_name;
  // First, find an instance of remote medium, that exposed this service.
  auto& env = MediumEnvironment::Instance();
  auto* remote_medium =
      static_cast<WifiLanMedium*>(env.GetWifiLanMedium(ip_address, port));
  if (!remote_medium) {
    return {};
  }

  WifiLanServerSocket* server_socket = nullptr;
  NEARBY_LOGS(INFO) << "G3 WifiLan ConnectToService [peer]: medium="
                    << remote_medium
                    << ", remote ip address + port=" << socket_name;
  // Then, find our server socket context in this medium.
  {
    absl::MutexLock medium_lock(&remote_medium->mutex_);
    auto item = remote_medium->server_sockets_.find(socket_name);
    server_socket =
        item != remote_medium->server_sockets_.end() ? item->second : nullptr;
    if (server_socket == nullptr) {
      NEARBY_LOGS(ERROR)
          << "G3 WifiLan Failed to find WifiLan Server socket: socket_name="
          << socket_name;
      return {};
    }
  }

  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(ERROR) << "G3 WifiLan Connect: Has been cancelled: socket_name="
                       << socket_name;
    return {};
  }

  CancellationFlagListener listener(cancellation_flag, [&server_socket]() {
    NEARBY_LOGS(INFO) << "G3 WifiLan Cancel Connect.";
    if (server_socket != nullptr) {
      server_socket->Close();
    }
  });

  auto socket = std::make_unique<WifiLanSocket>();
  // Finally, Request to connect to this socket.
  if (!server_socket->Connect(*socket)) {
    NEARBY_LOGS(ERROR) << "G3 WifiLan Failed to connect to existing WifiLan "
                          "Server socket: name="
                       << socket_name;
    return {};
  }
  NEARBY_LOGS(INFO) << "G3 WifiLan ConnectToService: connected: socket="
                    << socket.get();
  return socket;
}

std::unique_ptr<api::WifiLanServerSocket> WifiLanMedium::ListenForService(
    int port) {
  auto& env = MediumEnvironment::Instance();
  auto server_socket = std::make_unique<WifiLanServerSocket>();
  server_socket->SetIPAddress(env.GetFakeIPAddress());
  server_socket->SetPort(port == 0 ? env.GetFakePort() : port);
  std::string socket_name = WifiLanServerSocket::GetName(
      server_socket->GetIPAddress(), server_socket->GetPort());
  server_socket->SetCloseNotifier([this, socket_name]() {
    absl::MutexLock lock(&mutex_);
    server_sockets_.erase(socket_name);
  });
  NEARBY_LOGS(INFO) << "G3 WifiLan Adding server socket: medium=" << this
                    << ", socket_name=" << socket_name;
  absl::MutexLock lock(&mutex_);
  server_sockets_.insert({socket_name, server_socket.get()});
  return server_socket;
}

}  // namespace g3
}  // namespace nearby
