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

#include <vector>

#include "core/core.h"
#include "core/listeners.h"
#include "core/params.h"
#include "core/payload.h"
#include "core/status.h"
#include "platform/api/platform.h"
#include "platform/byte_array.h"
#include "platform/impl/shared/file_impl.h"
#include "platform/impl/shared/sample/sample_wifi_medium.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

using TestPlatform = platform::ImplementationPlatform;

class ResultListenerImpl : public ResultListener {
 public:
  void onResult(Status::Value status) override {}
};

class ConnectionLifecycleListenerImpl : public ConnectionLifecycleListener {
 public:
  void onConnectionInitiated(ConstPtr<OnConnectionInitiatedParams>
                                 on_connection_initiated_params) override {}
  void onConnectionResult(
      ConstPtr<OnConnectionResultParams> on_connection_result_params) override {
  }
  void onDisconnected(
      ConstPtr<OnDisconnectedParams> on_disconnected_params) override {}
  void onBandwidthChanged(
      ConstPtr<OnBandwidthChangedParams> on_bandwidth_changed_params) override {
  }
};

class DiscoveryListenerImpl : public DiscoveryListener {
 public:
  void onEndpointFound(
      ConstPtr<OnEndpointFoundParams> on_endpoint_found_params) override {}
  void onEndpointLost(
      ConstPtr<OnEndpointLostParams> on_endpoint_lost_params) override {}
};

class PayloadListenerImpl : public PayloadListener {
 public:
  void onPayloadReceived(
      ConstPtr<OnPayloadReceivedParams> on_payload_received_params) override {}
  void onPayloadTransferUpdate(ConstPtr<OnPayloadTransferUpdateParams>
                                   on_payload_transfer_update_params) override {
  }
};

void check_compilation() {
  Core<TestPlatform> core;

  const string name = "name";
  const string service_id = "service_id";
  const string remote_endpoint_id = "remote_endpoint_id";

  core.startAdvertising(MakeConstPtr(new StartAdvertisingParams(
      MakePtr(new ResultListenerImpl()), name, service_id,
      AdvertisingOptions(Strategy::kP2PCluster,
                         /* auto_upgrade_bandwidth= */ false,
                         /* enforce_topology_constraints= */ false),
      MakePtr(new ConnectionLifecycleListenerImpl()))));

  core.stopAdvertising(MakeConstPtr(new StopAdvertisingParams()));

  core.startDiscovery(MakeConstPtr(
      new StartDiscoveryParams(MakePtr(new ResultListenerImpl()), service_id,
                               DiscoveryOptions(Strategy::kP2PCluster),
                               MakePtr(new DiscoveryListenerImpl()))));

  core.stopDiscovery(MakeConstPtr(new StopDiscoveryParams()));

  core.requestConnection(MakeConstPtr(new RequestConnectionParams(
      MakePtr(new ResultListenerImpl()), name, remote_endpoint_id,
      MakePtr(new ConnectionLifecycleListenerImpl()))));

  core.acceptConnection(MakeConstPtr(new AcceptConnectionParams(
      MakePtr(new ResultListenerImpl()), remote_endpoint_id,
      MakePtr(new PayloadListenerImpl()))));

  core.rejectConnection(MakeConstPtr(new RejectConnectionParams(
      MakePtr(new ResultListenerImpl()), remote_endpoint_id)));

  core.initiateBandwidthUpgrade(MakeConstPtr(new InitiateBandwidthUpgradeParams(
      MakePtr(new ResultListenerImpl()), remote_endpoint_id)));

  core.sendPayload(MakeConstPtr(new SendPayloadParams(
      MakePtr(new ResultListenerImpl()),
      std::vector<string>(1, remote_endpoint_id),
      ConstifyPtr(
          Payload::fromBytes(MakeConstPtr(new ByteArray("bytes", 5)))))));

  core.cancelPayload(MakeConstPtr(
      new CancelPayloadParams(MakePtr(new ResultListenerImpl()), 1)));

  core.sendPayload(MakeConstPtr(new SendPayloadParams(
      MakePtr(new ResultListenerImpl()),
      std::vector<string>(2, remote_endpoint_id),
      ConstifyPtr(Payload::fromFile(MakePtr(
          new InputFileImpl("/some/arbitrary/file/path.txt", 1024)))))));

  core.cancelPayload(MakeConstPtr(
      new CancelPayloadParams(MakePtr(new ResultListenerImpl()), 2)));

  core.disconnectFromEndpoint(
      MakeConstPtr(new DisconnectFromEndpointParams(remote_endpoint_id)));

  core.stopAllEndpoints(MakeConstPtr(
      new StopAllEndpointsParams(MakePtr(new ResultListenerImpl()))));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location

int main() {
  location::nearby::connections::check_compilation();
  return 0;
}
