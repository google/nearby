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
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Connectivity.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/wifi_direct.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

WifiDirectServerSocket::WifiDirectServerSocket(int port) : port_(port) {}

WifiDirectServerSocket::~WifiDirectServerSocket() { Close(); }

std::string WifiDirectServerSocket::GetIPAddress() const {
  return wifi_direct_ipaddr_;
}

int WifiDirectServerSocket::GetPort() const { return server_socket_.GetPort(); }

std::unique_ptr<api::WifiDirectSocket> WifiDirectServerSocket::Accept() {
  auto client_socket = server_socket_.Accept();
  if (client_socket == nullptr) {
    return nullptr;
  }

  LOG(INFO) << __func__ << ": Accepted a remote connection.";
  return std::make_unique<WifiDirectSocket>(std::move(client_socket));
}

void WifiDirectServerSocket::SetCloseNotifier(
    absl::AnyInvocable<void()> notifier) {
  close_notifier_ = std::move(notifier);
}

Exception WifiDirectServerSocket::Close() {
  absl::MutexLock lock(&mutex_);
  if (closed_) {
    return {Exception::kSuccess};
  }

  server_socket_.Close();
  closed_ = true;

  if (close_notifier_ != nullptr) {
    close_notifier_();
  }

  LOG(INFO) << __func__ << ": Close completed succesfully.";
  return {Exception::kSuccess};
}

bool WifiDirectServerSocket::Listen(bool dual_stack, std::string& ip_address) {
  // Get current IP addresses of the device.
  if (ip_address.empty()) {
    return false;
  }
  wifi_direct_ipaddr_ = ip_address;
  LOG(INFO) << "Listen wifi_direct on IP:port " << ip_address << ":" << port_;
  SocketAddress address(dual_stack);
  if (!SocketAddress::FromString(address, ip_address, port_)) {
    LOG(ERROR) << "Failed to parse wifi_direct IP address: " << ip_address
               << " and port: " << port_;
    return false;
  }
  if (!server_socket_.Listen(address)) {
    LOG(ERROR) << "Failed to listen socket.";
    return false;
  }

  return true;
}

std::string WifiDirectServerSocket::GetWifiDirectIpAddress() const {
  return wifi_direct_ipaddr_;
}

}  // namespace nearby::windows
