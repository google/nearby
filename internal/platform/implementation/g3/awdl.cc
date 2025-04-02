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

#include "internal/platform/implementation/g3/awdl.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/awdl.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/nsd_service_info.h"

namespace nearby {
namespace g3 {

std::string AwdlServerSocket::GetName(const std::string& ip_address,
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

std::unique_ptr<api::AwdlSocket> AwdlServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  // whether or not we were running in the wait loop, return early if closed.
  if (closed_) return {};
  auto* remote_socket =
      pending_sockets_.extract(pending_sockets_.begin()).value();
  CHECK(remote_socket);

  auto local_socket = std::make_unique<AwdlSocket>();
  local_socket->Connect(*remote_socket);
  remote_socket->Connect(*local_socket);
  cond_.SignalAll();
  return local_socket;
}

bool AwdlServerSocket::Connect(AwdlSocket& socket) {
  absl::MutexLock lock(&mutex_);
  if (closed_) return false;
  if (socket.IsConnected()) {
    NEARBY_LOGS(ERROR)
        << "Failed to connect to Awdl server socket: already connected";
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

void AwdlServerSocket::SetCloseNotifier(
    absl::AnyInvocable<void()> notifier) {
  absl::MutexLock lock(&mutex_);
  close_notifier_ = std::move(notifier);
}

AwdlServerSocket::~AwdlServerSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

Exception AwdlServerSocket::Close() {
  absl::MutexLock lock(&mutex_);
  return DoClose();
}

Exception AwdlServerSocket::DoClose() {
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

AwdlMedium::AwdlMedium() {
  auto& env = MediumEnvironment::Instance();
  env.RegisterAwdlMedium(*this);
}

AwdlMedium::~AwdlMedium() {
  auto& env = MediumEnvironment::Instance();
  env.UnregisterAwdlMedium(*this);
}

bool AwdlMedium::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  std::string service_type = nsd_service_info.GetServiceType();
  NEARBY_LOGS(INFO) << "G3 Awdl StartAdvertising: nsd_service_info="
                    << &nsd_service_info
                    << ", service_name=" << nsd_service_info.GetServiceName()
                    << ", service_type=" << service_type;
  {
    absl::MutexLock lock(&mutex_);
    if (advertising_info_.Existed(service_type)) {
      NEARBY_LOGS(INFO)
          << "G3 Awdl StartAdvertising: Can't start advertising because "
             "service_type="
          << service_type << ", has started already.";
      return false;
    }
  }
  auto& env = MediumEnvironment::Instance();
  env.UpdateAwdlMediumForAdvertising(*this, nsd_service_info,
                                        /*enabled=*/true);
  {
    absl::MutexLock lock(&mutex_);
    advertising_info_.Add(service_type);
  }
  return true;
}

bool AwdlMedium::StopAdvertising(const NsdServiceInfo& nsd_service_info) {
  std::string service_type = nsd_service_info.GetServiceType();
  NEARBY_LOGS(INFO) << "G3 Awdl StopAdvertising: nsd_service_info="
                    << &nsd_service_info
                    << ", service_name=" << nsd_service_info.GetServiceName()
                    << ", service_type=" << service_type;
  {
    absl::MutexLock lock(&mutex_);
    if (!advertising_info_.Existed(service_type)) {
      NEARBY_LOGS(INFO)
          << "G3 Awdl StopAdvertising: Can't stop advertising because "
             "we never started advertising for service_type="
          << service_type;
      return false;
    }
    advertising_info_.Remove(service_type);
  }
  auto& env = MediumEnvironment::Instance();
  env.UpdateAwdlMediumForAdvertising(*this, nsd_service_info,
                                        /*enabled=*/false);
  return true;
}

bool AwdlMedium::StartDiscovery(const std::string& service_type,
                                   DiscoveredServiceCallback callback) {
  NEARBY_LOGS(INFO) << "G3 Awdl StartDiscovery: service_type="
                    << service_type;
  {
    absl::MutexLock lock(&mutex_);
    if (discovering_info_.Existed(service_type)) {
      NEARBY_LOGS(INFO)
          << "G3 Awdl StartDiscovery: Can't start discovery because "
             "service_type="
          << service_type << " has started already.";
      return false;
    }
  }
  auto& env = MediumEnvironment::Instance();
  env.UpdateAwdlMediumForDiscovery(*this, std::move(callback), service_type,
                                      true);
  {
    absl::MutexLock lock(&mutex_);
    discovering_info_.Add(service_type);
  }
  return true;
}

bool AwdlMedium::StopDiscovery(const std::string& service_type) {
  NEARBY_LOGS(INFO) << "G3 Awdl StopDiscovery: service_type="
                    << service_type;
  {
    absl::MutexLock lock(&mutex_);
    if (!discovering_info_.Existed(service_type)) {
      NEARBY_LOGS(INFO)
          << "G3 Awdl StopDiscovery: Can't stop discovering because we "
             "never started discovering.";
      return false;
    }
    discovering_info_.Remove(service_type);
  }
  auto& env = MediumEnvironment::Instance();
  env.UpdateAwdlMediumForDiscovery(*this, {}, service_type, false);
  return true;
}

std::unique_ptr<api::AwdlSocket> AwdlMedium::ConnectToService(
    const NsdServiceInfo& remote_service_info,
    CancellationFlag* cancellation_flag) {
  std::string service_type = remote_service_info.GetServiceType();
  NEARBY_LOGS(INFO) << "G3 Awdl ConnectToService [self]: medium=" << this
                    << ", service_type=" << service_type;
  return ConnectToService(remote_service_info.GetIPAddress(),
                          remote_service_info.GetPort(), cancellation_flag);
}

std::unique_ptr<api::AwdlSocket> AwdlMedium::ConnectToService(
    const std::string& ip_address, int port,
    CancellationFlag* cancellation_flag) {
  std::string socket_name = AwdlServerSocket::GetName(ip_address, port);
  NEARBY_LOGS(INFO) << "G3 Awdl ConnectToService [self]: medium=" << this
                    << ", ip address + port=" << socket_name;
  // First, find an instance of remote medium, that exposed this service.
  auto& env = MediumEnvironment::Instance();
  auto* remote_medium =
      static_cast<AwdlMedium*>(env.GetAwdlMedium(ip_address, port));
  if (!remote_medium) {
    return {};
  }

  AwdlServerSocket* server_socket = nullptr;
  NEARBY_LOGS(INFO) << "G3 Awdl ConnectToService [peer]: medium="
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
          << "G3 Awdl Failed to find Awdl Server socket: socket_name="
          << socket_name;
      return {};
    }
  }

  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(ERROR) << "G3 Awdl Connect: Has been cancelled: socket_name="
                       << socket_name;
    return {};
  }

  CancellationFlagListener listener(cancellation_flag, [&server_socket]() {
    NEARBY_LOGS(INFO) << "G3 Awdl Cancel Connect.";
    if (server_socket != nullptr) {
      server_socket->Close();
    }
  });

  auto socket = std::make_unique<AwdlSocket>();
  // Finally, Request to connect to this socket.
  if (!server_socket->Connect(*socket)) {
    NEARBY_LOGS(ERROR) << "G3 Awdl Failed to connect to existing Awdl "
                          "Server socket: name="
                       << socket_name;
    return {};
  }
  NEARBY_LOGS(INFO) << "G3 Awdl ConnectToService: connected: socket="
                    << socket.get();
  return socket;
}

std::unique_ptr<api::AwdlServerSocket> AwdlMedium::ListenForService(
    int port) {
  auto& env = MediumEnvironment::Instance();
  auto server_socket = std::make_unique<AwdlServerSocket>();
  server_socket->SetIPAddress(env.GetFakeIPAddress());
  server_socket->SetPort(port == 0 ? env.GetFakePort() : port);
  std::string socket_name = AwdlServerSocket::GetName(
      server_socket->GetIPAddress(), server_socket->GetPort());
  server_socket->SetCloseNotifier([this, socket_name]() {
    absl::MutexLock lock(&mutex_);
    server_sockets_.erase(socket_name);
  });
  NEARBY_LOGS(INFO) << "G3 Awdl Adding server socket: medium=" << this
                    << ", socket_name=" << socket_name;
  absl::MutexLock lock(&mutex_);
  server_sockets_.insert({socket_name, server_socket.get()});
  return server_socket;
}

}  // namespace g3
}  // namespace nearby
