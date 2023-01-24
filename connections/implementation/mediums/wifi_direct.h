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

#ifndef CORE_INTERNAL_MEDIUMS_WIFI_DIRECT_H_
#define CORE_INTERNAL_MEDIUMS_WIFI_DIRECT_H_

#include <cstdint>
#include <functional>
#include <string>

#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/mutex.h"
#include "internal/platform/wifi_direct.h"

namespace nearby {
namespace connections {

class WifiDirect {
 public:
  // Callback that is invoked when a new connection is accepted.
  struct AcceptedConnectionCallback {
    std::function<void(const std::string& service_id, WifiDirectSocket socket)>
        accepted_cb = [](const std::string&, WifiDirectSocket) {};
  };

  WifiDirect() : is_go_started_(false), is_connected_to_go_(false) {}
  ~WifiDirect();
  // Not copyable or movable
  WifiDirect(const WifiDirect&) = delete;
  WifiDirect& operator=(const WifiDirect&) = delete;
  WifiDirect(WifiDirect&&) = delete;
  WifiDirect& operator=(WifiDirect&&) = delete;

  // Returns true, if WifiDirect Group Owner is supported by a platform.
  bool IsGOAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns true, if WifiDirect Group Client is supported by a platform.
  bool IsGCAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  // If WifiDirect Group Owner started
  bool IsGOStarted() ABSL_LOCKS_EXCLUDED(mutex_);
  // Start WifiDirect Group Owner. Returns true if AutoGO is successfully
  // started.
  bool StartWifiDirect() ABSL_LOCKS_EXCLUDED(mutex_);
  // Stop WifiDirect Group Owner
  bool StopWifiDirect() ABSL_LOCKS_EXCLUDED(mutex_);

  // If WifiDirect Group Client connects to Group Owner
  bool IsConnectedToGO() ABSL_LOCKS_EXCLUDED(mutex_);
  // WifiDirect Group Client request to connect to the Group Owner
  bool ConnectWifiDirect(const std::string& ssid, const std::string& password)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // WifiDirect Group Client request to disconnect from the Group Owner
  bool DisconnectWifiDirect() ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts a worker thread, creates a WifiDirect socket, associates it with a
  // service id.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Closes socket corresponding to a service id.
  bool StopAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Establishes connection to WifiDirect service by ip address and port for
  // bandwidth upgradation.
  // Returns socket instance. On success, WifiDirectSocket.IsValid() return
  // true.
  WifiDirectSocket Connect(const std::string& service_id,
                           const std::string& ip_address, int port,
                           CancellationFlag* cancellation_flag)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Gets SoftAP ssid + password + ip address + gateway + port etc for remote
  // services on the network to identify and connect to this service.
  // Credential is for the currently-hosted WiFi SoftAP ServerSocket (if any).
  WifiDirectCredentials* GetCredentials(absl::string_view service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  mutable Mutex mutex_;
  static constexpr int kMaxConcurrentAcceptLoops = 5;

  // Same as IsAvailable(), but must be called with mutex_ held.
  bool IsGOAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool IsGCAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  // Same as IsAcceptingConnections(), but must be called with mutex_ held.
  bool IsAcceptingConnectionsLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool is_go_started_ ABSL_GUARDED_BY(mutex_);
  bool is_connected_to_go_ ABSL_GUARDED_BY(mutex_);
  WifiDirectMedium medium_ ABSL_GUARDED_BY(mutex_);

  // A thread pool dedicated to running all the accept loops from
  // StartAcceptingConnections().
  MultiThreadExecutor accept_loops_runner_{kMaxConcurrentAcceptLoops};

  // A map of service_id -> ServerSocket. If map is non-empty, we are currently
  // listening for incoming connections. WifiDirectServerSocket instances are
  // used from accept_loops_runner_, and thus require pointer stability.
  absl::flat_hash_map<std::string, WifiDirectServerSocket> server_sockets_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WIFI_DIRECT_H_
