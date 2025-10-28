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

#ifndef PLATFORM_IMPL_WINDOWS_WIFI_HOTSPOT_SERVER_SOCKET_H_
#define PLATFORM_IMPL_WINDOWS_WIFI_HOTSPOT_SERVER_SOCKET_H_

// Windows headers
#include <windows.h>
#include <winsock2.h>
#include <wlanapi.h>

// Standard C/C++ headers
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <utility>

// Nearby connections headers
#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/windows/nearby_server_socket.h"

namespace nearby::windows {

// WifiHotspotServerSocket provides the support to server socket, this server
// socket accepts connection from clients.
class WifiHotspotServerSocket : public api::WifiHotspotServerSocket {
 public:
  explicit WifiHotspotServerSocket(int port = 0);
  WifiHotspotServerSocket(const WifiHotspotServerSocket&) = default;
  WifiHotspotServerSocket(WifiHotspotServerSocket&&) = default;
  ~WifiHotspotServerSocket() override;
  WifiHotspotServerSocket& operator=(const WifiHotspotServerSocket&) = default;
  WifiHotspotServerSocket& operator=(WifiHotspotServerSocket&&) = default;

  std::string GetIPAddress() const override;
  int GetPort() const override;

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  std::unique_ptr<api::WifiHotspotSocket> Accept() override;

  // Called by the server side of a connection before passing ownership of
  // WifiHotspotServerSocker to user, to track validity of a pointer to this
  // server socket.
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override;

  // Binds to local port
  bool Listen(bool dual_stack);

  NearbyServerSocket server_socket_;

 private:
  // Retrieves hotspot IP address from local machine
  std::string GetHotspotIpAddress() const;

  const int port_;
  mutable absl::Mutex mutex_;

  // Close notifier
  absl::AnyInvocable<void()> close_notifier_ = nullptr;

  // IP addresses of the computer. mDNS uses them to advertise.
  std::vector<std::string> ip_addresses_{};

  // Cache socket not be picked by upper layer
  std::string hotspot_ipaddr_ = {};
  bool closed_ = false;
};

}  // namespace nearby::windows

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_HOTSPOT_SERVER_SOCKET_H_
