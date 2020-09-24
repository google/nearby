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

#ifndef CORE_V2_INTERNAL_SIMULATION_USER_H_
#define CORE_V2_INTERNAL_SIMULATION_USER_H_

#include <string>

#include "core_v2/internal/bwu_manager.h"
#include "core_v2/internal/client_proxy.h"
#include "core_v2/internal/endpoint_channel_manager.h"
#include "core_v2/internal/endpoint_manager.h"
#include "core_v2/internal/payload_manager.h"
#include "core_v2/internal/pcp_manager.h"
#include "core_v2/options.h"
#include "platform_v2/base/medium_environment.h"
#include "platform_v2/public/condition_variable.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/future.h"
#include "gtest/gtest.h"

// Test-only class to help run end-to-end simulations for nearby connections
// protocol.
//
// This is a "standalone" version of PcpManager. It can run independently,
// provided MediumEnvironment has adequate support for all medium types in use.
namespace location {
namespace nearby {
namespace connections {

class SimulationUser {
 public:
  struct DiscoveredInfo {
    std::string endpoint_id;
    ByteArray endpoint_info;
    std::string service_id;

    bool Empty() const { return endpoint_id.empty(); }
    void Clear() { endpoint_id.clear(); }
  };

  explicit SimulationUser(
      const std::string& device_name,
      BooleanMediumSelector allowed = BooleanMediumSelector())
      : info_{ByteArray{device_name}},
        options_{
            .strategy = Strategy::kP2pCluster,
            .allowed = allowed,
        } {}
  virtual ~SimulationUser() { Stop(); }
  void Stop() {
    pm_.DisconnectFromEndpointManager();
    mgr_.DisconnectFromEndpointManager();
  }

  // Calls PcpManager::StartAdvertising.
  // If latch is provided, will call latch->CountDown() in the initiated_cb
  // callback.
  void StartAdvertising(const std::string& service_id, CountDownLatch* latch);

  // Calls PcpManager::StartDiscovery.
  // If latch is provided, will call latch->CountDown() in the endpoint_found_cb
  // callback.
  void StartDiscovery(const std::string& service_id, CountDownLatch* latch);

  // Calls PcpManager::RequestConnection.
  // If latch is provided, latch->CountDown() will be called in the initiated_cb
  // callback.
  void RequestConnection(CountDownLatch* latch);

  // Calls PcpManager::AcceptConnection.
  // If latch is provided, latch->CountDown() will be called in the accepted_cb
  // callback.
  void AcceptConnection(CountDownLatch* latch);

  // Calls PcpManager::RejectConnection.
  // If latch is provided, latch->CountDown() will be called in the rejected_cb
  // callback.
  void RejectConnection(CountDownLatch* latch);

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

  bool WaitForProgress(std::function<bool(const PayloadProgressInfo&)> pred,
                       absl::Duration timeout);

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
  void OnPayload(const std::string& endpoint_id, Payload payload);
  void OnPayloadProgress(const std::string& endpoint_id,
                         const PayloadProgressInfo& info);

  std::string service_id_;
  DiscoveredInfo discovered_;
  ConnectionOptions connection_options_;
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
  std::function<bool(const PayloadProgressInfo&)> predicate_;
  ByteArray info_;
  Mediums mediums_;
  ConnectionOptions options_;
  ClientProxy client_;
  EndpointChannelManager ecm_;
  EndpointManager em_{&ecm_};
  BwuManager bwu_{mediums_, em_, ecm_, {}, {}};
  PcpManager mgr_{mediums_, ecm_, em_, bwu_};
  PayloadManager pm_{em_};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_SIMULATION_USER_H_
