// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_DART_NEARBY_CONNECTIONS_CLIENT_STATE_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_DART_NEARBY_CONNECTIONS_CLIENT_STATE_H_

#include <deque>
#include <memory>
#include <optional>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "third_party/dart_lang/v2/runtime/include/dart_api.h"
#include "connections/c/nc.h"
#include "connections/dart/nc_adapter_types.h"

namespace nearby::connections::dart {

// This class maintains the client state of Nearby Connections. An applicaiton
// should only maintain one client.
class NearbyConnectionsClientState {
 public:
  enum class NearbyConnectionsApi {
    kStartAdvertising,
    kStopAdvertising,
    kStartDiscovery,
    kStopDiscovery,
    kRequestConnection,
    kAcceptConnection,
    kRejectConnection,
    kDisconnectFromEndpoint,
    kSendPayload,
    kEnableBleV2
  };

  NearbyConnectionsClientState() = default;
  NearbyConnectionsClientState(const NearbyConnectionsClientState&) = delete;
  NearbyConnectionsClientState& operator=(const NearbyConnectionsClientState&) =
      delete;

  NC_INSTANCE GetOpennedService() const ABSL_LOCKS_EXCLUDED(mutex_);
  void SetOpennedService(NC_INSTANCE nc_instance) ABSL_LOCKS_EXCLUDED(mutex_);

  DiscoveryListenerDart* GetDiscoveryListenerDart() ABSL_LOCKS_EXCLUDED(mutex_);
  void SetDiscoveryListenerDart(
      std::unique_ptr<DiscoveryListenerDart> discovery_listener_dart)
      ABSL_LOCKS_EXCLUDED(mutex_);

  ConnectionListenerDart* GetConnectionListenerDart()
      ABSL_LOCKS_EXCLUDED(mutex_);
  void SetConnectionListenerDart(
      std::unique_ptr<ConnectionListenerDart> connection_listener_dart)
      ABSL_LOCKS_EXCLUDED(mutex_);

  PayloadListenerDart* GetPayloadListenerDart() ABSL_LOCKS_EXCLUDED(mutex_);
  void SetPayloadListenerDart(
      std::unique_ptr<PayloadListenerDart> payload_listener_dart)
      ABSL_LOCKS_EXCLUDED(mutex_);

  std::optional<Dart_Port> PopNearbyConnectionsApiPort(NearbyConnectionsApi api)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void PushNearbyConnectionsApiPort(NearbyConnectionsApi api,
                                    Dart_Port dart_port)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void reset() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  mutable absl::Mutex mutex_;

  NC_INSTANCE opened_instance_ ABSL_GUARDED_BY(mutex_) = nullptr;
  absl::flat_hash_map<NearbyConnectionsApi, std::deque<Dart_Port>>
      nearby_connections_api_ports_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<DiscoveryListenerDart> discovery_listener_dart_
      ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<ConnectionListenerDart> connection_listener_dart_
      ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<PayloadListenerDart> payload_listener_dart_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby::connections::dart

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_DART_NEARBY_CONNECTIONS_CLIENT_STATE_H_
