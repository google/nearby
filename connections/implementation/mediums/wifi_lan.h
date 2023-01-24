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

#ifndef CORE_INTERNAL_MEDIUMS_WIFI_LAN_H_
#define CORE_INTERNAL_MEDIUMS_WIFI_LAN_H_

#include <cstdint>
#include <functional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/mutex.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/wifi_lan.h"

namespace nearby {
namespace connections {

class WifiLan {
 public:
  using DiscoveredServiceCallback = WifiLanMedium::DiscoveredServiceCallback;

  // Callback that is invoked when a new connection is accepted.
  struct AcceptedConnectionCallback {
    std::function<void(const std::string& service_id, WifiLanSocket socket)>
        accepted_cb = [](const std::string&, WifiLanSocket) {};
  };

  WifiLan() = default;
  ~WifiLan();

  // Returns true, if WifiLan communications are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Sets custom service info name, endpoint info name in NsdServiceInfo and
  // then enables WifiLan advertising.
  // Returns true, if NsdServiceInfo is successfully set, and false otherwise.
  bool StartAdvertising(const std::string& service_id,
                        NsdServiceInfo& nsd_service_info)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables WifiLan advertising.
  // Returns false if no successful call StartAdvertising() was previously
  // made, otherwise returns true.
  bool StopAdvertising(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAdvertising(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Enables WifiLan discovery. Will report any discoverable services
  // through a callback.
  // Returns true, if discovery was enabled, false otherwise.
  bool StartDiscovery(const std::string& service_id,
                      DiscoveredServiceCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables WifiLan discovery.
  bool StopDiscovery(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsDiscovering(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts a worker thread, creates a WifiLan socket, associates it with a
  // service id.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Closes socket corresponding to a service id.
  bool StopAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Establishes connection to WifiLan service that was might be started on
  // another service with StartAcceptingConnections() using the same service_id.
  // Blocks until connection is established, or server-side is terminated.
  // Returns socket instance. On success, WifiLanSocket.IsValid() return true.
  WifiLanSocket Connect(const std::string& service_id,
                        const NsdServiceInfo& service_info,
                        CancellationFlag* cancellation_flag)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Establishes connection to WifiLan service by ip address and port for
  // bandwidth upgradation.
  // Returns socket instance. On success, WifiLanSocket.IsValid() return true.
  WifiLanSocket Connect(const std::string& service_id,
                        const std::string& ip_address, int port,
                        CancellationFlag* cancellation_flag)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Gets ip address + port for remote services on the network to identify and
  // connect to this service.
  //
  // Credential is for the currently-hosted Wifi ServerSocket (if any).
  std::pair<std::string, int> GetCredentials(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  struct AdvertisingInfo {
    bool Empty() const { return nsd_service_infos.empty(); }
    void Clear() { nsd_service_infos.clear(); }
    void Add(const std::string& service_id,
             const NsdServiceInfo& nsd_service_info) {
      nsd_service_infos.insert({service_id, nsd_service_info});
    }
    void Remove(const std::string& service_id) {
      nsd_service_infos.erase(service_id);
    }
    bool Existed(const std::string& service_id) const {
      return nsd_service_infos.contains(service_id);
    }
    NsdServiceInfo* GetServiceInfo(const std::string& service_id) {
      const auto& it = nsd_service_infos.find(service_id);
      if (it == nsd_service_infos.end()) {
        return nullptr;
      }
      return &it->second;
    }

    absl::flat_hash_map<std::string, NsdServiceInfo> nsd_service_infos;
  };

  struct DiscoveringInfo {
    bool Empty() const { return service_ids.empty(); }
    void Clear() { service_ids.clear(); }
    void Add(const std::string& service_id) { service_ids.insert(service_id); }
    void Remove(const std::string& service_id) {
      service_ids.erase(service_id);
    }
    bool Existed(const std::string& service_id) const {
      return service_ids.contains(service_id);
    }

    absl::flat_hash_set<std::string> service_ids;
  };

  static constexpr int kMaxConcurrentAcceptLoops = 5;

  // Same as IsAvailable(), but must be called with mutex_ held.
  bool IsAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsAdvertising(), but must be called with mutex_ held.
  bool IsAdvertisingLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsDiscovering(), but must be called with mutex_ held.
  bool IsDiscoveringLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsAcceptingConnections(), but must be called with mutex_ held.
  bool IsAcceptingConnectionsLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Generates mDNS type.
  std::string GenerateServiceType(const std::string& service_id);

  // Generates port number based on port_range_.
  int GeneratePort(const std::string& service_id,
                   std::pair<std::int32_t, std::int32_t> port_range);

  mutable Mutex mutex_;
  WifiLanMedium medium_ ABSL_GUARDED_BY(mutex_);
  AdvertisingInfo advertising_info_ ABSL_GUARDED_BY(mutex_);
  DiscoveringInfo discovering_info_ ABSL_GUARDED_BY(mutex_);

  // A thread pool dedicated to running all the accept loops from
  // StartAcceptingConnections().
  MultiThreadExecutor accept_loops_runner_{kMaxConcurrentAcceptLoops};

  // A map of service_id -> ServerSocket. If map is non-empty, we
  // are currently listening for incoming connections.
  // WifiLanServerSocket instances are used from accept_loops_runner_,
  // and thus require pointer stability.
  absl::flat_hash_map<std::string, WifiLanServerSocket> server_sockets_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WIFI_LAN_H_
