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

#ifndef PLATFORM_IMPL_G3_WIFI_DIRECT_H_
#define PLATFORM_IMPL_G3_WIFI_DIRECT_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/g3/multi_thread_executor.h"
#include "internal/platform/implementation/g3/socket_base.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace g3 {

class WifiDirectMedium;

class WifiDirectSocket : public api::WifiDirectSocket, public SocketBase {
 public:
  // Returns the InputStream of the WifiDirectSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiDirectSocket object is destroyed.
  InputStream& GetInputStream() override {
    return SocketBase::GetInputStream();
  }

  // Returns the OutputStream of the WifiDirectSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiDirectSocket object is destroyed.
  OutputStream& GetOutputStream() override {
    return SocketBase::GetOutputStream();
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override { return SocketBase::Close(); }
};

// WifiDirectServerSocket provides the support to server socket, this server
// socket accepts connection from clients.
class WifiDirectServerSocket : public api::WifiDirectServerSocket {
 public:
  ~WifiDirectServerSocket() override;

  static std::string GetName(absl::string_view ip_address, int port);

  std::string GetIPAddress() const override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return ip_address_;
  }

  void SetIPAddress(const std::string& ip_address) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    ip_address_ = ip_address;
  }

  int GetPort() const override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return port_;
  }

  void SetPort(int port) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    port_ = port;
  }

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  std::unique_ptr<api::WifiDirectSocket> Accept() override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Blocks until either:
  // - connection is available, or
  // - server socket is closed, or
  // - error happens.
  //
  // Called by the client side of a connection.
  // Returns true, if socket is successfully connected.
  bool Connect(WifiDirectSocket& socket) ABSL_LOCKS_EXCLUDED(mutex_);

  // Called by the server side of a connection before passing ownership of
  // WifiDirectServerSocker to user, to track validity of a pointer to this
  // server socket.
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  // Calls close_notifier if it was previously set, and marks socket as closed.
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // Retrieves IP addresses from local machine
  std::vector<std::string> GetIpAddresses() const;
  std::string GetDirectGOIpAddresses() const;

  Exception DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  std::string ip_address_ ABSL_GUARDED_BY(mutex_);
  int port_ ABSL_GUARDED_BY(mutex_);
  absl::CondVar cond_;
  absl::flat_hash_set<WifiDirectSocket*> pending_sockets_
      ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> close_notifier_ ABSL_GUARDED_BY(mutex_);
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

// Container of operations that can be performed over the WifiDirect medium.
class WifiDirectMedium : public api::WifiDirectMedium {
 public:
  WifiDirectMedium();
  ~WifiDirectMedium() override;

  WifiDirectMedium(const WifiDirectMedium&) = delete;
  WifiDirectMedium(WifiDirectMedium&&) = delete;
  WifiDirectMedium& operator=(const WifiDirectMedium&) = delete;
  WifiDirectMedium& operator=(WifiDirectMedium&&) = delete;

  // If the WiFi Adaptor supports to start a WifiDirect interface.
  bool IsInterfaceValid() const override { return true; }

  // Discoverer connects to server socket
  std::unique_ptr<api::WifiDirectSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag* cancellation_flag) override;

  // Advertiser starts to listen on server socket
  std::unique_ptr<api::WifiDirectServerSocket> ListenForService(
      int port) override;

  // Advertiser start WiFiDirect GO with specific Crendentials
  bool StartWifiDirect(WifiDirectCredentials* wifi_direct_credentials) override;
  // Advertiser stop the current WiFiDirect GO
  bool StopWifiDirect() override;
  // Discoverer connects to the WiFiDirect GO
  bool ConnectWifiDirect(
      WifiDirectCredentials* wifi_direct_credentials) override;
  // Discoverer disconnects from the WiFiDirect GO
  bool DisconnectWifiDirect() override;

  std::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override {
    return std::nullopt;
  }

 private:
  absl::Mutex mutex_;

  absl::flat_hash_map<std::string, WifiDirectServerSocket*> server_sockets_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_WIFI_DIRECT_H_
