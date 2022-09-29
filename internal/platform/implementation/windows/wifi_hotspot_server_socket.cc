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
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/wifi_hotspot.h"
#include "internal/platform/logging.h"

namespace location {
namespace nearby {
namespace windows {
namespace {
using ::winrt::Windows::Networking::Sockets::SocketQualityOfService;
}  // namespace

WifiHotspotServerSocket::WifiHotspotServerSocket(int port) : port_(port) {}

WifiHotspotServerSocket::~WifiHotspotServerSocket() { Close(); }

std::string WifiHotspotServerSocket::GetIPAddress() const {
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

int WifiHotspotServerSocket::GetPort() const {
  if (stream_socket_listener_ == nullptr) {
    return 0;
  }

  return std::stoi(stream_socket_listener_.Information().LocalPort().c_str());
}

std::unique_ptr<api::WifiHotspotSocket> WifiHotspotServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << __func__ << ": Accept is called.";

  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  if (closed_) return {};

  StreamSocket wifi_hotspot_socket = pending_sockets_.front();
  pending_sockets_.pop_front();

  NEARBY_LOGS(INFO) << __func__ << ": Accepted a remote connection.";
  return std::make_unique<WifiHotspotSocket>(wifi_hotspot_socket);
}

void WifiHotspotServerSocket::SetCloseNotifier(std::function<void()> notifier) {
  close_notifier_ = std::move(notifier);
}

Exception WifiHotspotServerSocket::Close() {
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

      for (const auto &pending_socket : pending_sockets_) {
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

bool WifiHotspotServerSocket::listen() {
  // Get current IP addresses of the device.
  for (int i = 0; i < kMaxRetries; i++) {
    hotspot_ipaddr_ = GetHotspotIpAddresses();
    if (hotspot_ipaddr_.empty()) {
      NEARBY_LOGS(WARNING) << "Failed to find Hotspot's IP addr for the try: "
                           << i + 1 << ". Wait " << kRetryIntervalMilliSeconds
                           << "ms snd try again";
      Sleep(kRetryIntervalMilliSeconds);
    } else {
      break;
    }
  }
  if (hotspot_ipaddr_.empty()) {
    NEARBY_LOGS(WARNING) << "Failed to start accepting connection without IP "
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
      {this, &WifiHotspotServerSocket::Listener_ConnectionReceived});

  try {
    HostName host_name{winrt::to_hstring(hotspot_ipaddr_)};
    stream_socket_listener_
        .BindEndpointAsync(host_name, winrt::to_hstring(port_))
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
    // need to save the port information.
    port_ =
        std::stoi(stream_socket_listener_.Information().LocalPort().c_str());
    NEARBY_LOGS(INFO) << "Server Socket port: " << port_;
    return true;
  } catch (...) {
    // Cannot bind to the preferred port, will let system to assign port.
    NEARBY_LOGS(ERROR) << "cannot bind to any port.";
  }

  return false;
}

fire_and_forget WifiHotspotServerSocket::Listener_ConnectionReceived(
    StreamSocketListener listener,
    StreamSocketListenerConnectionReceivedEventArgs const &args) {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << __func__ << ": Received connection.";

  if (closed_) {
    return fire_and_forget{};
  }

  pending_sockets_.push_back(args.Socket());
  cond_.SignalAll();
  return fire_and_forget{};
}

bool HasEnding(std::string const &full_string, std::string const &ending) {
  if (full_string.length() >= ending.length()) {
    return (0 == full_string.compare(full_string.length() - ending.length(),
                                     ending.length(), ending));
  } else {
    return false;
  }
}

std::vector<std::string> WifiHotspotServerSocket::GetIpAddresses() const {
  std::vector<std::string> result{};
  auto host_names = NetworkInformation::GetHostNames();
  for (auto host_name : host_names) {
    if (host_name.IPInformation() != nullptr &&
        host_name.IPInformation().NetworkAdapter() != nullptr &&
        host_name.Type() == HostNameType::Ipv4) {
      std::string ipv4_s = winrt::to_string(host_name.ToString());

      if (HasEnding(ipv4_s, ".1")) {
        NEARBY_LOGS(INFO) << "Found Hotspot IP: " << ipv4_s;
        result.push_back(ipv4_s);
      }
    }
  }
  return result;
}

std::string WifiHotspotServerSocket::GetHotspotIpAddresses() const {
  try {
    for (int i = 0; i < kMaxRetries; i++) {
      auto host_names = NetworkInformation::GetHostNames();
      for (auto host_name : host_names) {
        if (host_name.IPInformation() != nullptr &&
            host_name.IPInformation().NetworkAdapter() != nullptr &&
            host_name.Type() == HostNameType::Ipv4) {
          std::string ipv4_s = winrt::to_string(host_name.ToString());
          if (HasEnding(ipv4_s, ".1")) {
            // TODO(b/228541380): replace when we find a better way to
            // identifying the hotspot address
            NEARBY_LOGS(INFO) << "Found Hotspot IP: " << ipv4_s;
            return ipv4_s;
          }
        }
      }
    }
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception to GetHotspotIpAddresses: "
                       << exception.what();
    return {};
  } catch (const winrt::hresult_error &ex) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Exception to GetHotspotIpAddresses: " << ex.code()
                       << ": " << winrt::to_string(ex.message());
    return {};
  }
  return {};
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
