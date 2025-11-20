// Copyright 2021 Google LLC
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

#include <windows.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>

// Nearby connections headers
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/wifi_direct.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"

namespace nearby::windows {
namespace {
constexpr int kWaitingForServerSocketReadyTimeoutSeconds = 90;  // seconds
}  // namespace

WifiDirectServerSocket::~WifiDirectServerSocket() { Close(); }

std::string WifiDirectServerSocket::GetIPAddress() const {
  return wifi_direct_ipaddr_;
}

void WifiDirectServerSocket::SetIPAddress(std::string ip_address) {
  absl::MutexLock lock(&mutex_);
  if (ip_address.empty()) {
    return;
  }
  wifi_direct_ipaddr_ = ip_address;
}

std::unique_ptr<api::WifiDirectSocket> WifiDirectServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  if (!is_listen_started_) {
    LOG(INFO) << __func__
              << ": Server socket is not started, wait for server socket is "
                 "ready.";
    is_listen_ready_.WaitWithTimeout(
        &mutex_, absl::Seconds(kWaitingForServerSocketReadyTimeoutSeconds));
    if (!is_listen_started_) {
      LOG(INFO) << __func__
                << ": Server socket failed to start within timeout.";
      return nullptr;
    }
  }

  auto client_socket = server_socket_.Accept();
  if (client_socket == nullptr) {
    return nullptr;
  }

  LOG(INFO) << __func__ << ": Accepted a remote connection.";
  return std::make_unique<WifiDirectSocket>(std::move(client_socket));
}

void WifiDirectServerSocket::PopulateWifiDirectCredentials(
    WifiDirectCredentials& wifi_direct_credentials) {
  wifi_direct_credentials.SetGateway(wifi_direct_ipaddr_);
  if (GetPort() != 0) {
  wifi_direct_credentials.SetPort(GetPort());
  } else {
    wifi_direct_credentials.SetPort(FeatureFlags::GetInstance()
                                        .GetFlags()
                                        .wifi_direct_default_port);
  }
}

Exception WifiDirectServerSocket::Close() {
  absl::MutexLock lock(mutex_);
  if (closed_) {
    return {Exception::kSuccess};
  }
  wifi_direct_ipaddr_.clear();
  is_listen_started_ = false;
  server_socket_.Close();
  closed_ = true;

  LOG(INFO) << __func__ << ": Close completed succesfully.";
  return {Exception::kSuccess};
}

bool WifiDirectServerSocket::Listen(int port, bool dual_stack) {
  LOG(INFO) << "Listen wifi_direct on IP:port " << wifi_direct_ipaddr_ << ":"
            << port;
  SocketAddress address(dual_stack);
  if (!SocketAddress::FromString(address, wifi_direct_ipaddr_, port)) {
    LOG(ERROR) << "Failed to parse wifi_direct IP address.";
    return false;
  }
  if (!server_socket_.Listen(address)) {
    LOG(ERROR) << "Failed to listen socket.";
    return false;
  }
  LOG(INFO) << "Notify the server socket is started.";
  absl::MutexLock lock(&mutex_);
  is_listen_started_ = true;
  is_listen_ready_.SignalAll();

  return true;
}

std::string WifiDirectServerSocket::GetWifiDirectIpAddress() const {
  return wifi_direct_ipaddr_;
}

}  // namespace nearby::windows
