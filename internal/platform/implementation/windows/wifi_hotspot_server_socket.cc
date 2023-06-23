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
#include "internal/flags/nearby_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/wifi_hotspot.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {
namespace {
using ::winrt::Windows::Networking::Sockets::SocketQualityOfService;
}  // namespace

WifiHotspotServerSocket::WifiHotspotServerSocket(int port) : port_(port) {}

WifiHotspotServerSocket::~WifiHotspotServerSocket() { Close(); }

std::string WifiHotspotServerSocket::GetIPAddress() const {
  if (NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableHotspotWin32Socket)) {
    if (listen_socket_ == INVALID_SOCKET) {
      return {};
    }
  } else {
    if (stream_socket_listener_ == nullptr) {
      return {};
    }
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
  if (NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableHotspotWin32Socket)) {
    if (listen_socket_ == INVALID_SOCKET) {
      NEARBY_LOGS(WARNING) << __func__ << ": listen_socket_ is invalid.";
      return 0;
    }
    return port_;
  } else {
    if (stream_socket_listener_ == nullptr) {
      return 0;
    }
    return std::stoi(stream_socket_listener_.Information().LocalPort().c_str());
  }
}

std::unique_ptr<api::WifiHotspotSocket> WifiHotspotServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << __func__ << ": Accept is called.";

  if (NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableHotspotWin32Socket)) {
    while (!closed_ && pending_client_sockets_.empty()) {
      cond_.Wait(&mutex_);
    }
    if (closed_) return {};

    SOCKET wifi_hotspot_socket = pending_client_sockets_.front();
    pending_client_sockets_.pop_front();
    NEARBY_LOGS(INFO) << __func__ << ": Accepted a remote connection.";
    return std::make_unique<WifiHotspotSocket>(wifi_hotspot_socket);
  }

  // Code when using WinRT API
  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  if (closed_) return {};

  StreamSocket wifi_hotspot_socket = pending_sockets_.front();
  pending_sockets_.pop_front();
  NEARBY_LOGS(INFO) << __func__ << ": Accepted a remote connection.";
  return std::make_unique<WifiHotspotSocket>(wifi_hotspot_socket);
}

void WifiHotspotServerSocket::SetCloseNotifier(
    absl::AnyInvocable<void()> notifier) {
  close_notifier_ = std::move(notifier);
}

Exception WifiHotspotServerSocket::Close() {
  try {
    absl::MutexLock lock(&mutex_);
    NEARBY_LOGS(INFO) << __func__ << ": Close is called.";

    if (closed_) {
      return {Exception::kSuccess};
    }

    if (NearbyFlags::GetInstance().GetBoolFlag(
            platform::config_package_nearby::nearby_platform_feature::
                kEnableHotspotWin32Socket)) {
      if (listen_socket_ != INVALID_SOCKET) {
        NEARBY_LOGS(INFO) << ": Close listen_socket_: " << listen_socket_;
        closesocket(listen_socket_);
        closesocket(client_socket_);
        listen_socket_ = INVALID_SOCKET;
        client_socket_ = INVALID_SOCKET;
        for (const auto &pending_socket : pending_client_sockets_) {
          if (pending_socket != INVALID_SOCKET) closesocket(pending_socket);
        }
        WSACleanup();

        pending_client_sockets_ = {};
      }
      submittable_executor_.Shutdown();
    } else {
      if (stream_socket_listener_ != nullptr) {
        stream_socket_listener_.ConnectionReceived(listener_event_token_);
        stream_socket_listener_.Close();
        stream_socket_listener_ = nullptr;

        for (const auto &pending_socket : pending_sockets_) {
          pending_socket.Close();
        }

        pending_sockets_ = {};
      }
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

bool WifiHotspotServerSocket::SetupServerSocketWinRT() {
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

// Checks for SOCKET_ERROR, this error can come up when trying to bind, listen,
// Getsockname, WSACreateEvent, WSAEventSelect etc.
void SocketErrorNotice(SOCKET socket_to_close, const char *action) {
  // const char *actionAttempted = action;
  NEARBY_LOGS(WARNING) << "socket error. " << action
                       << " failed with error: " << WSAGetLastError();

  closesocket(socket_to_close);
  WSACleanup();
}

bool WifiHotspotServerSocket::SetupServerSocketWinSock() {
  WSADATA wsa_data;
  WSAEVENT socket_event;
  int flag = 1;

  int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (result != 0) {
    NEARBY_LOGS(WARNING) << "WSAStartup failed with error:" << result;
    return false;
  }

  listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_socket_ == INVALID_SOCKET) {
    NEARBY_LOGS(WARNING) << "Failed to get socket";
    WSACleanup();
    return false;
  }
  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port_);
  serv_addr.sin_addr.s_addr = inet_addr(hotspot_ipaddr_.c_str());

  unsigned long qos = 1;  // NOLINT
  ioctlsocket(listen_socket_, SIO_SET_QOS, &qos);
  setsockopt(listen_socket_, SOL_SOCKET, SO_KEEPALIVE, (const char *)&flag,
             sizeof(flag));
  if (bind(listen_socket_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) ==
      SOCKET_ERROR) {
    SocketErrorNotice(listen_socket_, "Bind");
    return false;
  }
  NEARBY_LOGS(INFO) << "Bind socket successful";

  int size = sizeof(serv_addr);
  memset(&serv_addr, 0, size);
  if (getsockname(listen_socket_, (struct sockaddr *)&serv_addr, &size) ==
      SOCKET_ERROR) {
    SocketErrorNotice(listen_socket_, "Getsockname");
    return false;
  }
  port_ = ntohs(serv_addr.sin_port);
  NEARBY_LOGS(INFO) << "Hotspot Server bound to port: " << port_;

  socket_event = WSACreateEvent();
  if (socket_event == nullptr) {
    SocketErrorNotice(listen_socket_, "WSACreateEvent");
    return false;
  }
  // Associate event types FD_ACCEPT and FD_CLOSE with the listen_socket_ and
  // socket_event
  if (WSAEventSelect(listen_socket_, socket_event, FD_ACCEPT | FD_CLOSE) ==
      SOCKET_ERROR) {
    SocketErrorNotice(listen_socket_, "WSAEventSelect");
    return false;
  }

  if (::listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR) {
    SocketErrorNotice(listen_socket_, "Listen");
    return false;
  }
  NEARBY_LOGS(INFO) << "Hotspot Server Socket " << listen_socket_
                    << " started to listen with socket event: " << socket_event;

  submittable_executor_.Execute([this, socket_event]() {
    DWORD index;
    WSANETWORKEVENTS network_events;
    // Wait for network events on all sockets
    index =
        WSAWaitForMultipleEvents(1, &socket_event, FALSE, WSA_INFINITE, FALSE);

    NEARBY_LOGS(INFO) << "Hotspot Server Socket " << listen_socket_
                      << " received event: " << socket_event;
    if (index == WSA_WAIT_TIMEOUT || index == WSA_WAIT_FAILED) {
      NEARBY_LOGS(INFO) << "Hotspot Server Socket timout or failed ";
      return false;
    }

    index = index - WSA_WAIT_EVENT_0;
    // Iterate through all events and enumerate
    if (WSAEnumNetworkEvents(listen_socket_, socket_event, &network_events) ==
        SOCKET_ERROR) {
      NEARBY_LOGS(INFO) << "Iterate through all events failed";
      return false;
    }
    if (network_events.lNetworkEvents & FD_CLOSE) {
      NEARBY_LOGS(INFO) << "Reveived FD_CLOSE event";
      return false;
    }
    if (network_events.lNetworkEvents & FD_ACCEPT) {
      client_socket_ = accept(listen_socket_, nullptr, nullptr);
      NEARBY_LOGS(INFO) << "Reveived FD_ACCEPT event.";

      if (client_socket_ == INVALID_SOCKET) {
        return false;
      }

      if (WSAEventSelect(listen_socket_, socket_event, 0) == SOCKET_ERROR) {
        NEARBY_LOGS(WARNING)
            << "Remove association between listen_socket_ and event failed: "
            << WSAGetLastError();
      }

      NEARBY_LOGS(INFO) << "Hotspot Server Client Socket created: "
                        << client_socket_;
      if (closed_) {
        return false;
      }
      {
        absl::MutexLock lock(&mutex_);
        pending_client_sockets_.push_back(client_socket_);
        cond_.SignalAll();
      }
    }
    return true;
  });
  return true;
}

bool WifiHotspotServerSocket::listen() {
  // Get current IP addresses of the device.
  int64_t ip_address_max_retries = NearbyFlags::GetInstance().GetInt64Flag(
      platform::config_package_nearby::nearby_platform_feature::
          kWifiHotspotCheckIpMaxRetries);
  int64_t ip_address_retry_interval_millis =
      NearbyFlags::GetInstance().GetInt64Flag(
          platform::config_package_nearby::nearby_platform_feature::
              kWifiHotspotCheckIpIntervalMillis);
  NEARBY_LOGS(INFO) << "maximum IP check retries=" << ip_address_max_retries
                    << ", IP check interval="
                    << ip_address_retry_interval_millis << "ms";
  for (int i = 0; i < ip_address_max_retries; i++) {
    hotspot_ipaddr_ = GetHotspotIpAddresses();
    if (hotspot_ipaddr_.empty()) {
      NEARBY_LOGS(WARNING) << "Failed to find Hotspot's IP addr for the try: "
                           << i + 1 << ". Wait "
                           << ip_address_retry_interval_millis
                           << "ms snd try again";
      Sleep(ip_address_retry_interval_millis);
    } else {
      break;
    }
  }
  if (hotspot_ipaddr_.empty()) {
    NEARBY_LOGS(WARNING) << "Failed to start accepting connection without IP "
                            "addresses configured on computer.";
    return false;
  }

  if (NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableHotspotWin32Socket)) {
    return SetupServerSocketWinSock();
  } else {
    return SetupServerSocketWinRT();
  }
}

std::vector<std::string> WifiHotspotServerSocket::GetIpAddresses() const {
  std::vector<std::string> result;
  try {
    auto host_names = NetworkInformation::GetHostNames();
    for (auto host_name : host_names) {
      if (host_name.IPInformation() != nullptr &&
          host_name.IPInformation().NetworkAdapter() != nullptr &&
          host_name.Type() == HostNameType::Ipv4) {
        std::string ipv4_s = winrt::to_string(host_name.ToString());

        if (absl::EndsWith(ipv4_s, ".1")) {
          NEARBY_LOGS(INFO) << "Found Hotspot IP: " << ipv4_s;
          result.push_back(ipv4_s);
        }
      }
    }
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
  } catch (const winrt::hresult_error &error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
  }

  return result;
}

std::string WifiHotspotServerSocket::GetHotspotIpAddresses() const {
  try {
    int64_t ip_address_max_retries = NearbyFlags::GetInstance().GetInt64Flag(
        platform::config_package_nearby::nearby_platform_feature::
            kWifiHotspotCheckIpMaxRetries);

    for (int i = 0; i < ip_address_max_retries; i++) {
      auto host_names = NetworkInformation::GetHostNames();
      for (auto host_name : host_names) {
        if (host_name.IPInformation() != nullptr &&
            host_name.IPInformation().NetworkAdapter() != nullptr &&
            host_name.Type() == HostNameType::Ipv4) {
          std::string ipv4_s = winrt::to_string(host_name.ToString());
          if (absl::EndsWith(ipv4_s, ".1")) {
            // TODO(b/228541380): replace when we find a better way to
            // identifying the hotspot address
            NEARBY_LOGS(INFO) << "Found Hotspot IP: " << ipv4_s;
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
