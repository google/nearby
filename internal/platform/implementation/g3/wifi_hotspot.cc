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

#include "internal/platform/implementation/g3/wifi_hotspot.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/prng.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {
namespace g3 {

// Code for WifiHotspotServerSocket
std::string WifiHotspotServerSocket::GetName(absl::string_view ip_address,
                                             int port) {
  return absl::StrCat(ip_address, ":", port);
}

std::unique_ptr<api::WifiHotspotSocket> WifiHotspotServerSocket::Accept() {
  absl::MutexLock lock(mutex_);
  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  // whether or not we were running in the wait loop, return early if closed.
  if (closed_) return {};
  auto* remote_socket =
      pending_sockets_.extract(pending_sockets_.begin()).value();
  CHECK(remote_socket);

  auto local_socket = std::make_unique<WifiHotspotSocket>();
  local_socket->Connect(*remote_socket);
  remote_socket->Connect(*local_socket);
  cond_.SignalAll();
  return local_socket;
}

bool WifiHotspotServerSocket::Connect(WifiHotspotSocket& socket) {
  absl::MutexLock lock(mutex_);
  if (closed_) return false;
  if (socket.IsConnected()) {
    LOG(ERROR)
        << "Failed to connect to WifiHotspot server socket: already connected";
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

void WifiHotspotServerSocket::SetCloseNotifier(
    absl::AnyInvocable<void()> notifier) {
  absl::MutexLock lock(mutex_);
  close_notifier_ = std::move(notifier);
}

WifiHotspotServerSocket::~WifiHotspotServerSocket() {
  absl::MutexLock lock(mutex_);
  DoClose();
}

Exception WifiHotspotServerSocket::Close() {
  absl::MutexLock lock(mutex_);
  return DoClose();
}

Exception WifiHotspotServerSocket::DoClose() {
  bool should_notify = !closed_;
  closed_ = true;
  if (should_notify) {
    cond_.SignalAll();
    if (close_notifier_) {
      auto notifier = std::move(close_notifier_);
      mutex_.unlock();
      // Notifier may contain calls to public API, and may cause deadlock, if
      // mutex_ is held during the call.
      notifier();
      mutex_.lock();
    }
  }
  return {Exception::kSuccess};
}

void WifiHotspotServerSocket::PopulateHotspotCredentials(
    HotspotCredentials& hotspot_credentials) {
  absl::MutexLock lock(mutex_);
  std::vector<ServiceAddress> service_addresses = {
    {
      .port = static_cast<uint16_t>(port_),
    },
  };
  service_addresses.back().address.assign(ip_address_.begin(),
                                          ip_address_.end());
  hotspot_credentials.SetAddressCandidates(std::move(service_addresses));
}

// Code for WifiHotspotMedium
WifiHotspotMedium::WifiHotspotMedium() {
  auto& env = MediumEnvironment::Instance();
  env.RegisterWifiHotspotMedium(*this);
}

WifiHotspotMedium::~WifiHotspotMedium() {
  auto& env = MediumEnvironment::Instance();
  env.UnregisterWifiHotspotMedium(*this);
}

bool WifiHotspotMedium::StartWifiHotspot(
    HotspotCredentials* hotspot_credentials) {
  absl::MutexLock lock(mutex_);

  if (!IsInterfaceValid()) return false;

  std::string ssid = absl::StrCat("DIRECT-", Prng().NextUint32());
  hotspot_credentials->SetSSID(ssid);
  std::string password = absl::StrFormat("%08x", Prng().NextUint32());
  hotspot_credentials->SetPassword(password);

  LOG(INFO) << "G3 StartWifiHotspot: ssid=" << ssid
            << ",  password:" << password;

  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiHotspotMediumForStartOrConnect(*this, hotspot_credentials,
                                               /*is_ap=*/true,
                                               /*enabled=*/true);

  return true;
}

bool WifiHotspotMedium::StopWifiHotspot() {
  absl::MutexLock lock(mutex_);
  LOG(INFO) << "G3 StopWifiHotspot";

  if (!IsInterfaceValid()) return false;

  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiHotspotMediumForStartOrConnect(*this, /*credentials*/ nullptr,
                                               /*is_ap=*/true,
                                               /*enabled=*/false);
  return true;
}

bool WifiHotspotMedium::ConnectWifiHotspot(
    const HotspotCredentials& hotspot_credentials) {
  absl::MutexLock lock(mutex_);

  LOG(INFO) << "G3 ConnectWifiHotspot: ssid=" << hotspot_credentials.GetSSID()
            << ",  password:" << hotspot_credentials.GetPassword();

  auto& env = MediumEnvironment::Instance();
  auto* remote_medium = static_cast<WifiHotspotMedium*>(
      env.GetWifiHotspotMedium(hotspot_credentials.GetSSID(), ""));
  if (!remote_medium) {
    env.UpdateWifiHotspotMediumForStartOrConnect(*this, &hotspot_credentials,
                                                 /*is_ap=*/false,
                                                 /*enabled=*/false);
    return false;
  }

  env.UpdateWifiHotspotMediumForStartOrConnect(*this, &hotspot_credentials,
                                               /*is_ap=*/false,
                                               /*enabled=*/true);
  return true;
}

bool WifiHotspotMedium::DisconnectWifiHotspot() {
  absl::MutexLock lock(mutex_);

  LOG(INFO) << "G3 DisconnectWifiHotspot";

  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiHotspotMediumForStartOrConnect(*this, /*credentials*/ nullptr,
                                               /*is_ap=*/false,
                                               /*enabled=*/false);
  return true;
}

std::unique_ptr<api::WifiHotspotSocket> WifiHotspotMedium::ConnectToService(
    const ServiceAddress& service_address,
    CancellationFlag* cancellation_flag) {
  std::string ip_address = std::string(service_address.address.data(),
                                       service_address.address.size());
  std::string socket_name =
      WifiHotspotServerSocket::GetName(ip_address, service_address.port);
  LOG(INFO) << "G3 WifiHotspot ConnectToService [self]: medium=" << this
            << ", ip address + port=" << socket_name;
  // First, find an instance of remote medium, that exposed this service.
  auto& env = MediumEnvironment::Instance();
  auto* remote_medium = static_cast<WifiHotspotMedium*>(
      env.GetWifiHotspotMedium({}, WifiUtils::GetHumanReadableIpAddress(
                                       {service_address.address.data(),
                                        service_address.address.size()})));
  if (remote_medium == nullptr) {
    return {};
  }

  WifiHotspotServerSocket* server_socket = nullptr;
  LOG(INFO) << "G3 WifiHotspot ConnectToService [peer]: medium="
            << remote_medium << ", remote ip address + port=" << socket_name;
  // Then, find our server socket context in this medium.
  {
    absl::MutexLock medium_lock(remote_medium->mutex_);
    auto item = remote_medium->server_sockets_.find(socket_name);
    server_socket =
        item != remote_medium->server_sockets_.end() ? item->second : nullptr;
    if (server_socket == nullptr) {
      LOG(ERROR) << "G3 WifiHotspot Failed to find WifiHotspot Server "
                    "socket: socket_name="
                 << socket_name;
      return {};
    }
  }

  if (cancellation_flag->Cancelled()) {
    LOG(ERROR) << "G3 WifiHotspot Connect: Has been cancelled: socket_name="
               << socket_name;
    return {};
  }

  CancellationFlagListener listener(cancellation_flag, [&server_socket]() {
    LOG(INFO) << "G3 WifiHotspot Cancel Connect.";
    if (server_socket != nullptr) {
      server_socket->Close();
    }
  });

  auto socket = std::make_unique<WifiHotspotSocket>();
  // Finally, Request to connect to this socket.
  if (!server_socket->Connect(*socket)) {
    LOG(ERROR) << "G3 WifiHotspot Failed to connect to existing WifiHotspot "
                  "Server socket: name="
               << socket_name;
    return {};
  }
  LOG(INFO) << "G3 WifiHotspot ConnectToService: connected: socket="
            << socket.get();
  return socket;
}

std::unique_ptr<api::WifiHotspotServerSocket>
WifiHotspotMedium::ListenForService(int port) {
  auto& env = MediumEnvironment::Instance();
  auto server_socket = std::make_unique<WifiHotspotServerSocket>();

  std::string ip_address = env.GetFakeIPAddress();
  if (ip_address.empty()) return nullptr;

  server_socket->SetIPAddress(ip_address);
  int port_to_use = port == 0 ? env.GetFakePort() : port;
  server_socket->SetPort(port_to_use);
  std::string socket_name =
      WifiHotspotServerSocket::GetName(ip_address, port_to_use);
  server_socket->SetCloseNotifier([this, socket_name]() {
    absl::MutexLock lock(mutex_);
    server_sockets_.erase(socket_name);
  });
  LOG(INFO) << "G3 WifiHotspot Adding server socket: medium=" << this
            << ", socket_name=" << socket_name;
  absl::MutexLock lock(mutex_);
  server_sockets_.insert({socket_name, server_socket.get()});
  return server_socket;
}

}  // namespace g3
}  // namespace nearby
