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

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/wifi_lan.h"
#include "internal/platform/logging.h"

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
    LOG(ERROR) << "Failed to get IP address due to no server socket.";
    return "";
  }

  if (ip_addresses_.empty()) {
    LOG(ERROR) << "Failed to get IP address due to no avaible IP addresses.";
    return "";
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
  LOG(INFO) << __func__ << ": Accept is called.";

  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  if (closed_) return {};

  StreamSocket wifi_lan_socket = pending_sockets_.front();
  pending_sockets_.pop_front();

  LOG(INFO) << __func__ << ": Accepted a remote connection.";
  return std::make_unique<WifiLanSocket>(wifi_lan_socket);
}

void WifiLanServerSocket::SetCloseNotifier(
    absl::AnyInvocable<void()> notifier) {
  close_notifier_ = std::move(notifier);
}

// Returns Exception::kIo on error, Exception::kSuccess otherwise.
Exception WifiLanServerSocket::Close() {
  try {
    absl::MutexLock lock(&mutex_);
    LOG(INFO) << __func__ << ": Close is called.";

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

    LOG(INFO) << __func__ << ": Close completed succesfully.";
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    closed_ = true;
    cond_.SignalAll();
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    closed_ = true;
    cond_.SignalAll();
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    closed_ = true;
    cond_.SignalAll();
    LOG(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

bool WifiLanServerSocket::listen() {
  // Get current IP addresses of the device.
  ip_addresses_ = Get4BytesIpv4Addresses();

  if (ip_addresses_.empty()) {
    LOG(WARNING) << "failed to start accepting connection without IP "
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
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Cannot accept connection on preferred port. Exception: "
               << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR)
        << __func__
        << ": Cannot accept connection on preferred port. WinRT exception: "
        << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }

  try {
    stream_socket_listener_.BindServiceNameAsync({}).get();

    // Need to save the port information.
    port_ =
        std::stoi(stream_socket_listener_.Information().LocalPort().c_str());
    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Cannot bind to any port. Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__
               << ": Cannot bind to any port. WinRT exception: " << error.code()
               << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }

  return false;
}

fire_and_forget WifiLanServerSocket::Listener_ConnectionReceived(
    StreamSocketListener listener,
    StreamSocketListenerConnectionReceivedEventArgs const& args) {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << __func__ << ": Received connection.";

  if (closed_) {
    return fire_and_forget{};
  }

  pending_sockets_.push_back(args.Socket());
  cond_.SignalAll();
  return fire_and_forget{};
}

}  // namespace windows
}  // namespace nearby
