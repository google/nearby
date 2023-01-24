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

#ifndef PLATFORM_IMPL_G3_WIFI_LAN_H_
#define PLATFORM_IMPL_G3_WIFI_LAN_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/g3/multi_thread_executor.h"
#include "internal/platform/implementation/g3/pipe.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace g3 {

class WifiLanMedium;

class WifiLanSocket : public api::WifiLanSocket {
 public:
  WifiLanSocket() = default;
  ~WifiLanSocket() override;

  // Connect to another WifiLanSocket, to form a functional low-level channel.
  // from this point on, and until Close is called, connection exists.
  void Connect(WifiLanSocket& other) ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the InputStream of this connected WifiLanSocket.
  InputStream& GetInputStream() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the OutputStream of this connected WifiLanSocket.
  // This stream is for local side to write.
  OutputStream& GetOutputStream() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns address of a remote WifiLanSocket or nullptr.
  WifiLanSocket* GetRemoteSocket() ABSL_LOCKS_EXCLUDED(mutex_);

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
  WifiLanSocket* remote_socket_ ABSL_GUARDED_BY(mutex_) = nullptr;
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

class WifiLanServerSocket : public api::WifiLanServerSocket {
 public:
  static std::string GetName(const std::string& ip_address, int port);

  ~WifiLanServerSocket() override;

  // Gets ip address.
  std::string GetIPAddress() const override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return ip_address_;
  }

  // Sets the ip address.
  void SetIPAddress(const std::string& ip_address) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    ip_address_ = ip_address;
  }

  // Gets the port.
  int GetPort() const override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return port_;
  }

  // Sets the port.
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
  //
  // Called by the server side of a connection.
  // Returns WifiLanSocket to the server side.
  // If not null, returned socket is connected to its remote (client-side) peer.
  std::unique_ptr<api::WifiLanSocket> Accept() override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Blocks until either:
  // - connection is available, or
  // - server socket is closed, or
  // - error happens.
  //
  // Called by the client side of a connection.
  // Returns true, if socket is successfully connected.
  bool Connect(WifiLanSocket& socket) ABSL_LOCKS_EXCLUDED(mutex_);

  // Called by the server side of a connection before passing ownership of
  // WifiLanServerSocker to user, to track validity of a pointer to this
  // server socket.
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  // Calls close_notifier if it was previously set, and marks socket as closed.
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  Exception DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  std::string ip_address_ ABSL_GUARDED_BY(mutex_);
  int port_ ABSL_GUARDED_BY(mutex_);
  absl::CondVar cond_;
  absl::flat_hash_set<WifiLanSocket*> pending_sockets_ ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> close_notifier_ ABSL_GUARDED_BY(mutex_);
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMedium : public api::WifiLanMedium {
 public:
  WifiLanMedium();
  ~WifiLanMedium() override;

  // Check if a network connection to a primary router exist.
  bool IsNetworkConnected() const override { return true; }

  // Starts WifiLan advertising.
  //
  // nsd_service_info - NsdServiceInfo data that's advertised through mDNS
  //                    service.
  // On success if the service is now advertising.
  // On error if the service cannot start to advertise or the service type in
  // NsdServiceInfo has been passed previously which StopAdvertising is not
  // been called.
  bool StartAdvertising(const NsdServiceInfo& nsd_service_info) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Stops WifiLan advertising.
  //
  // nsd_service_info - NsdServiceInfo data that's advertised through mDNS
  //                    service.
  // On success if the service stops advertising.
  // On error if the service cannot stop advertising or the service type in
  // NsdServiceInfo cannot be found.
  bool StopAdvertising(const NsdServiceInfo& nsd_service_info) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts the discovery of nearby WifiLan services.
  //
  // Returns true once the WifiLan discovery has been initiated. The
  // service_type is associated with callback.
  bool StartDiscovery(const std::string& service_type,
                      DiscoveredServiceCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Stops the discovery of nearby WifiLan services.
  //
  // service_type - The one assigend in StartDiscovery.
  // On success if service_type is matched to the callback and will be removed
  //            from the list. If list is empty then stops the WifiLan discovery
  //            service.
  // On error if the service_type is not existed, then return immediately.
  bool StopDiscovery(const std::string& service_type) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Connects to a WifiLan service.
  // On success, returns a new WifiLanSocket.
  // On error, returns nullptr.
  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const NsdServiceInfo& remote_service_info,
      CancellationFlag* cancellation_flag) override ABSL_LOCKS_EXCLUDED(mutex_);

  // Connects to a WifiLan service by ip address and port.
  // On success, returns a new WifiLanSocket.
  // On error, returns nullptr.
  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const std::string& ip_address, int port,
      CancellationFlag* cancellation_flag) override ABSL_LOCKS_EXCLUDED(mutex_);

  // Listens for incoming connection.
  //
  // port - A port number.
  //         0 : use a random port.
  //   1~65536 : open a server socket on that exact port.
  // On success, returns a new WifiLanServerSocket.
  // On error, returns nullptr.
  std::unique_ptr<api::WifiLanServerSocket> ListenForService(int port) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the port range as a pair of min and max port.
  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override {
    return std::make_pair(49152, 65535);
  }

 private:
  struct AdvertisingInfo {
    bool Empty() const { return service_types.empty(); }
    void Clear() { service_types.clear(); }
    void Add(const std::string& service_type) {
      service_types.insert(service_type);
    }
    void Remove(const std::string& service_type) {
      service_types.erase(service_type);
    }
    bool Existed(const std::string& service_type) const {
      return service_types.contains(service_type);
    }

    absl::flat_hash_set<std::string> service_types;
  };
  struct DiscoveringInfo {
    bool Empty() const { return service_types.empty(); }
    void Clear() { service_types.clear(); }
    void Add(const std::string& service_type) {
      service_types.insert(service_type);
    }
    void Remove(const std::string& service_type) {
      service_types.erase(service_type);
    }
    bool Existed(const std::string& service_type) const {
      return service_types.contains(service_type);
    }

    absl::flat_hash_set<std::string> service_types;
  };

  absl::Mutex mutex_;
  AdvertisingInfo advertising_info_ ABSL_GUARDED_BY(mutex_);
  DiscoveringInfo discovering_info_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, WifiLanServerSocket*> server_sockets_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_WIFI_LAN_H_
