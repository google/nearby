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
#include <vector>

// ABSL headers
#include "absl/strings/match.h"

// Nearby connections headers
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/wifi_direct.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {
namespace {
using ::winrt::Windows::Networking::Sockets::SocketQualityOfService;

constexpr int kMaxRetries = 3;
constexpr int kRetryIntervalMilliSeconds = 300;

}  // namespace

WifiDirectServerSocket::WifiDirectServerSocket(int port) : port_(port) {}

WifiDirectServerSocket::~WifiDirectServerSocket() { Close(); }

std::string WifiDirectServerSocket::GetIPAddress() const {
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

int WifiDirectServerSocket::GetPort() const {
  if (stream_socket_listener_ == nullptr) {
    return 0;
  }

  return std::stoi(stream_socket_listener_.Information().LocalPort().c_str());
}

std::unique_ptr<api::WifiDirectSocket> WifiDirectServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << __func__ << ": Accept is called.";

  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  if (closed_) return {};

  StreamSocket wifi_direct_socket = pending_sockets_.front();
  pending_sockets_.pop_front();

  NEARBY_LOGS(INFO) << __func__ << ": Accepted a remote connection.";
  return std::make_unique<WifiDirectSocket>(wifi_direct_socket);
}

void WifiDirectServerSocket::SetCloseNotifier(
    absl::AnyInvocable<void()> notifier) {
  close_notifier_ = std::move(notifier);
}

Exception WifiDirectServerSocket::Close() {
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
  } catch (std::exception exception) {
    closed_ = true;
    cond_.SignalAll();
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error &error) {
    closed_ = true;
    cond_.SignalAll();
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    closed_ = true;
    cond_.SignalAll();
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

bool WifiDirectServerSocket::listen() {
  // Get current IP addresses of the device.
  for (int i = 0; i < kMaxRetries; i++) {
    wifi_direct_go_ipaddr_ = GetDirectGOIpAddresses();
    if (wifi_direct_go_ipaddr_.empty()) {
      NEARBY_LOGS(WARNING)
          << "Failed to find WifiDirect GO's IP addr for the try: " << i + 1
          << ". Wait " << kRetryIntervalMilliSeconds << "ms snd try again";
      Sleep(kRetryIntervalMilliSeconds);
    } else {
      break;
    }
  }
  if (wifi_direct_go_ipaddr_.empty()) {
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
      {this, &WifiDirectServerSocket::Listener_ConnectionReceived});

  try {
    HostName host_name{winrt::to_hstring(wifi_direct_go_ipaddr_)};
    stream_socket_listener_
        .BindEndpointAsync(host_name, winrt::to_hstring(port_))
        .get();
    if (port_ == 0) {
      port_ =
          std::stoi(stream_socket_listener_.Information().LocalPort().c_str());
    }

    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Cannot accept connection on preferred port. Exception: "
        << exception.what();
  } catch (const winrt::hresult_error &error) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ":Cannot accept connection on preferred port.  WinRT exception: "
        << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
  }

  try {
    stream_socket_listener_.BindServiceNameAsync({}).get();
    // need to save the port information.
    port_ =
        std::stoi(stream_socket_listener_.Information().LocalPort().c_str());
    NEARBY_LOGS(INFO) << "Server Socket port: " << port_;
    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Cannot bind to any port. Exception: "
                       << exception.what();
  } catch (const winrt::hresult_error &error) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Cannot bind to any port. WinRT exception: "
                       << error.code() << ": "
                       << winrt::to_string(error.message());
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
  }

  return false;
}

fire_and_forget WifiDirectServerSocket::Listener_ConnectionReceived(
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

std::vector<std::string> WifiDirectServerSocket::GetIpAddresses() const {
  std::vector<std::string> result{};
  auto host_names = NetworkInformation::GetHostNames();
  for (auto host_name : host_names) {
    if (host_name.IPInformation() != nullptr &&
        host_name.IPInformation().NetworkAdapter() != nullptr &&
        host_name.Type() == HostNameType::Ipv4) {
      std::string ipv4_s = winrt::to_string(host_name.ToString());

      if (absl::EndsWith(ipv4_s, ".1")) {
        NEARBY_LOGS(INFO) << "Found WifiDirect GO IP: " << ipv4_s;
        result.push_back(ipv4_s);
      }
    }
  }
  return result;
}

std::string WifiDirectServerSocket::GetDirectGOIpAddresses() const {
  try {
    for (int i = 0; i < kMaxRetries; i++) {
      auto host_names = NetworkInformation::GetHostNames();
      for (auto host_name : host_names) {
        if (host_name.IPInformation() != nullptr &&
            host_name.IPInformation().NetworkAdapter() != nullptr &&
            host_name.Type() == HostNameType::Ipv4) {
          std::string ipv4_s = winrt::to_string(host_name.ToString());
          if (absl::EndsWith(ipv4_s, ".1")) {
            // TODO(b/228541380): replace when we find a better way to
            // identifying the WifiDirect GO IP address
            NEARBY_LOGS(INFO) << "Found WifiDirect GO IP: " << ipv4_s;
            return ipv4_s;
          }
        }
      }
    }
    return {};
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {};
  } catch (const winrt::hresult_error &error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {};
  }
}

}  // namespace windows
}  // namespace nearby
