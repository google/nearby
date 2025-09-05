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
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/exception.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/nearby_server_socket.h"
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
  // The result of this function is used in BWU to let the remote side know
  // which IP to connect to.
  // server_socket_ is not bound to any addresses.  So we need to pick an
  // IP address from the list of available addresses.
  std::vector<std::string> ip_addresses = GetIpv4Addresses();
  if (ip_addresses.empty()) {
    LOG(ERROR) << "No IP addresses found.";
    return "";
  }
  return ipaddr_dotdecimal_to_4bytes_string(ip_addresses.front());
}

// Returns socket port.
int WifiLanServerSocket::GetPort() const {
  return server_socket_.GetPort();
}

// Blocks until either:
// - at least one incoming connection request is available, or
// - ServerSocket is closed.
// On success, returns connected socket, ready to exchange data.
// Returns nullptr on error.
// Once error is reported, it is permanent, and ServerSocket has to be closed.
std::unique_ptr<api::WifiLanSocket> WifiLanServerSocket::Accept() {
  auto client_socket = server_socket_.Accept();
  if (client_socket == nullptr) {
    return nullptr;
  }

  LOG(INFO) << __func__ << ": Accepted a remote connection.";

  return std::make_unique<WifiLanSocket>(std::move(client_socket));
}

void WifiLanServerSocket::SetCloseNotifier(
    absl::AnyInvocable<void()> notifier) {
  close_notifier_ = std::move(notifier);
}

// Returns Exception::kIo on error, Exception::kSuccess otherwise.
Exception WifiLanServerSocket::Close() {
  try {
    absl::MutexLock lock(mutex_);
    VLOG(1) << __func__ << ": Close is called.";
    if (closed_) {
      return {Exception::kSuccess};
    }

    LOG(INFO) << __func__ << ": closing blocking socket.";

    server_socket_.Close();
    closed_ = true;

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
  // Listen on all interfaces.
  if (!server_socket_.Listen("", port_)) {
    LOG(ERROR) << "Failed to listen socket at port:" << port_;
    return false;
  }
  return true;
}

fire_and_forget WifiLanServerSocket::Listener_ConnectionReceived(
    StreamSocketListener listener,
    StreamSocketListenerConnectionReceivedEventArgs const& args) {
  absl::MutexLock lock(mutex_);
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
