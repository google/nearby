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

#ifndef CORE_INTERNAL_SIMULATION_USER_H_
#define CORE_INTERNAL_SIMULATION_USER_H_

#include <stdbool.h>
#include <cstdint>
#include <string>

#include "gtest/gtest.h"
#include "connections/implementation/bwu_manager.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/injected_bluetooth_device_store.h"
#include "connections/implementation/payload_manager.h"
#include "connections/implementation/pcp_manager.h"
#include "connections/v3/connections_device.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/future.h"

// Test-only class to help run end-to-end simulations for nearby connections
// protocol.
//
// This is a "standalone" version of PcpManager. It can run independently,
// provided MediumEnvironment has adequate support for all medium types in use.

namespace nearby {
namespace connections {

class SetSafeToDisconnect {
 public:
  explicit SetSafeToDisconnect(bool safe_to_disconnect, bool auto_reconnect,
                               bool payload_received_ack,
                               std::int32_t safe_to_disconnect_version) {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::
            kEnableSafeToDisconnect,
        safe_to_disconnect);
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::kEnableAutoReconnect,
        auto_reconnect);
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::
            kEnablePayloadReceivedAck,
        payload_received_ack);
    NearbyFlags::GetInstance().OverrideInt64FlagValue(
        config_package_nearby::nearby_connections_feature::
            kSafeToDisconnectVersion,
        safe_to_disconnect_version);
  }
};

class SimulationUser {
 public:
  struct DiscoveredInfo {
    std::string endpoint_id;
    ByteArray endpoint_info;
    std::string service_id;

    bool Empty() const { return endpoint_id.empty(); }
    void Clear() { endpoint_id.clear(); }
  };

  SimulationUser(const std::string& device_name,
                 BooleanMediumSelector allowed = BooleanMediumSelector(),
                 SetSafeToDisconnect set_safe_to_disconnect =
                     SetSafeToDisconnect(true, false, true, 5))
      : info_{ByteArray{device_name}},
        advertising_options_{
            {
                Strategy::kP2pCluster,
                allowed,
            },
        },
        connection_options_{
            .keep_alive_interval_millis = FeatureFlags::GetInstance()
                                              .GetFlags()
                                              .keep_alive_interval_millis,
            .keep_alive_timeout_millis = FeatureFlags::GetInstance()
                                             .GetFlags()
                                             .keep_alive_timeout_millis,
        },
        discovery_options_{
            {
                Strategy::kP2pCluster,
                allowed,
            },
        },
        set_safe_to_disconnect_(set_safe_to_disconnect) {}
  virtual ~SimulationUser() { Stop(); }
  void Stop() {
    pm_.DisconnectFromEndpointManager();
    mgr_.DisconnectFromEndpointManager();
    bwu_.Shutdown();
  }

  // Calls PcpManager::StartAdvertising.
  // If latch is provided, will call latch->CountDown() in the initiated_cb
  // callback.
  void StartAdvertising(const std::string& service_id, CountDownLatch* latch);

  // Calls PcpManager::StartDiscovery.
  // If latch is provided, will call latch->CountDown() in the endpoint_found_cb
  // callback.
  void StartDiscovery(const std::string& service_id, CountDownLatch* latch);

  void StopDiscovery();

  // Calls PcpManager::UpdateDiscoveryOptions.
  Status UpdateDiscoveryOptions(absl::string_view service_id);

  // Calls PcpManager::InjectEndpoint.
  void InjectEndpoint(const std::string& service_id,
                      const OutOfBandConnectionMetadata& metadata);

  // Calls PcpManager::RequestConnection.
  // If latch is provided, latch->CountDown() will be called in the initiated_cb
  // callback.
  void RequestConnection(CountDownLatch* latch);

  // Calls PcpManager::RequestConnectionV3.
  // If latch is provided, latch->CountDown() will be called in the initiated_cb
  // callback.
  void RequestConnectionV3(CountDownLatch* latch,
                           const NearbyDevice& remote_device);

  // Calls PcpManager::AcceptConnection.
  // If latch is provided, latch->CountDown() will be called in the accepted_cb
  // callback.
  void AcceptConnection(CountDownLatch* latch);

  // Calls PcpManager::RejectConnection.
  // If latch is provided, latch->CountDown() will be called in the rejected_cb
  // callback.
  void RejectConnection(CountDownLatch* latch);

  // Calls PcpManager::StartListeningForIncomingConnections.
  // If latch is provided, latch->CountDown() will be called on call completion.
  void StartListeningForIncomingConnections(
      CountDownLatch* latch, absl::string_view service_id,
      const v3::ConnectionListeningOptions& options, Status expected_status);

  // Unlike acceptance, rejection does not have to be mutual, in order to work.
  // This method will allow to synchronize on the remote rejection, without
  // performing a local rejection.
  // latch.CountDown() will be called in the rejected_cb callback.
  void ExpectRejectedConnection(CountDownLatch& latch) {
    reject_latch_ = &latch;
  }

  void ExpectPayload(CountDownLatch& latch) { payload_latch_ = &latch; }

  const DiscoveredInfo& GetDiscovered() const { return discovered_; }
  ByteArray GetInfo() const { return info_; }

  bool WaitForProgress(
      absl::AnyInvocable<bool(const PayloadProgressInfo&)> pred,
      absl::Duration timeout);

  ClientProxy& GetClient() { return client_; }
  EndpointChannelManager& GetEndpointChannelManager() { return ecm_; }

 protected:
  // ConnectionListener callbacks
  void OnConnectionInitiated(const std::string& endpoint_id,
                             const ConnectionResponseInfo& info,
                             bool is_outgoing);
  void OnConnectionAccepted(const std::string& endpoint_id);
  void OnConnectionRejected(const std::string& endpoint_id, Status status);

  // DiscoveryListener callbacks
  void OnEndpointFound(const std::string& endpoint_id,
                       const ByteArray& endpoint_info,
                       const std::string& service_id);
  void OnEndpointLost(const std::string& endpoint_id);

  // PayloadListener callbacks
  void OnPayload(absl::string_view, Payload payload);
  void OnPayloadProgress(absl::string_view endpoint_id,
                         const PayloadProgressInfo& info);

  std::string service_id_;
  DiscoveredInfo discovered_;
  Mutex progress_mutex_;
  ConditionVariable progress_sync_{&progress_mutex_};
  PayloadProgressInfo progress_info_;
  Payload payload_;
  CountDownLatch* initiated_latch_ = nullptr;
  CountDownLatch* accept_latch_ = nullptr;
  CountDownLatch* reject_latch_ = nullptr;
  CountDownLatch* found_latch_ = nullptr;
  CountDownLatch* lost_latch_ = nullptr;
  CountDownLatch* payload_latch_ = nullptr;
  Future<bool>* future_ = nullptr;
  absl::AnyInvocable<bool(const PayloadProgressInfo&)> predicate_;
  ByteArray info_;
  Mediums mediums_;
  AdvertisingOptions advertising_options_;
  ConnectionOptions connection_options_;
  DiscoveryOptions discovery_options_;
  SetSafeToDisconnect set_safe_to_disconnect_;
  ClientProxy client_;
  EndpointChannelManager ecm_;
  EndpointManager em_{&ecm_};
  BwuManager bwu_{mediums_, em_, ecm_, {}, {}};
  InjectedBluetoothDeviceStore injected_bluetooth_device_store_;
  PcpManager mgr_{mediums_, ecm_, em_, bwu_, injected_bluetooth_device_store_};
  PayloadManager pm_{em_};
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_SIMULATION_USER_H_
