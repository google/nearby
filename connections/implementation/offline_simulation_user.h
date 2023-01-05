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

#ifndef CORE_INTERNAL_OFFLINE_SIMULATION_USER_H_
#define CORE_INTERNAL_OFFLINE_SIMULATION_USER_H_

#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/offline_service_controller.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/future.h"

// Test-only class to help run end-to-end simulations for nearby connections
// protocol.
//
// This is a "standalone" version of PcpManager. It can run independently,
// provided MediumEnvironment has adequate support for all medium types in use.

namespace nearby {
namespace connections {

class OfflineSimulationUser {
 public:
  struct DiscoveredInfo {
    std::string endpoint_id;
    ByteArray endpoint_info;
    std::string service_id;

    bool Empty() const { return endpoint_id.empty(); }
    void Clear() { endpoint_id.clear(); }
  };

  explicit OfflineSimulationUser(
      absl::string_view device_name,
      BooleanMediumSelector allowed = BooleanMediumSelector())
      : info_{ByteArray{std::string(device_name)}},
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
        discovery_options_{{
            Strategy::kP2pCluster,
            allowed,
        }} {}
  virtual ~OfflineSimulationUser() = default;

  // Calls PcpManager::StartAdvertising().
  // If latch is provided, will call latch->CountDown() in the initiated_cb
  // callback.
  Status StartAdvertising(const std::string& service_id, CountDownLatch* latch);

  // Calls PcpManager::StopAdvertising().
  void StopAdvertising();

  // Calls PcpManager::StartDiscovery().
  // If found_latch is provided, will call found_latch->CountDown() in the
  // endpoint_found_cb callback.
  // If lost_latch is provided, will call lost_latch->CountDown() in the
  // endpoint_lost_cb callback.
  Status StartDiscovery(const std::string& service_id,
                        CountDownLatch* found_latch,
                        CountDownLatch* lost_latch = nullptr);

  // Calls PcpManager::StopDiscovery().
  void StopDiscovery();

  // Calls PcpManager::InjectEndpoint();
  void InjectEndpoint(const std::string& service_id,
                      const OutOfBandConnectionMetadata& metadata);

  // Calls PcpManager::RequestConnection().
  // If latch is provided, latch->CountDown() will be called in the initiated_cb
  // callback.
  Status RequestConnection(CountDownLatch* latch);

  // Calls PcpManager::AcceptConnection.
  // If latch is provided, latch->CountDown() will be called in the accepted_cb
  // callback.
  Status AcceptConnection(CountDownLatch* latch);

  // Calls PcpManager::RejectConnection.
  // If latch is provided, latch->CountDown() will be called in the rejected_cb
  // callback.
  Status RejectConnection(CountDownLatch* latch);

  // Unlike acceptance, rejection does not have to be mutual, in order to work.
  // This method will allow to synchronize on the remote rejection, without
  // performing a local rejection.
  // latch.CountDown() will be called in the rejected_cb callback.
  void ExpectRejectedConnection(CountDownLatch& latch) {
    reject_latch_ = &latch;
  }

  void ExpectPayload(CountDownLatch& latch) { payload_latch_ = &latch; }
  void ExpectDisconnect(CountDownLatch& latch) { disconnect_latch_ = &latch; }

  const DiscoveredInfo& GetDiscovered() const { return discovered_; }
  ByteArray GetInfo() const { return info_; }

  bool WaitForProgress(std::function<bool(const PayloadProgressInfo&)> pred,
                       absl::Duration timeout);

  Payload& GetPayload() { return payload_; }
  void SendPayload(Payload payload) {
    sender_payload_id_ = payload.GetId();
    ctrl_.SendPayload(&client_, {discovered_.endpoint_id}, std::move(payload));
  }

  Status CancelPayload() {
    if (sender_payload_id_) {
      return ctrl_.CancelPayload(&client_, sender_payload_id_);
    } else {
      return ctrl_.CancelPayload(&client_, payload_.GetId());
    }
  }

  void Disconnect();

  bool IsAdvertising() const { return client_.IsAdvertising(); }

  bool IsDiscovering() const { return client_.IsDiscovering(); }

  bool IsConnected() const {
    return client_.IsConnectedToEndpoint(discovered_.endpoint_id);
  }

  void Stop() {
    StopAdvertising();
    StopDiscovery();
    ctrl_.Stop();
  }

 protected:
  // ConnectionListener callbacks
  void OnConnectionInitiated(const std::string& endpoint_id,
                             const ConnectionResponseInfo& info,
                             bool is_outgoing);
  void OnConnectionAccepted(const std::string& endpoint_id);
  void OnConnectionRejected(const std::string& endpoint_id, Status status);
  void OnEndpointDisconnect(const std::string& endpoint_id);

  // DiscoveryListener callbacks
  void OnEndpointFound(const std::string& endpoint_id,
                       const ByteArray& endpoint_info,
                       const std::string& service_id);
  void OnEndpointLost(const std::string& endpoint_id);

  // PayloadListener callbacks
  void OnPayload(const std::string& endpoint_id, Payload payload);
  void OnPayloadProgress(const std::string& endpoint_id,
                         const PayloadProgressInfo& info);

  std::string service_id_;
  DiscoveredInfo discovered_;
  ByteArray info_;
  AdvertisingOptions advertising_options_;
  ConnectionOptions connection_options_;
  DiscoveryOptions discovery_options_;
  Mutex progress_mutex_;
  ConditionVariable progress_sync_{&progress_mutex_};
  PayloadProgressInfo progress_info_;
  Payload payload_;
  Payload::Id sender_payload_id_ = 0;
  CountDownLatch* initiated_latch_ = nullptr;
  CountDownLatch* accept_latch_ = nullptr;
  CountDownLatch* reject_latch_ = nullptr;
  CountDownLatch* found_latch_ = nullptr;
  CountDownLatch* lost_latch_ = nullptr;
  CountDownLatch* payload_latch_ = nullptr;
  CountDownLatch* disconnect_latch_ = nullptr;
  Future<bool>* future_ = nullptr;
  std::function<bool(const PayloadProgressInfo&)> predicate_;
  ClientProxy client_;
  OfflineServiceController ctrl_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_OFFLINE_SIMULATION_USER_H_
