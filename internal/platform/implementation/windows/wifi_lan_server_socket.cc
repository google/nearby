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

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/wifi_lan.h"
#include "internal/platform/logging.h"

namespace location {
namespace nearby {
namespace windows {
namespace {

using ::winrt::Windows::Networking::Sockets::SocketQualityOfService;

}

WifiLanServerSocket::WifiLanServerSocket(int port) : port_(port) {}

WifiLanServerSocket::~WifiLanServerSocket() { Close(); }

// Returns the first IP address.
std::string WifiLanServerSocket::GetIPAddress() const {
  if (stream_socket_listener_ == nullptr) {
    return {};
  }

  if (ip_addresses_.empty()) {
    auto ip_addr = GetIpAddresses();
    if (ip_addr.empty()) {
      return {};
    }
    return ip_addr.front();
  }
  return ip_addresses_.front();
}

// Returns socket port.
int WifiLanServerSocket::GetPort() const {
  if (stream_socket_listener_ == nullptr) {
    return 0;
  }

  return std::stoi(stream_socket_listener_.Information().LocalPort().c_str());
}

// Blocks until either:
// - at least one incoming connection request is available, or
// - ServerSocket is closed.
// On success, returns connected socket, ready to exchange data.
// Returns nullptr on error.
// Once error is reported, it is permanent, and ServerSocket has to be closed.
std::unique_ptr<api::WifiLanSocket> WifiLanServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << __func__ << ": Accept is called.";

  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  if (closed_) return {};

  StreamSocket wifi_lan_socket = pending_sockets_.front();
  pending_sockets_.pop_front();

  NEARBY_LOGS(INFO) << __func__ << ": Accepted a remote connection.";
  return std::make_unique<WifiLanSocket>(wifi_lan_socket);
}

void WifiLanServerSocket::SetCloseNotifier(std::function<void()> notifier) {
  close_notifier_ = std::move(notifier);
}

// Returns Exception::kIo on error, Exception::kSuccess otherwise.
Exception WifiLanServerSocket::Close() {
  try {
    absl::MutexLock lock(&mutex_);
    NEARBY_LOGS(INFO) << __func__ << ": Close is called.";

    if (closed_) {
      return {Exception::kSuccess};
    }
    if (stream_socket_listener_ != nullptr) {
      stream_socket_listener_.ConnectionReceived(listener_event_token_);
      stream_socket_listener_.Close();
      stream_socket_listener_ = nullptr;

      for (const auto& pending_socket : pending_sockets_) {
        pending_socket.Close();
      }

      pending_sockets_ = {};
    }

    closed_ = true;
    cond_.SignalAll();

    if (close_notifier_ != nullptr) {
      close_notifier_();
    }

    NEARBY_LOGS(INFO) << __func__ << ": Close completed succesfully.";
    return {Exception::kSuccess};
  } catch (...) {
    closed_ = true;
    cond_.SignalAll();

    NEARBY_LOGS(INFO) << __func__ << ": Failed to close server socket.";
    return {Exception::kIo};
  }
}

bool WifiLanServerSocket::listen() {
  // Get current IP addresses of the device.
  ip_addresses_ = GetIpAddresses();

  if (ip_addresses_.empty()) {
    NEARBY_LOGS(WARNING) << "failed to start accepting connection without IP "
                            "addresses configured on computer.";
    return false;
  }

  // Setup stream socket listener.
  stream_socket_listener_ = StreamSocketListener();

  stream_socket_listener_.Control().QualityOfService(
      SocketQualityOfService::LowLatency);

  stream_socket_listener_.Control().KeepAlive(true);

  // Setup socket event of ConnectionReceived.
  listener_event_token_ = stream_socket_listener_.ConnectionReceived(
      {this, &WifiLanServerSocket::Listener_ConnectionReceived});

  try {
    stream_socket_listener_.BindServiceNameAsync(winrt::to_hstring(port_))
        .get();
    if (port_ == 0) {
      port_ =
          std::stoi(stream_socket_listener_.Information().LocalPort().c_str());
    }

    return true;
  } catch (...) {
    // Cannot bind to the preferred port, will let system to assign port.
    NEARBY_LOGS(WARNING) << "cannot accept connection on preferred port.";
  }

  try {
    stream_socket_listener_.BindServiceNameAsync({}).get();

    // Need to save the port information.
    port_ =
        std::stoi(stream_socket_listener_.Information().LocalPort().c_str());
    return true;
  } catch (...) {
    // Cannot bind to the preferred port, will let system to assign port.
    NEARBY_LOGS(ERROR) << "cannot bind to any port.";
  }

  return false;
}

fire_and_forget WifiLanServerSocket::Listener_ConnectionReceived(
    StreamSocketListener listener,
    StreamSocketListenerConnectionReceivedEventArgs const& args) {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << __func__ << ": Received connection.";

  if (closed_) {
    return fire_and_forget{};
  }

  pending_sockets_.push_back(args.Socket());
  cond_.SignalAll();
  return fire_and_forget{};
}

// Retrieves IP addresses from local machine.
std::vector<std::string> WifiLanServerSocket::GetIpAddresses() const {
  std::vector<std::string> result{};
  auto host_names = NetworkInformation::GetHostNames();
  for (auto host_name : host_names) {
    if (host_name.IPInformation() != nullptr &&
        host_name.IPInformation().NetworkAdapter() != nullptr &&
        host_name.Type() == HostNameType::Ipv4) {
      std::string ipv4_s = winrt::to_string(host_name.ToString());
      // Converts ip address from x.x.x.x to 4 bytes format.
      in_addr address;
      address.S_un.S_addr = inet_addr(ipv4_s.c_str());
      char ipv4_b[5];
      ipv4_b[0] = address.S_un.S_un_b.s_b1;
      ipv4_b[1] = address.S_un.S_un_b.s_b2;
      ipv4_b[2] = address.S_un.S_un_b.s_b3;
      ipv4_b[3] = address.S_un.S_un_b.s_b4;
      ipv4_b[4] = 0;
      std::string ipv4_b_s = std::string(ipv4_b, 4);

      result.push_back(ipv4_b_s);
    }
  }
  return result;
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
