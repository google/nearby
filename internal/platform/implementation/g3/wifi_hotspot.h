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

#ifndef PLATFORM_IMPL_G3_WIFI_HOTSPOT_H_
#define PLATFORM_IMPL_G3_WIFI_HOTSPOT_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/g3/multi_thread_executor.h"
#include "internal/platform/implementation/g3/pipe.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace g3 {

class WifiHotspotMedium;

class WifiHotspotSocket : public api::WifiHotspotSocket {
 public:
  WifiHotspotSocket() = default;
  ~WifiHotspotSocket() override;
  WifiHotspotSocket(const WifiHotspotSocket&) = default;
  WifiHotspotSocket(WifiHotspotSocket&&) = default;
  WifiHotspotSocket& operator=(const WifiHotspotSocket&) = default;
  WifiHotspotSocket& operator=(WifiHotspotSocket&&) = default;

  // Connect to another WifiHotspotSocket, to form a functional low-level
  // channel. from this point on, and until Close is called, connection exists.
  void Connect(WifiHotspotSocket& other) ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the InputStream of the WifiHotspotSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiHotspotSocket object is destroyed.
  InputStream& GetInputStream() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the OutputStream of the WifiHotspotSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiHotspotSocket object is destroyed.
  OutputStream& GetOutputStream() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns address of a remote WifiHotspotSocket or nullptr.
  WifiHotspotSocket* GetRemoteSocket() ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if connection exists to the (possibly closed) remote socket.
  bool IsConnected() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if socket is closed.
  bool IsClosed() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  void DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Returns true if connection exists to the (possibly closed) remote socket.
  bool IsConnectedLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Returns InputStream of our side of a connection.
  // This is what the remote side is supposed to read from.
  // This is a helper for GetInputStream() method.
  InputStream& GetLocalInputStream() ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns OutputStream of our side of a connection.
  // This is what the local size is supposed to write to.
  // This is a helper for GetOutputStream() method.
  OutputStream& GetLocalOutputStream() ABSL_LOCKS_EXCLUDED(mutex_);

  // Output pipe is initialized by constructor, it remains always valid, until
  // it is closed. it represents output part of a local socket. Input part of a
  // local socket comes from the peer socket, after connection.
  std::shared_ptr<Pipe> output_{new Pipe};
  std::shared_ptr<Pipe> input_;
  mutable absl::Mutex mutex_;
  WifiHotspotSocket* remote_socket_ ABSL_GUARDED_BY(mutex_) = nullptr;
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

// WifiHotspotServerSocket provides the support to server socket, this server
// socket accepts connection from clients.
class WifiHotspotServerSocket : public api::WifiHotspotServerSocket {
 public:
  ~WifiHotspotServerSocket() override;

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
  std::unique_ptr<api::WifiHotspotSocket> Accept() override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Blocks until either:
  // - connection is available, or
  // - server socket is closed, or
  // - error happens.
  //
  // Called by the client side of a connection.
  // Returns true, if socket is successfully connected.
  bool Connect(WifiHotspotSocket& socket) ABSL_LOCKS_EXCLUDED(mutex_);

  // Called by the server side of a connection before passing ownership of
  // WifiHotspotServerSocker to user, to track validity of a pointer to this
  // server socket.
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  // Calls close_notifier if it was previously set, and marks socket as closed.
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // Retrieves IP addresses from local machine
  std::vector<std::string> GetIpAddresses() const;
  std::string GetHotspotIpAddresses() const;

  Exception DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  std::string ip_address_ ABSL_GUARDED_BY(mutex_);
  int port_ ABSL_GUARDED_BY(mutex_);
  absl::CondVar cond_;
  absl::flat_hash_set<WifiHotspotSocket*> pending_sockets_
      ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> close_notifier_ ABSL_GUARDED_BY(mutex_);
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

// Container of operations that can be performed over the WifiHotspot medium.
class WifiHotspotMedium : public api::WifiHotspotMedium {
 public:
  WifiHotspotMedium();
  ~WifiHotspotMedium() override;

  WifiHotspotMedium(const WifiHotspotMedium&) = delete;
  WifiHotspotMedium(WifiHotspotMedium&&) = delete;
  WifiHotspotMedium& operator=(const WifiHotspotMedium&) = delete;
  WifiHotspotMedium& operator=(WifiHotspotMedium&&) = delete;

  // If the WiFi Adaptor supports to start a Hotspot interface.
  bool IsInterfaceValid() const override { return true; }

  // Discoverer connects to server socket
  std::unique_ptr<api::WifiHotspotSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag* cancellation_flag) override;

  // Advertiser starts to listen on server socket
  std::unique_ptr<api::WifiHotspotServerSocket> ListenForService(
      int port) override;

  // Advertiser start WiFi Hotspot with specific Crendentials
  bool StartWifiHotspot(HotspotCredentials* hotspot_credentials) override;
  // Advertiser stop the current WiFi Hotspot
  bool StopWifiHotspot() override;
  // Discoverer connects to the Hotspot
  bool ConnectWifiHotspot(HotspotCredentials* hotspot_credentials) override;
  // Discoverer disconnects from the Hotspot
  bool DisconnectWifiHotspot() override;

  std::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override {
    return std::nullopt;
  }

 private:
  // Gets error message from exception pointer
  //  std::string GetErrorMessage(std::exception_ptr eptr);

  // Protects to access some members
  absl::Mutex mutex_;

  absl::flat_hash_map<std::string, WifiHotspotServerSocket*> server_sockets_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_WIFI_HOTSPOT_H_
