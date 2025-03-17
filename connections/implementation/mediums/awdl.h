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

#ifndef CORE_INTERNAL_MEDIUMS_AWDL_H_
#define CORE_INTERNAL_MEDIUMS_AWDL_H_

#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/multiplex/multiplex_socket.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/expected.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/mutex.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/awdl.h"

namespace nearby {
namespace connections {

class Awdl {
 public:
  using DiscoveredServiceCallback = AwdlMedium::DiscoveredServiceCallback;

  // Callback that is invoked when a new connection is accepted.
  using AcceptedConnectionCallback = absl::AnyInvocable<void(
      const std::string& service_id, AwdlSocket socket)>;

  Awdl() = default;
  ~Awdl();

  // Returns true, if Awdl communications are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Sets custom service info name, endpoint info name in NsdServiceInfo and
  // then enables Awdl advertising.
  // Returns true, if NsdServiceInfo is successfully set, and false otherwise.
  ErrorOr<bool> StartAdvertising(const std::string& service_id,
                                 NsdServiceInfo& nsd_service_info)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables Awdl advertising.
  // Returns false if no successful call StartAdvertising() was previously
  // made, otherwise returns true.
  bool StopAdvertising(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAdvertising(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Enables Awdl discovery. Will report any discoverable services
  // through a callback.
  // Returns true, if discovery was enabled, false otherwise.
  ErrorOr<bool> StartDiscovery(const std::string& service_id,
                               DiscoveredServiceCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables Awdl discovery.
  bool StopDiscovery(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsDiscovering(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts a worker thread, creates a Awdl socket, associates it with a
  // service id.
  ErrorOr<bool> StartAcceptingConnections(const std::string& service_id,
                                          AcceptedConnectionCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Closes socket corresponding to a service id.
  bool StopAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Establishes connection to Awdl service that was might be started on
  // another service with StartAcceptingConnections() using the same service_id.
  // Blocks until connection is established, or server-side is terminated.
  // Returns socket instance. On success, AwdlSocket.IsValid() return true.
  ErrorOr<AwdlSocket> Connect(const std::string& service_id,
                                 const NsdServiceInfo& service_info,
                                 CancellationFlag* cancellation_flag)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Establishes connection to Awdl service by ip address and port for
  // bandwidth upgradation.
  // Returns socket instance. On success, AwdlSocket.IsValid() return true.
  ErrorOr<AwdlSocket> Connect(const std::string& service_id,
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

  // Establishes connection to Awdl service by ip address through
  // MultiplexSocket.
  ExceptionOr<AwdlSocket> ConnectWithMultiplexSocketLocked(
      const std::string& service_id, const std::string& ip_address)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Creates a MultiplexSocket for outgoing connection based on connected
  // AwdlSocket physical socket for specific service_id and ip address.
  ExceptionOr<AwdlSocket> CreateOutgoingMultiplexSocketLocked(
      AwdlSocket& socket, const std::string& service_id,
      const std::string& ip_address) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

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
  AwdlMedium medium_ ABSL_GUARDED_BY(mutex_);
  AdvertisingInfo advertising_info_ ABSL_GUARDED_BY(mutex_);
  DiscoveringInfo discovering_info_ ABSL_GUARDED_BY(mutex_);

  // A thread pool dedicated to running all the accept loops from
  // StartAcceptingConnections().
  MultiThreadExecutor accept_loops_runner_{kMaxConcurrentAcceptLoops};

  // A map of service_id -> ServerSocket. If map is non-empty, we
  // are currently listening for incoming connections.
  // AwdlServerSocket instances are used from accept_loops_runner_,
  // and thus require pointer stability.
  absl::flat_hash_map<std::string, AwdlServerSocket> server_sockets_
      ABSL_GUARDED_BY(mutex_);

  // Whether the multiplex feature is enabled.
  bool is_multiplex_enabled_ = NearbyFlags::GetInstance().GetBoolFlag(
      config_package_nearby::nearby_connections_feature::kEnableMultiplex) &&
      NearbyFlags::GetInstance().GetBoolFlag(
          // TODO(govenliu): Add a flag for Awdl?
          config_package_nearby::nearby_connections_feature::
              kEnableMultiplexWifiLan);

  // A map of IpAddress -> MultiplexSocket.
  absl::flat_hash_map<std::string, mediums::multiplex::MultiplexSocket*>
      multiplex_sockets_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_AWDL_H_

