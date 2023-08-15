// Copyright 2022 Google LLC
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

#include "internal/platform/implementation/g3/wifi_direct.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/prng.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {
namespace g3 {

// Code for WifiDirectServerSocket
std::string WifiDirectServerSocket::GetName(absl::string_view ip_address,
                                            int port) {
  return absl::StrCat(ip_address, ":", port);
}

std::unique_ptr<api::WifiDirectSocket> WifiDirectServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  // whether or not we were running in the wait loop, return early if closed.
  if (closed_) return {};
  auto* remote_socket =
      pending_sockets_.extract(pending_sockets_.begin()).value();
  CHECK(remote_socket);

  auto local_socket = std::make_unique<WifiDirectSocket>();
  local_socket->Connect(*remote_socket);
  remote_socket->Connect(*local_socket);
  cond_.SignalAll();
  return local_socket;
}

bool WifiDirectServerSocket::Connect(WifiDirectSocket& socket) {
  absl::MutexLock lock(&mutex_);
  if (closed_) return false;
  if (socket.IsConnected()) {
    NEARBY_LOGS(ERROR)
        << "Failed to connect to WifiDirect server socket: already connected";
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

void WifiDirectServerSocket::SetCloseNotifier(
    absl::AnyInvocable<void()> notifier) {
  absl::MutexLock lock(&mutex_);
  close_notifier_ = std::move(notifier);
}

WifiDirectServerSocket::~WifiDirectServerSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

Exception WifiDirectServerSocket::Close() {
  absl::MutexLock lock(&mutex_);
  return DoClose();
}

Exception WifiDirectServerSocket::DoClose() {
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

// Code for WifiDirectMedium
WifiDirectMedium::WifiDirectMedium() {
  auto& env = MediumEnvironment::Instance();
  env.RegisterWifiDirectMedium(*this);
}

WifiDirectMedium::~WifiDirectMedium() {
  auto& env = MediumEnvironment::Instance();
  env.UnregisterWifiDirectMedium(*this);
}

bool WifiDirectMedium::StartWifiDirect(
    WifiDirectCredentials* wifi_direct_credentials) {
  absl::MutexLock lock(&mutex_);

  std::string ssid = absl::StrCat("DIRECT-", Prng().NextUint32());
  wifi_direct_credentials->SetSSID(ssid);
  std::string password = absl::StrFormat("%08x", Prng().NextUint32());
  wifi_direct_credentials->SetPassword(password);

  NEARBY_LOGS(INFO) << "G3 StartWifiDirect GO: ssid=" << ssid
                    << ",  password:" << password;

  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiDirectMediumForStartOrConnect(*this, wifi_direct_credentials,
                                              /*is_go=*/true, /*enabled=*/true);

  return true;
}

bool WifiDirectMedium::StopWifiDirect() {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "G3 StopWifiDirect GO";

  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiDirectMediumForStartOrConnect(*this, /*credentials*/ nullptr,
                                              /*is_go=*/true,
                                              /*enabled=*/false);
  return true;
}

bool WifiDirectMedium::ConnectWifiDirect(
    WifiDirectCredentials* wifi_direct_credentials) {
  absl::MutexLock lock(&mutex_);

  NEARBY_LOGS(INFO) << "G3 ConnectWifiDirect : ssid="
                    << wifi_direct_credentials->GetSSID()
                    << ",  password:" << wifi_direct_credentials->GetPassword();

  auto& env = MediumEnvironment::Instance();
  auto* remote_medium = static_cast<WifiDirectMedium*>(
      env.GetWifiDirectMedium(wifi_direct_credentials->GetSSID(), {}));
  if (!remote_medium) {
    env.UpdateWifiDirectMediumForStartOrConnect(*this, wifi_direct_credentials,
                                                /*is_go=*/false,
                                                /*enabled=*/false);
    return false;
  }

  env.UpdateWifiDirectMediumForStartOrConnect(*this, wifi_direct_credentials,
                                              /*is_go=*/false,
                                              /*enabled=*/true);
  return true;
}

bool WifiDirectMedium::DisconnectWifiDirect() {
  absl::MutexLock lock(&mutex_);

  NEARBY_LOGS(INFO) << "G3 DisconnectWifiDirect";

  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiDirectMediumForStartOrConnect(*this, /*credentials*/ nullptr,
                                              /*is_go=*/false,
                                              /*enabled=*/false);
  return true;
}

std::unique_ptr<api::WifiDirectSocket> WifiDirectMedium::ConnectToService(
    absl::string_view ip_address, int port,
    CancellationFlag* cancellation_flag) {
  std::string socket_name = WifiDirectServerSocket::GetName(ip_address, port);
  NEARBY_LOGS(INFO) << "G3 WifiDirect ConnectToService [self]: medium=" << this
                    << ", ip address + port=" << socket_name;
  // First, find an instance of remote medium, that exposed this service.
  auto& env = MediumEnvironment::Instance();
  auto* remote_medium =
      static_cast<WifiDirectMedium*>(env.GetWifiDirectMedium({}, ip_address));
  if (remote_medium == nullptr) {
    return nullptr;
  }

  WifiDirectServerSocket* server_socket = nullptr;
  NEARBY_LOGS(INFO) << "G3 WifiDirect ConnectToService [peer]: medium="
                    << remote_medium
                    << ", remote ip address + port=" << socket_name;
  // Then, find our server socket context in this medium.
  {
    absl::MutexLock medium_lock(&remote_medium->mutex_);
    auto item = remote_medium->server_sockets_.find(socket_name);
    server_socket =
        item != remote_medium->server_sockets_.end() ? item->second : nullptr;
    if (server_socket == nullptr) {
      NEARBY_LOGS(ERROR) << "G3 WifiDirect Failed to find WifiDirect Server "
                            "socket: socket_name="
                         << socket_name;
      return nullptr;
    }
  }

  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(ERROR)
        << "G3 WifiDirect Connect: Has been cancelled: socket_name="
        << socket_name;
    return nullptr;
  }

  auto socket = std::make_unique<WifiDirectSocket>();
  // Finally, Request to connect to this socket.

  server_socket->Connect(*socket);
  NEARBY_LOGS(INFO) << "G3 WifiDirect GC ConnectToService: connected: socket="
                    << socket.get();
  return socket;
}

std::unique_ptr<api::WifiDirectServerSocket> WifiDirectMedium::ListenForService(
    int port) {
  auto& env = MediumEnvironment::Instance();
  auto server_socket = std::make_unique<WifiDirectServerSocket>();

  std::string dot_decimal_ip;
  std::string ip_address = env.GetFakeIPAddress();

  for (auto byte : ip_address) {
    absl::StrAppend(&dot_decimal_ip, absl::StrFormat("%d", byte), ".");
  }
  dot_decimal_ip.pop_back();

  server_socket->SetIPAddress(dot_decimal_ip);
  server_socket->SetPort(port == 0 ? env.GetFakePort() : port);
  std::string socket_name = WifiDirectServerSocket::GetName(
      server_socket->GetIPAddress(), server_socket->GetPort());
  server_socket->SetCloseNotifier([this, socket_name]() {
    absl::MutexLock lock(&mutex_);
    server_sockets_.erase(socket_name);
  });
  NEARBY_LOGS(INFO) << "G3 WifiDirect GO Adding server socket: medium=" << this
                    << ", socket_name=" << socket_name;
  absl::MutexLock lock(&mutex_);
  server_sockets_.insert({socket_name, server_socket.get()});
  return server_socket;
}

}  // namespace g3
}  // namespace nearby
