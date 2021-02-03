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

#include "platform/api/wifi_lan.h"
#include "platform/base/byte_array.h"
#include "platform/base/input_stream.h"
#include "platform/base/nsd_service_info.h"
#include "platform/base/output_stream.h"
#include "platform/impl/g3/multi_thread_executor.h"
#include "platform/impl/g3/pipe.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"

namespace location {
namespace nearby {
namespace g3 {

class WifiLanMedium;

// Opaque wrapper over a WifiLan service which contains |NsdServiceInfo|.
class WifiLanService : public api::WifiLanService {
 public:
  WifiLanService() = default;
  explicit WifiLanService(NsdServiceInfo nsd_service_info)
      : nsd_service_info_(std::move(nsd_service_info)) {}
  ~WifiLanService() override = default;

  NsdServiceInfo GetServiceInfo() const override { return nsd_service_info_; }

  void SetServiceInfo(NsdServiceInfo nsd_service_info) {
    nsd_service_info_ = std::move(nsd_service_info);
  }

  WifiLanMedium* GetMedium() { return medium_; }

  void SetMedium(WifiLanMedium* medium) { medium_ = medium; }

 private:
  NsdServiceInfo nsd_service_info_;
  WifiLanMedium* medium_ = nullptr;
};

class WifiLanSocket : public api::WifiLanSocket {
 public:
  WifiLanSocket() = default;
  explicit WifiLanSocket(WifiLanService* wifi_lan_service)
      : wifi_lan_service_(wifi_lan_service) {}
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

  // Returns valid WifiLanService pointer if there is a connection, and
  // nullptr otherwise.
  WifiLanService* GetRemoteWifiLanService() override
      ABSL_LOCKS_EXCLUDED(mutex_);

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
  WifiLanService* wifi_lan_service_;
  WifiLanSocket* remote_socket_ ABSL_GUARDED_BY(mutex_) = nullptr;
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

class WifiLanServerSocket {
 public:
  ~WifiLanServerSocket();

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
  std::unique_ptr<api::WifiLanSocket> Accept(WifiLanService* service)
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
  // server socket,
  void SetCloseNotifier(std::function<void()> notifier)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  // Calls close_notifier if it was previously set, and marks socket as closed.
  Exception Close() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  Exception DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  absl::Mutex mutex_;
  absl::CondVar cond_;
  absl::flat_hash_set<WifiLanSocket*> pending_sockets_ ABSL_GUARDED_BY(mutex_);
  std::function<void()> close_notifier_ ABSL_GUARDED_BY(mutex_);
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMedium : public api::WifiLanMedium {
 public:
  WifiLanMedium();
  ~WifiLanMedium() override;

  bool StartAdvertising(const std::string& service_id,
                        const NsdServiceInfo& nsd_service_info) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopAdvertising(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true once the WifiLan discovery has been initiated.
  bool StartDiscovery(const std::string& service_id,
                      DiscoveredServiceCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true once WifiLan discovery for service_id is well and truly
  // stopped; after this returns, there must be no more invocations of the
  // DiscoveredServiceCallback passed in to StartDiscovery() for service_id.
  bool StopDiscovery(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true once WifiLan socket connection requests to service_id can be
  // accepted.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopAcceptingConnections(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Connects to existing remote WifiLan service.
  //
  // On success, returns a new WifiLanSocket.
  // On error, returns nullptr.
  std::unique_ptr<api::WifiLanSocket> Connect(
      api::WifiLanService& remote_wifi_lan_service,
      const std::string& service_id,
      CancellationFlag* cancellation_flag) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  api::WifiLanService* GetRemoteService(const std::string& ip_address,
                                        int port) override;

  std::pair<std::string, int> GetServiceAddress(
      const std::string& service_id) override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  static constexpr int kMaxConcurrentAcceptLoops = 5;

  struct AdvertisingInfo {
    bool Empty() const { return service_id.empty(); }
    void Clear() { service_id.clear(); }

    std::string service_id;
  };

  struct DiscoveringInfo {
    bool Empty() const { return service_id.empty(); }
    void Clear() { service_id.clear(); }

    std::string service_id;
  };

  void SetWifiLanService(const NsdServiceInfo& nsd_service_info);
  std::pair<std::string, int> GetFakeServiceAddress() const;

  absl::Mutex mutex_;
  WifiLanService wifi_lan_service_;

  // A thread pool dedicated to running all the accept loops from
  // StartAdvertising().
  MultiThreadExecutor accept_loops_runner_{kMaxConcurrentAcceptLoops};
  std::atomic_bool acceptance_thread_running_ = false;

  // A thread pool dedicated to wait to complete the accept_loops_runner_.
  MultiThreadExecutor close_accept_loops_runner_{kMaxConcurrentAcceptLoops};

  // A server socket is established when start advertising.
  std::unique_ptr<WifiLanServerSocket> server_socket_;
  AdvertisingInfo advertising_info_ ABSL_GUARDED_BY(mutex_);
  DiscoveringInfo discovering_info_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_G3_WIFI_LAN_H_
