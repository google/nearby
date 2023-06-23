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

#include "connections/core.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "connections/implementation/service_id_constants.h"
#include "connections/listeners.h"
#include "connections/v3/bandwidth_info.h"
#include "connections/v3/connection_result.h"
#include "connections/v3/connections_device.h"
#include "connections/v3/listeners.h"
#include "connections/v3/params.h"
#include "internal/interop/device.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {

namespace {

// Timeout for ServiceControllerRouter to run StopAllEndpoints.
constexpr absl::Duration kWaitForDisconnect = absl::Milliseconds(10000);

// Verify that |service_id| is not empty and will not conflict with any internal
// service ID formats.
void CheckServiceId(absl::string_view service_id) {
  CHECK(!service_id.empty());
  CHECK_NE(service_id, kUnknownServiceId);
  CHECK(!IsInitiatorUpgradeServiceId(service_id));
}

}  // namespace

Core::Core(ServiceControllerRouter* router) : router_(router) {}

Core::~Core() {
  CountDownLatch latch(1);
  router_->StopAllEndpoints(
      &client_, {
                    .result_cb = [&latch](Status) { latch.CountDown(); },
                });
  if (!latch.Await(kWaitForDisconnect).result()) {
    NEARBY_LOG(FATAL, "Unable to shutdown");
  }
}

Core::Core(Core&&) = default;

Core& Core::operator=(Core&&) = default;

void Core::StartAdvertising(absl::string_view service_id,
                            AdvertisingOptions advertising_options,
                            ConnectionRequestInfo info,
                            ResultCallback callback) {
  CheckServiceId(service_id);
  CHECK(advertising_options.strategy.IsValid());

  router_->StartAdvertising(&client_, service_id, advertising_options, info,
                            callback);
}

void Core::StopAdvertising(const ResultCallback callback) {
  router_->StopAdvertising(&client_, callback);
}

void Core::StartDiscovery(absl::string_view service_id,
                          DiscoveryOptions discovery_options,
                          DiscoveryListener listener, ResultCallback callback) {
  CheckServiceId(service_id);
  CHECK(discovery_options.strategy.IsValid());

  router_->StartDiscovery(&client_, service_id, discovery_options, listener,
                          callback);
}

void Core::InjectEndpoint(absl::string_view service_id,
                          OutOfBandConnectionMetadata metadata,
                          ResultCallback callback) {
  CheckServiceId(service_id);
  router_->InjectEndpoint(&client_, service_id, metadata, callback);
}

void Core::StopDiscovery(ResultCallback callback) {
  router_->StopDiscovery(&client_, callback);
}

void Core::RequestConnection(absl::string_view endpoint_id,
                             ConnectionRequestInfo info,
                             ConnectionOptions connection_options,
                             ResultCallback callback) {
  CHECK(!endpoint_id.empty());

  // Assign the default from feature flags for the keep-alive frame interval and
  // timeout values if client don't mind them or has the unexpected ones.
  if (connection_options.keep_alive_interval_millis == 0 ||
      connection_options.keep_alive_timeout_millis == 0 ||
      connection_options.keep_alive_interval_millis >=
          connection_options.keep_alive_timeout_millis) {
    NEARBY_LOG(
        WARNING,
        "Client request connection with keep-alive frame as interval=%d, "
        "timeout=%d, which is un-expected. Change to default.",
        connection_options.keep_alive_interval_millis,
        connection_options.keep_alive_timeout_millis);
    connection_options.keep_alive_interval_millis =
        FeatureFlags::GetInstance().GetFlags().keep_alive_interval_millis;
    connection_options.keep_alive_timeout_millis =
        FeatureFlags::GetInstance().GetFlags().keep_alive_timeout_millis;
  }

  router_->RequestConnection(&client_, endpoint_id, info, connection_options,
                             callback);
}

void Core::AcceptConnection(absl::string_view endpoint_id,
                            PayloadListener listener, ResultCallback callback) {
  CHECK(!endpoint_id.empty());

  router_->AcceptConnection(&client_, endpoint_id, std::move(listener),
                            callback);
}

void Core::RejectConnection(absl::string_view endpoint_id,
                            ResultCallback callback) {
  CHECK(!endpoint_id.empty());

  router_->RejectConnection(&client_, endpoint_id, callback);
}

void Core::InitiateBandwidthUpgrade(absl::string_view endpoint_id,
                                    ResultCallback callback) {
  router_->InitiateBandwidthUpgrade(&client_, endpoint_id, callback);
}

void Core::SendPayload(absl::Span<const std::string> endpoint_ids,
                       Payload payload, ResultCallback callback) {
  CHECK(payload.GetType() != PayloadType::kUnknown);
  CHECK(!endpoint_ids.empty());

  router_->SendPayload(&client_, endpoint_ids, std::move(payload), callback);
}

void Core::CancelPayload(std::int64_t payload_id, ResultCallback callback) {
  CHECK_NE(payload_id, 0);

  router_->CancelPayload(&client_, payload_id, callback);
}

void Core::DisconnectFromEndpoint(absl::string_view endpoint_id,
                                  ResultCallback callback) {
  CHECK(!endpoint_id.empty());

  router_->DisconnectFromEndpoint(&client_, endpoint_id, callback);
}

void Core::StopAllEndpoints(ResultCallback callback) {
  router_->StopAllEndpoints(&client_, callback);
}

void Core::SetCustomSavePath(absl::string_view path, ResultCallback callback) {
  router_->SetCustomSavePath(&client_, path, callback);
}

std::string Core::Dump() { return client_.Dump(); }

// V3
void Core::StartAdvertisingV3(absl::string_view service_id,
                              const AdvertisingOptions& advertising_options,
                              const NearbyDevice& local_device,
                              v3::ConnectionListener listener,
                              ResultCallback callback) {
  ConnectionListener old_listener = {
      .initiated_cb =
          [&listener](const std::string& endpoint_id,
                      const ConnectionResponseInfo& info) {
            auto remote_device = v3::ConnectionsDevice(
                endpoint_id, info.remote_endpoint_info.AsStringView(), {});
            listener.initiated_cb(
                remote_device,
                v3::InitialConnectionInfo{
                    .authentication_digits = info.authentication_token,
                    .raw_authentication_token =
                        info.raw_authentication_token.string_data(),
                    .is_incoming_connection = info.is_incoming_connection,
                });
          },
      .accepted_cb =
          [v3_cb = listener.result_cb](const std::string& endpoint_id) {
            auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
            v3_cb(remote_device,
                  v3::ConnectionResult{.status = Status{
                                           .value = Status::kSuccess,
                                       }});
          },
      .rejected_cb =
          [v3_cb = listener.result_cb](const std::string& endpoint_id,
                                       Status status) {
            auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
            v3_cb(remote_device, v3::ConnectionResult{
                                     .status = status,
                                 });
          },
      .disconnected_cb =
          [&listener](const std::string& endpoint_id) {
            auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
            listener.disconnected_cb(remote_device);
          },
      .bandwidth_changed_cb =
          [&listener](const std::string& endpoint_id, Medium medium) {
            auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
            listener.bandwidth_changed_cb(remote_device,
                                          v3::BandwidthInfo{.medium = medium});
          }};
  ByteArray local_endpoint_info;
  if (local_device.GetType() == NearbyDevice::kConnectionsDevice) {
    local_endpoint_info =
        ByteArray(reinterpret_cast<const v3::ConnectionsDevice&>(local_device)
                      .GetEndpointInfo());
  }
  ConnectionRequestInfo old_info = {
      .endpoint_info = local_endpoint_info,
      .listener = old_listener,
  };
  StartAdvertising(service_id, advertising_options, old_info, callback);
}

void Core::StartAdvertisingV3(absl::string_view service_id,
                              const AdvertisingOptions& advertising_options,
                              v3::ConnectionListener listener,
                              ResultCallback callback) {
  ConnectionListener old_listener = {
      .initiated_cb =
          [&listener](const std::string& endpoint_id,
                      const ConnectionResponseInfo& info) {
            auto remote_device = v3::ConnectionsDevice(
                endpoint_id, info.remote_endpoint_info.AsStringView(), {});
            listener.initiated_cb(
                remote_device,
                v3::InitialConnectionInfo{
                    .authentication_digits = info.authentication_token,
                    .raw_authentication_token =
                        info.raw_authentication_token.string_data(),
                    .is_incoming_connection = info.is_incoming_connection,
                });
          },
      .accepted_cb =
          [v3_cb = listener.result_cb](const std::string& endpoint_id) {
            auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
            v3_cb(remote_device,
                  v3::ConnectionResult{.status = Status{
                                           .value = Status::kSuccess,
                                       }});
          },
      .rejected_cb =
          [v3_cb = listener.result_cb](const std::string& endpoint_id,
                                       Status status) {
            auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
            v3_cb(remote_device, v3::ConnectionResult{
                                     .status = status,
                                 });
          },
      .disconnected_cb =
          [&listener](const std::string& endpoint_id) {
            auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
            listener.disconnected_cb(remote_device);
          },
      .bandwidth_changed_cb =
          [&listener](const std::string& endpoint_id, Medium medium) {
            auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
            listener.bandwidth_changed_cb(remote_device,
                                          v3::BandwidthInfo{.medium = medium});
          }};
  ByteArray local_endpoint_info;
  const NearbyDevice* local_device = client_.GetLocalDevice();
  if (local_device->GetType() == NearbyDevice::kConnectionsDevice) {
    local_endpoint_info =
        ByteArray(reinterpret_cast<const v3::ConnectionsDevice*>(local_device)
                      ->GetEndpointInfo());
  }
  ConnectionRequestInfo old_info = {
      .endpoint_info = local_endpoint_info,
      .listener = old_listener,
  };
  StartAdvertising(service_id, advertising_options, old_info, callback);
}

void Core::StopAdvertisingV3(ResultCallback result_cb) {
  StopAdvertising(result_cb);
}

void Core::StartDiscoveryV3(absl::string_view service_id,
                            const DiscoveryOptions& discovery_options,
                            v3::DiscoveryListener listener,
                            ResultCallback callback) {
  DiscoveryListener old_listener = {
      .endpoint_found_cb =
          [&listener](const std::string& endpoint_id,
                      const ByteArray& endpoint_info,
                      const std::string& service_id) {
            auto remote_device = v3::ConnectionsDevice(
                endpoint_id, endpoint_info.AsStringView(), {});
            listener.endpoint_found_cb(remote_device, service_id);
          },
      .endpoint_lost_cb =
          [&listener](const std::string& endpoint_id) {
            auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
            listener.endpoint_lost_cb(remote_device);
          },
      .endpoint_distance_changed_cb =
          [&listener](const std::string& endpoint_id,
                      DistanceInfo distance_info) {
            auto remote = v3::ConnectionsDevice(endpoint_id, "", {});
            listener.endpoint_distance_changed_cb(remote, distance_info);
          },
  };
  StartDiscovery(service_id, discovery_options, old_listener, callback);
}

void Core::StopDiscoveryV3(ResultCallback result_cb) {
  router_->StopDiscovery(&client_, result_cb);
}

void Core::StartListeningForIncomingConnectionsV3(
    const v3::ConnectionListeningOptions& options, absl::string_view service_id,
    v3::ConnectionListener listener_cb, v3::ListeningResultListener result_cb) {
  router_->StartListeningForIncomingConnectionsV3(
      &client_, service_id, std::move(listener_cb), options,
      std::move(result_cb));
}

void Core::StopListeningForIncomingConnectionsV3() {
  router_->StopListeningForIncomingConnectionsV3(&client_);
}

void Core::RequestConnectionV3(const NearbyDevice& local_device,
                               const NearbyDevice& remote_device,
                               ConnectionOptions connection_options,
                               v3::ConnectionListener connection_cb,
                               ResultCallback result_cb) {
  CHECK(!remote_device.GetEndpointId().empty());

  v3::ConnectionRequestInfo info = {
      .local_device = const_cast<NearbyDevice&>(local_device),
      .listener = std::move(connection_cb),
  };

  // Assign the default from feature flags for the keep-alive frame interval and
  // timeout values if client don't mind them or has the unexpected ones.
  if (connection_options.keep_alive_interval_millis == 0 ||
      connection_options.keep_alive_timeout_millis == 0 ||
      connection_options.keep_alive_interval_millis >=
          connection_options.keep_alive_timeout_millis) {
    NEARBY_LOG(
        WARNING,
        "Client request connection with keep-alive frame as interval=%d, "
        "timeout=%d, which is un-expected. Change to default.",
        connection_options.keep_alive_interval_millis,
        connection_options.keep_alive_timeout_millis);
    connection_options.keep_alive_interval_millis =
        FeatureFlags::GetInstance().GetFlags().keep_alive_interval_millis;
    connection_options.keep_alive_timeout_millis =
        FeatureFlags::GetInstance().GetFlags().keep_alive_timeout_millis;
  }
  router_->RequestConnectionV3(&client_, remote_device, std::move(info),
                               connection_options, result_cb);
}

void Core::RequestConnectionV3(const NearbyDevice& remote_device,
                               ConnectionOptions connection_options,
                               v3::ConnectionListener connection_cb,
                               ResultCallback result_cb) {
  v3::ConnectionRequestInfo info = {
      .local_device = const_cast<NearbyDevice&>(*(client_.GetLocalDevice())),
      .listener = std::move(connection_cb),
  };
  CHECK(!remote_device.GetEndpointId().empty());

  // Assign the default from feature flags for the keep-alive frame interval and
  // timeout values if client don't mind them or has the unexpected ones.
  if (connection_options.keep_alive_interval_millis == 0 ||
      connection_options.keep_alive_timeout_millis == 0 ||
      connection_options.keep_alive_interval_millis >=
          connection_options.keep_alive_timeout_millis) {
    NEARBY_LOG(
        WARNING,
        "Client request connection with keep-alive frame as interval=%d, "
        "timeout=%d, which is un-expected. Change to default.",
        connection_options.keep_alive_interval_millis,
        connection_options.keep_alive_timeout_millis);
    connection_options.keep_alive_interval_millis =
        FeatureFlags::GetInstance().GetFlags().keep_alive_interval_millis;
    connection_options.keep_alive_timeout_millis =
        FeatureFlags::GetInstance().GetFlags().keep_alive_timeout_millis;
  }
  router_->RequestConnectionV3(&client_, remote_device, std::move(info),
                               connection_options, result_cb);
}

void Core::AcceptConnectionV3(const NearbyDevice& remote_device,
                              v3::PayloadListener listener_cb,
                              ResultCallback result_cb) {
  CHECK(!remote_device.GetEndpointId().empty());

  router_->AcceptConnectionV3(&client_, remote_device, std::move(listener_cb),
                              result_cb);
}

void Core::RejectConnectionV3(const NearbyDevice& remote_device,
                              ResultCallback result_cb) {
  CHECK(!remote_device.GetEndpointId().empty());

  router_->RejectConnectionV3(&client_, remote_device, result_cb);
}

void Core::SendPayloadV3(const NearbyDevice& remote_device, Payload payload,
                         ResultCallback result_cb) {
  CHECK(payload.GetType() != PayloadType::kUnknown);
  CHECK(!remote_device.GetEndpointId().empty());

  router_->SendPayloadV3(&client_, remote_device, std::move(payload),
                         result_cb);
}

void Core::CancelPayloadV3(const NearbyDevice& remote_device,
                           int64_t payload_id, ResultCallback result_cb) {
  CHECK_NE(payload_id, 0);

  router_->CancelPayloadV3(&client_, remote_device, payload_id, result_cb);
}

void Core::DisconnectFromDeviceV3(const NearbyDevice& remote_device,
                                  ResultCallback result_cb) {
  CHECK(!remote_device.GetEndpointId().empty());

  router_->DisconnectFromDeviceV3(&client_, remote_device, result_cb);
}

void Core::StopAllDevicesV3(ResultCallback result_cb) {
  router_->StopAllEndpoints(&client_, result_cb);
}

void Core::InitiateBandwidthUpgradeV3(const NearbyDevice& remote_device,
                                      ResultCallback result_cb) {
  router_->InitiateBandwidthUpgradeV3(&client_, remote_device, result_cb);
}

void Core::UpdateAdvertisingOptionsV3(absl::string_view service_id,
                                      AdvertisingOptions advertising_options,
                                      ResultCallback result_cb) {
  router_->UpdateAdvertisingOptionsV3(&client_, service_id, advertising_options,
                                      result_cb);
}

void Core::UpdateDiscoveryOptionsV3(absl::string_view service_id,
                                    DiscoveryOptions discovery_options,
                                    ResultCallback result_cb) {
  router_->UpdateDiscoveryOptionsV3(&client_, service_id, discovery_options,
                                    result_cb);
}

}  // namespace connections
}  // namespace nearby
