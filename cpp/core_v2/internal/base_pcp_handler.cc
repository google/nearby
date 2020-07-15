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

#include "core_v2/internal/base_pcp_handler.h"

#include <cassert>
#include <cinttypes>
#include <cstdlib>
#include <limits>
#include <memory>

#include "core_v2/internal/offline_frames.h"
#include "core_v2/internal/pcp_handler.h"
#include "platform_v2/public/logging.h"
#include "platform_v2/public/system_clock.h"
#include "securegcm/d2d_connection_context_v1.h"
#include "securegcm/ukey2_handshake.h"
#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"

namespace location {
namespace nearby {
namespace connections {

using ::location::nearby::proto::connections::Medium;
using ::securegcm::UKey2Handshake;

constexpr absl::Duration BasePcpHandler::kConnectionRequestReadTimeout;
constexpr absl::Duration BasePcpHandler::kRejectedConnectionCloseDelay;

BasePcpHandler::BasePcpHandler(EndpointManager* endpoint_manager,
                               EndpointChannelManager* channel_manager, Pcp pcp)
    : endpoint_manager_(endpoint_manager),
      channel_manager_(channel_manager),
      pcp_(pcp) {}

BasePcpHandler::~BasePcpHandler() {
  NEARBY_LOGS(INFO) << "BasePcpHandler: going down; strategy="
                    << strategy_.GetName() << "; handle=" << handle_;
  DisconnectFromEndpointManager();
  // Stop all the ongoing Runnables (as gracefully as possible).
  NEARBY_LOGS(INFO) << "BasePcpHandler: bringing down executors; strategy="
                    << strategy_.GetName();
  serial_executor_.Shutdown();
  alarm_executor_.Shutdown();
  NEARBY_LOGS(INFO) << "BasePcpHandler: is down; strategy="
                    << strategy_.GetName();
}

void BasePcpHandler::DisconnectFromEndpointManager() {
  if (stop_.Set(true)) return;
  NEARBY_LOGS(INFO) << "BasePcpHandler: Unregister from EPM; strategy="
                    << strategy_.GetName() << "; handle=" << handle_;
  // Unregister ourselves from EPM message dispatcher.
  endpoint_manager_->UnregisterFrameProcessor(V1Frame::CONNECTION_RESPONSE,
                                              handle_, true);
}

Status BasePcpHandler::StartAdvertising(ClientProxy* client,
                                        const std::string& service_id,
                                        const ConnectionOptions& options,
                                        const ConnectionRequestInfo& info) {
  Future<Status> response;
  RunOnPcpHandlerThread(
      [this, client, &service_id, &info, &options, &response]() {
        auto result = StartAdvertisingImpl(client, service_id,
                                           client->GenerateLocalEndpointId(),
                                           info.name, options);
        if (!result.status.Ok()) {
          response.Set(result.status);
          return;
        }

        // Now that we've succeeded, mark the client as advertising.
        advertising_options_ = options;
        advertising_listener_ = info.listener;
        client->StartedAdvertising(service_id, GetStrategy(), info.listener,
                                   absl::MakeSpan(result.mediums));
        response.Set({Status::kSuccess});
      });
  return WaitForResult(absl::StrCat("StartAdvertising(", info.name, ")"),
                       client->GetClientId(), &response);
}

void BasePcpHandler::StopAdvertising(ClientProxy* client) {
  CountDownLatch latch(1);
  RunOnPcpHandlerThread([this, client, &latch]() {
    StopAdvertisingImpl(client);
    client->StoppedAdvertising();
    advertising_options_.Clear();
    latch.CountDown();
  });
  WaitForLatch("StopAdvertising", &latch);
}

Status BasePcpHandler::StartDiscovery(ClientProxy* client,
                                      const std::string& service_id,
                                      const ConnectionOptions& options,
                                      const DiscoveryListener& listener) {
  Future<Status> response;
  RunOnPcpHandlerThread(
      [this, client, service_id, options, &listener, &response]() {
        // Ask the implementation to attempt to start discovery.
        auto result = StartDiscoveryImpl(client, service_id, options);
        if (!result.status.Ok()) {
          response.Set(result.status);
          return;
        }

        // Now that we've succeeded, mark the client as discovering and clear
        // out any old endpoints we had discovered.
        discovery_options_ = options;
        discovered_endpoints_.clear();
        client->StartedDiscovery(service_id, GetStrategy(), listener,
                                 absl::MakeSpan(result.mediums));
        response.Set({Status::kSuccess});
      });
  return WaitForResult(absl::StrCat("StartDiscovery(", service_id, ")"),
                       client->GetClientId(), &response);
}

void BasePcpHandler::StopDiscovery(ClientProxy* client) {
  CountDownLatch latch(1);
  RunOnPcpHandlerThread([this, client, &latch]() {
    StopDiscoveryImpl(client);
    client->StoppedDiscovery();
    discovery_options_.Clear();
    latch.CountDown();
  });

  WaitForLatch("stopDiscovery", &latch);
}

void BasePcpHandler::WaitForLatch(const std::string& method_name,
                                  CountDownLatch* latch) {
  Exception await_exception = latch->Await();
  if (!await_exception.Ok()) {
    if (await_exception.Raised(Exception::kTimeout)) {
      NEARBY_LOG(INFO, "Blocked in %s", method_name.c_str());
    }
  }
}

Status BasePcpHandler::WaitForResult(const std::string& method_name,
                                     std::int64_t client_id,
                                     Future<Status>* future) {
  if (!future) {
    NEARBY_LOG(INFO, "No future to wait for; return with error");
    return {Status::kError};
  }
  NEARBY_LOG(INFO, "waiting for future to complete");
  ExceptionOr<Status> result = future->Get();
  if (!result.ok()) {
    NEARBY_LOG(INFO, "Future completed with exception: %d", result.exception());
    return {Status::kError};
  }
  NEARBY_LOG(INFO, "Future completed with status: %d", result.result().value);
  return result.result();
}

void BasePcpHandler::RunOnPcpHandlerThread(Runnable runnable) {
  serial_executor_.Execute(std::move(runnable));
}

EncryptionRunner::ResultListener BasePcpHandler::GetResultListener() {
  return {
      .on_success_cb =
          [this](const std::string& endpoint_id,
                 std::unique_ptr<UKey2Handshake> ukey2,
                 const std::string& auth_token,
                 const ByteArray& raw_auth_token) {
            RunOnPcpHandlerThread([this, endpoint_id,
                                   raw_ukey2 = ukey2.release(), auth_token,
                                   raw_auth_token]() mutable {
              OnEncryptionSuccessRunnable(
                  endpoint_id, std::unique_ptr<UKey2Handshake>(raw_ukey2),
                  auth_token, raw_auth_token);
            });
          },
      .on_failure_cb =
          [this](const std::string& endpoint_id, EndpointChannel* channel) {
            RunOnPcpHandlerThread([this, endpoint_id, channel]() {
              OnEncryptionFailureRunnable(endpoint_id, channel);
            });
          },
  };
}

void BasePcpHandler::OnEncryptionSuccessRunnable(
    const std::string& endpoint_id, std::unique_ptr<UKey2Handshake> ukey2,
    const std::string& auth_token, const ByteArray& raw_auth_token) {
  // Quick fail if we've been removed from pending connections while we were
  // busy running UKEY2.
  auto it = pending_connections_.find(endpoint_id);
  if (it == pending_connections_.end()) {
    NEARBY_LOG(INFO,
               "Connection not found on UKEY negotination complete; id=%s",
               endpoint_id.c_str());
    return;
  }

  BasePcpHandler::PendingConnectionInfo& connection_info = it->second;

  if (!ukey2) {
    // Fail early, if there is no crypto context.
    ProcessPreConnectionResultFailure(connection_info.client, endpoint_id);
    return;
  }

  connection_info.SetCryptoContext(std::move(ukey2));
  NEARBY_LOG(INFO, "Register encrypted connection; wait for response; id=%s",
             endpoint_id.c_str());

  // Set ourselves up so that we receive all acceptance/rejection messages
  handle_ = endpoint_manager_->RegisterFrameProcessor(
      V1Frame::CONNECTION_RESPONSE,
      static_cast<EndpointManager::FrameProcessor*>(this));

  // Now we register our endpoint so that we can listen for both sides to
  // accept.
  endpoint_manager_->RegisterEndpoint(
      connection_info.client, endpoint_id,
      {
          .remote_endpoint_name = connection_info.remote_endpoint_name,
          .authentication_token = auth_token,
          .raw_authentication_token = raw_auth_token,
          .is_incoming_connection = connection_info.is_incoming,
      },
      std::move(connection_info.channel), connection_info.listener);

  if (connection_info.result != nullptr) {
    NEARBY_LOG(INFO, "Connection established; Finalising future OK");
    connection_info.result->Set({Status::kSuccess});
    connection_info.result = nullptr;
  }
}

void BasePcpHandler::OnEncryptionFailureRunnable(
    const std::string& endpoint_id, EndpointChannel* endpoint_channel) {
  auto it = pending_connections_.find(endpoint_id);
  if (it == pending_connections_.end()) {
    NEARBY_LOG(INFO,
               "Connection not found on UKEY negotination complete; id=%s",
               endpoint_id.c_str());
    return;
  }

  BasePcpHandler::PendingConnectionInfo& info = it->second;
  // We had a bug here, caused by a race with EncryptionRunner. We now verify
  // the EndpointChannel to avoid it. In a simultaneous connection, we clean
  // up one of the two EndpointChannels and then update our pendingConnections
  // with the winning channel's state. Closing a channel that was in the
  // middle of EncryptionRunner would trigger onEncryptionFailed, and, since
  // the map had already updated with the winning EndpointChannel, we closed
  // it too by accident.
  if (*endpoint_channel != *info.channel) {
    NEARBY_LOG(
        INFO, "Not destroying channel [mismatch]: passed=%s; expected=%s",
        endpoint_channel->GetName().c_str(), info.channel->GetName().c_str());
    return;
  }

  ProcessPreConnectionInitiationFailure(endpoint_id, info.channel.get(),
                                        {Status::kEndpointIoError},
                                        info.result.get());
  info.result.reset();
}

Status BasePcpHandler::RequestConnection(ClientProxy* client,
                                         const std::string& endpoint_id,
                                         const ConnectionRequestInfo& info) {
  Future<Status> result;
  RunOnPcpHandlerThread([this, client, &info, endpoint_id, &result]() {
    absl::Time start_time = SystemClock::ElapsedRealtime();

    // If we already have a pending connection, then we shouldn't allow any more
    // outgoing connections to this endpoint.
    if (pending_connections_.count(endpoint_id)) {
      NEARBY_LOG(INFO, "Connection already exists: id=%s", endpoint_id.c_str());
      result.Set({Status::kAlreadyConnectedToEndpoint});
      return;
    }

    // If our child class says we can't send any more outgoing connections,
    // listen to them.
    if (ShouldEnforceTopologyConstraints() &&
        !CanSendOutgoingConnection(client)) {
      NEARBY_LOG(INFO, "Outgoing connection not allowed: id=%s",
                 endpoint_id.c_str());
      result.Set({Status::kOutOfOrderApiCall});
      return;
    }

    std::vector<DiscoveredEndpoint*> endpoints;
    auto endpoint = GetDiscoveredEndpoint(endpoint_id);
    if (endpoint == nullptr) {
      NEARBY_LOG(INFO, "Discovered endpoint not found: id=%s",
                 endpoint_id.c_str());
      result.Set({Status::kEndpointUnknown});
      return;
    }

    auto webrtc_endpoint = absl::make_unique<WebRtcEndpoint>(
        DiscoveredEndpoint{endpoint->endpoint_id, endpoint->endpoint_name,
                           endpoint->service_id,
                           proto::connections::Medium::WEB_RTC},
        CreatePeerIdFromAdvertisement(endpoint->service_id,
                                      endpoint->endpoint_id,
                                      endpoint->endpoint_name));
    endpoints.push_back(endpoint);
    endpoints.push_back(webrtc_endpoint.get());

    std::sort(endpoints.begin(), endpoints.end(),
              [this](DiscoveredEndpoint* a, DiscoveredEndpoint* b) -> bool {
                return IsPreferred(*a, *b);
              });

    std::unique_ptr<EndpointChannel> channel;
    ConnectImplResult connect_impl_result;

    for (auto connect_endpoint : endpoints) {
      connect_impl_result = ConnectImpl(client, connect_endpoint);
      if (connect_impl_result.status.Ok()) {
        channel = std::move(connect_impl_result.endpoint_channel);
        break;
      }
    }

    if (channel == nullptr) {
      NEARBY_LOG(INFO, "Endpoint channel not available: id=%s",
                 endpoint_id.c_str());
      ProcessPreConnectionInitiationFailure(
          endpoint_id, channel.get(), connect_impl_result.status, &result);
      return;
    }

    NEARBY_LOG(INFO, "Sending connection request: id=%s", endpoint_id.c_str());
    // Generate the nonce to use for this connection.
    std::int32_t nonce = prng_.NextInt32();

    // The first message we have to send, after connecting, is to tell the
    // endpoint about ourselves.
    Exception write_exception = WriteConnectionRequestFrame(
        channel.get(), client->GenerateLocalEndpointId(), info.name, nonce,
        GetConnectionMediumsByPriority());
    if (!write_exception.Ok()) {
      NEARBY_LOG(INFO, "Failed to send connection request: id=%s",
                 endpoint_id.c_str());
      ProcessPreConnectionInitiationFailure(
          endpoint_id, channel.get(), {Status::kEndpointIoError}, &result);
      return;
    }

    NEARBY_LOG(INFO, "adding connection to pending set: id=%s",
               endpoint_id.c_str());

    // We've successfully connected to the device, and are now about to jump on
    // to the EncryptionRunner thread to start running our encryption protocol.
    // We'll mark ourselves as pending in case we get another call to
    // requestConnection or OnIncomingConnection, so that we can cancel the
    // connection if needed.
    EndpointChannel* endpoint_channel =
        pending_connections_
            .emplace(endpoint_id,
                     PendingConnectionInfo{
                         .client = client,
                         .remote_endpoint_name = endpoint->endpoint_name,
                         .nonce = nonce,
                         .is_incoming = false,
                         .start_time = start_time,
                         .listener = info.listener,
                         .result = MakeSwapper(&result),
                         .channel = std::move(channel),
                     })
            .first->second.channel.get();

    NEARBY_LOG(INFO, "Initiating secure connection: id=%s",
               endpoint_id.c_str());
    // Next, we'll set up encryption. When it's done, our future will return and
    // requestConnection() will finish.
    encryption_runner_.StartClient(client, endpoint_id, endpoint_channel,
                                   GetResultListener());
  });
  NEARBY_LOG(INFO, "Waiting for connection to complete: id=%s",
             endpoint_id.c_str());
  auto status =
      WaitForResult(absl::StrCat("requestConnection(", endpoint_id, ")"),
                    client->GetClientId(), &result);
  NEARBY_LOG(INFO, "Wait is complete: id=%s; status=%d", endpoint_id.c_str(),
             status.value);
  return status;
}

BasePcpHandler::DiscoveredEndpoint* BasePcpHandler::GetDiscoveredEndpoint(
    const std::string& endpoint_id) {
  auto it = discovered_endpoints_.find(endpoint_id);
  if (it == discovered_endpoints_.end()) {
    return nullptr;
  }
  return it->second.get();
}

void BasePcpHandler::PendingConnectionInfo::SetCryptoContext(
    std::unique_ptr<UKey2Handshake> ukey2) {
  this->ukey2 = std::move(ukey2);
}

bool BasePcpHandler::HasOutgoingConnections(ClientProxy* client) const {
  for (const auto& item : pending_connections_) {
    auto& connection = item.second;
    if (!connection.is_incoming) {
      return true;
    }
  }
  return client->GetNumOutgoingConnections() > 0;
}

bool BasePcpHandler::HasIncomingConnections(ClientProxy* client) const {
  for (const auto& item : pending_connections_) {
    auto& connection = item.second;
    if (connection.is_incoming) {
      return true;
    }
  }
  return client->GetNumIncomingConnections() > 0;
}

bool BasePcpHandler::CanSendOutgoingConnection(ClientProxy* client) const {
  return true;
}

bool BasePcpHandler::CanReceiveIncomingConnection(ClientProxy* client) const {
  return true;
}

Exception BasePcpHandler::WriteConnectionRequestFrame(
    EndpointChannel* endpoint_channel, const std::string& local_endpoint_id,
    const std::string& local_endpoint_name, std::int32_t nonce,
    const std::vector<proto::connections::Medium>& supported_mediums) {
  return endpoint_channel->Write(parser::ForConnectionRequest(
      local_endpoint_id, local_endpoint_name, nonce, supported_mediums));
}

void BasePcpHandler::ProcessPreConnectionInitiationFailure(
    const std::string& endpoint_id, EndpointChannel* channel, Status status,
    Future<Status>* result) {
  if (channel != nullptr) {
    channel->Close();
  }

  pending_connections_.erase(endpoint_id);

  if (result != nullptr) {
    NEARBY_LOG(INFO, "Connection failed; aborting future");
    result->Set(status);
  }
}

void BasePcpHandler::ProcessPreConnectionResultFailure(
    ClientProxy* client, const std::string& endpoint_id) {
  auto item = pending_connections_.extract(endpoint_id);
  endpoint_manager_->DiscardEndpoint(client, endpoint_id);
  client->OnConnectionRejected(endpoint_id, {Status::kError});
}

bool BasePcpHandler::ShouldEnforceTopologyConstraints() const {
  // Topology constraints only matter for the advertiser.
  // For discoverers, we'll always enforce them.
  if (advertising_options_.strategy.IsNone()) {
    return true;
  }

  return advertising_options_.enforce_topology_constraints;
}

bool BasePcpHandler::AutoUpgradeBandwidth() const {
  if (advertising_options_.strategy.IsNone()) {
    return true;
  }

  return advertising_options_.auto_upgrade_bandwidth;
}

Status BasePcpHandler::AcceptConnection(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadListener& payload_listener) {
  Future<Status> response;
  RunOnPcpHandlerThread(
      [this, client, endpoint_id, payload_listener, &response]() {
        NEARBY_LOG(INFO, "AcceptConnection: id=%s", endpoint_id.c_str());
        if (!pending_connections_.count(endpoint_id)) {
          NEARBY_LOG(INFO, "AcceptConnection: no pending connection for id=%s",
                     endpoint_id.c_str());
          response.Set({Status::kEndpointUnknown});
          return;
        }
        auto& connection_info = pending_connections_[endpoint_id];

        // By this point in the flow, connection_info.channel has been
        // nulled out because ownership of that EndpointChannel was passed on to
        // EndpointChannelManager via a call to
        // EndpointManager::registerEndpoint(), so we now need to get access to
        // the EndpointChannel from the authoritative owner.
        std::shared_ptr<EndpointChannel> channel =
            channel_manager_->GetChannelForEndpoint(endpoint_id);
        if (channel == nullptr) {
          NEARBY_LOG(
              ERROR,
              "Channel destroyed before Accept; bring down connection: id=%s",
              endpoint_id.c_str());
          ProcessPreConnectionResultFailure(client, endpoint_id);
          response.Set({Status::kEndpointUnknown});
          return;
        }

        Exception write_exception =
            channel->Write(parser::ForConnectionResponse(Status::kSuccess));
        if (!write_exception.Ok()) {
          NEARBY_LOG(INFO, "AcceptConnection: failed to send response: id=%s",
                     endpoint_id.c_str());
          ProcessPreConnectionResultFailure(client, endpoint_id);
          response.Set({Status::kEndpointIoError});
          return;
        }

        NEARBY_LOG(INFO, "AcceptConnection: accepting locally: id=%s",
                   endpoint_id.c_str());
        connection_info.LocalEndpointAcceptedConnection(endpoint_id,
                                                        payload_listener);
        EvaluateConnectionResult(client, endpoint_id,
                                 false /* can_close_immediately */);
        response.Set({Status::kSuccess});
      });

  return WaitForResult(absl::StrCat("acceptConnection(", endpoint_id, ")"),
                       client->GetClientId(), &response);
}

Status BasePcpHandler::RejectConnection(ClientProxy* client,
                                        const std::string& endpoint_id) {
  Future<Status> response;
  RunOnPcpHandlerThread([this, client, endpoint_id, &response]() {
    NEARBY_LOG(INFO, "RejectConnection: id=%s", endpoint_id.c_str());
    if (!pending_connections_.count(endpoint_id)) {
      NEARBY_LOG(INFO, "RejectConnection: no pending connection for id=%s",
                 endpoint_id.c_str());
      response.Set({Status::kEndpointUnknown});
      return;
    }
    auto& connection_info = pending_connections_[endpoint_id];

    // By this point in the flow, connection_info->endpoint_channel_ has been
    // nulled out because ownership of that EndpointChannel was passed on to
    // EndpointChannelManager via a call to
    // EndpointManager::registerEndpoint(), so we now need to get access to the
    // EndpointChannel from the authoritative owner.
    std::shared_ptr<EndpointChannel> channel =
        channel_manager_->GetChannelForEndpoint(endpoint_id);
    if (channel == nullptr) {
      NEARBY_LOG(
          ERROR,
          "Channel destroyed before Reject; bring down connection: id=%s",
          endpoint_id.c_str());
      ProcessPreConnectionResultFailure(client, endpoint_id);
      response.Set({Status::kEndpointUnknown});
      return;
    }

    Exception write_exception = channel->Write(
        parser::ForConnectionResponse(Status::kConnectionRejected));
    if (!write_exception.Ok()) {
      NEARBY_LOG(INFO, "RejectConnection: failed to send response: id=%s",
                 endpoint_id.c_str());
      ProcessPreConnectionResultFailure(client, endpoint_id);
      response.Set({Status::kEndpointIoError});
      return;
    }

    NEARBY_LOG(INFO, "RejectConnection: rejecting locally: id=%s",
               endpoint_id.c_str());
    connection_info.LocalEndpointRejectedConnection(endpoint_id);
    EvaluateConnectionResult(client, endpoint_id,
                             false /* can_close_immediately */);
    response.Set({Status::kSuccess});
  });

  return WaitForResult(absl::StrCat("rejectConnection(", endpoint_id, ")"),
                       client->GetClientId(), &response);
}

// proto::connections::Medium BasePcpHandler::GetBandwidthUpgradeMedium() {
//  return bandwidth_upgrade_medium_.Get();
//}

void BasePcpHandler::OnIncomingFrame(OfflineFrame& frame,
                                     const std::string& endpoint_id,
                                     ClientProxy* client,
                                     proto::connections::Medium medium) {
  CountDownLatch latch(1);
  RunOnPcpHandlerThread([this, client, endpoint_id, frame, &latch]() {
    NEARBY_LOG(INFO, "OnConnectionResponse: id=%s", endpoint_id.c_str());

    if (client->HasRemoteEndpointResponded(endpoint_id)) {
      NEARBY_LOG(INFO, "OnConnectionResponse: already handled; id=%s",
                 endpoint_id.c_str());
      return;
    }

    const ConnectionResponseFrame& connection_response =
        frame.v1().connection_response();

    if (connection_response.status() == Status::kSuccess) {
      NEARBY_LOG(INFO, "OnConnectionResponse: remote accepted; id=%s",
                 endpoint_id.c_str());
      client->RemoteEndpointAcceptedConnection(endpoint_id);
    } else {
      NEARBY_LOG(INFO,
                 "OnConnectionResponse: remote rejected; id=%s; status=%d",
                 endpoint_id.c_str(), connection_response.status());
      client->RemoteEndpointRejectedConnection(endpoint_id);
    }

    EvaluateConnectionResult(client, endpoint_id,
                             /* can_close_immediately= */ true);

    latch.CountDown();
  });
  WaitForLatch("OnIncomingFrame()", &latch);
}

void BasePcpHandler::OnEndpointDisconnect(ClientProxy* client,
                                          const std::string& endpoint_id,
                                          CountDownLatch* barrier) {
  if (stop_.Get()) {
    if (barrier) barrier->CountDown();
    return;
  }
  RunOnPcpHandlerThread([this, client, endpoint_id, barrier]() {
    auto item = pending_alarms_.find(endpoint_id);
    if (item != pending_alarms_.end()) {
      auto& alarm = item->second;
      alarm.Cancel();
      pending_alarms_.erase(item);
    }
    ProcessPreConnectionResultFailure(client, endpoint_id);
    barrier->CountDown();
  });
}

ConnectionOptions BasePcpHandler::GetConnectionOptions() const {
  return advertising_options_;
}

void BasePcpHandler::OnEndpointFound(
    ClientProxy* client,
    std::shared_ptr<BasePcpHandler::DiscoveredEndpoint> endpoint) {
  // Check if we've seen this endpoint ID before.
  std::string& endpoint_id = endpoint->endpoint_id;
  BasePcpHandler::DiscoveredEndpoint* previously_discovered_endpoint =
      GetDiscoveredEndpoint(endpoint_id);

  NEARBY_LOG(INFO, "OnEndpointFound: id='%s' [enter]", endpoint_id.c_str());
  if (previously_discovered_endpoint == nullptr) {
    // If this is the first medium we've discovered this endpoint over, then add
    // it to the map.
    const auto& owned_endpoint =
        discovered_endpoints_.emplace(endpoint_id, std::move(endpoint))
            .first->second;

    NEARBY_LOG(INFO, "Adding new endpoint: id=%s", endpoint_id.c_str());
    // And, as it's the first time, report it to the client.
    client->OnEndpointFound(
        owned_endpoint->service_id, owned_endpoint->endpoint_id,
        owned_endpoint->endpoint_name, owned_endpoint->medium);
  } else if (previously_discovered_endpoint->endpoint_name !=
             endpoint->endpoint_name) {
    // If we've already seen this endpoint before, check if there was a name
    // change. If there was, report the previous endpoint as lost.
    NEARBY_LOG(INFO, "Switch to new endpoint: id=%s", endpoint_id.c_str());

    OnEndpointLost(client, *previously_discovered_endpoint);
    OnEndpointFound(client, std::move(endpoint));
  } else {
    // Otherwise, we need to see if the medium we discovered the endpoint over
    // this time is better than the medium we originally discovered the endpoint
    // over.
    NEARBY_LOG(INFO, "Rediscovered endpoint on new media: id=%s",
               endpoint_id.c_str());
    if (IsPreferred(*endpoint, *previously_discovered_endpoint)) {
      discovered_endpoints_.insert_or_assign(endpoint_id, std::move(endpoint));
    }
  }
}

void BasePcpHandler::OnEndpointLost(
    ClientProxy* client, const BasePcpHandler::DiscoveredEndpoint& endpoint) {
  // Look up the DiscoveredEndpoint we have in our cache.
  const auto* discovered_endpoint = GetDiscoveredEndpoint(endpoint.endpoint_id);
  if (discovered_endpoint == nullptr) {
    NEARBY_LOG(INFO, "No previous endpoint (nothing to lose): id=%s",
               endpoint.endpoint_id.c_str());
    return;
  }

  // Validate that the cached endpoint has the same name as the one reported as
  // onLost. If the name differs, then no-op. This likely means that the remote
  // device changed their name. We reported onFound for the new name and are
  // just now figuring out that we lost the old name.
  if (discovered_endpoint->endpoint_name != endpoint.endpoint_name) {
    NEARBY_LOG(INFO, "Previous endpoint name mismatch; passed=%s; expected=%s",
               endpoint.endpoint_name.c_str(),
               discovered_endpoint->endpoint_name.c_str());
    return;
  }

  auto item = discovered_endpoints_.extract(endpoint.endpoint_id);
  client->OnEndpointLost(endpoint.service_id, endpoint.endpoint_id);
}

bool BasePcpHandler::IsPreferred(
    const BasePcpHandler::DiscoveredEndpoint& new_endpoint,
    const BasePcpHandler::DiscoveredEndpoint& old_endpoint) {
  std::vector<proto::connections::Medium> mediums =
      GetConnectionMediumsByPriority();
  // As we iterate through the list of mediums, we see if we run into the new
  // endpoint's medium or the old endpoint's medium first.
  for (const auto& medium : mediums) {
    if (medium == new_endpoint.medium) {
      // The new endpoint's medium came first. It's preferred!
      return true;
    }

    if (medium == old_endpoint.medium) {
      // The old endpoint's medium came first. Stick with the old endpoint!
      return false;
    }
  }
  NEARBY_LOG(FATAL, "Failed to determine preferred medium; bailing out");
  return false;
}

Exception BasePcpHandler::OnIncomingConnection(
    ClientProxy* client, const std::string& remote_device_name,
    std::unique_ptr<EndpointChannel> channel,
    proto::connections::Medium medium) {
  absl::Time start_time = SystemClock::ElapsedRealtime();

  //  Fixes an NPE in ClientProxy.OnConnectionResult. The crash happened when
  //  the client stopped advertising and we nulled out state, followed by an
  //  incoming connection where we attempted to check that state.
  if (!client->IsAdvertising()) {
    NEARBY_LOG(WARNING,
               "Ignoring incoming connection because client 0x%" PRIX64
               " is no longer advertising.",
               client->GetClientId());
    return {Exception::kIo};
  }

  // Endpoints connecting to us will always tell us about themselves first.
  ExceptionOr<OfflineFrame> wrapped_frame =
      ReadConnectionRequestFrame(channel.get());

  if (!wrapped_frame.ok()) {
    if (wrapped_frame.exception()) {
      NEARBY_LOG(
          ERROR,
          "Failed to parse incoming connection request; client_id=0x%" PRIX64
          "; device=%s",
          client->GetClientId(), remote_device_name.c_str());
      ProcessPreConnectionInitiationFailure("", channel.get(), {Status::kError},
                                            nullptr);
      return {Exception::kSuccess};
    }
    return wrapped_frame.GetException();
  }

  OfflineFrame& frame = wrapped_frame.result();
  const ConnectionRequestFrame& connection_request =
      frame.v1().connection_request();
  NEARBY_LOG(INFO,
             "Incoming connection request; client_id=0x%" PRIX64
             "; device=%s; id=%s",
             client->GetClientId(), remote_device_name.c_str(),
             connection_request.endpoint_id().c_str());
  if (client->IsConnectedToEndpoint(connection_request.endpoint_id())) {
    return {Exception::kIo};
  }

  // If we've already sent out a connection request to this endpoint, then this
  // is where we need to decide which connection to break.
  if (BreakTie(client, connection_request.endpoint_id(),
               connection_request.nonce(), channel.get())) {
    return {Exception::kSuccess};
  }

  // If our child class says we can't accept any more incoming connections,
  // listen to them.
  if (ShouldEnforceTopologyConstraints() &&
      !CanReceiveIncomingConnection(client)) {
    return {Exception::kIo};
  }

  // The ConnectionRequest frame has two fields that both contain the
  // EndpointInfo. The legacy field stores it as a string while the newer field
  // stores it as a byte array. We'll attempt to grab from the newer field, but
  // will accept the older string if it's all that exists.
  const std::string endpoint_name = connection_request.has_endpoint_info()
                                        ? connection_request.endpoint_info()
                                        : connection_request.endpoint_name();

  // We've successfully connected to the device, and are now about to jump on to
  // the EncryptionRunner thread to start running our encryption protocol. We'll
  // mark ourselves as pending in case we get another call to requestConnection
  // or OnIncomingConnection, so that we can cancel the connection if needed.
  auto* owned_channel =
      pending_connections_
          .emplace(connection_request.endpoint_id(),
                   PendingConnectionInfo{
                       .client = client,
                       .remote_endpoint_name = endpoint_name,
                       .nonce = connection_request.nonce(),
                       .is_incoming = true,
                       .start_time = start_time,
                       .listener = advertising_listener_,
                       .supported_mediums =
                           parser::ConnectionRequestMediumsToMediums(
                               connection_request),
                       .channel = std::move(channel),
                   })
          .first->second.channel.get();

  // Next, we'll set up encryption.
  encryption_runner_.StartServer(client, connection_request.endpoint_id(),
                                 owned_channel, GetResultListener());
  return {Exception::kSuccess};
}

bool BasePcpHandler::BreakTie(ClientProxy* client,
                              const std::string& endpoint_id,
                              std::int32_t incoming_nonce,
                              EndpointChannel* endpoint_channel) {
  auto it = pending_connections_.find(endpoint_id);
  if (it != pending_connections_.end()) {
    BasePcpHandler::PendingConnectionInfo& info = it->second;

    NEARBY_LOG(INFO, "BreakTie: id=%s", endpoint_id.c_str());
    // Break the lowest connection. In the (extremely) rare case of a tie, break
    // both.
    if (info.nonce > incoming_nonce) {
      // Our connection won! Clean up their connection.
      endpoint_channel->Close();

      NEARBY_LOG(INFO, "BreakTie: We won; id=%s", endpoint_id.c_str());
      return true;
    } else if (info.nonce < incoming_nonce) {
      // Aw, we lost. Clean up our connection, and then we'll let their
      // connection continue on.
      ProcessTieBreakLoss(client, endpoint_id, &info);

      NEARBY_LOG(INFO, "BreakTie: We lost; id=%s", endpoint_id.c_str());
    } else {
      // Oh. Huh. We both lost. Well, that's awkward. We'll clean up both and
      // just force the devices to retry.
      endpoint_channel->Close();

      ProcessTieBreakLoss(client, endpoint_id, &info);

      NEARBY_LOG(INFO, "BreakTie: Both lost; id=%s", endpoint_id.c_str());
      return true;
    }
  }

  return false;
}

void BasePcpHandler::ProcessTieBreakLoss(
    ClientProxy* client, const std::string& endpoint_id,
    BasePcpHandler::PendingConnectionInfo* info) {
  ProcessPreConnectionInitiationFailure(endpoint_id, info->channel.get(),
                                        {Status::kEndpointIoError},
                                        info->result.get());
  info->result = nullptr;
  ProcessPreConnectionResultFailure(client, endpoint_id);
}

void BasePcpHandler::InitiateBandwidthUpgrade(
    ClientProxy* client, const std::string& endpoint_id,
    const std::vector<proto::connections::Medium>& supported_mediums) {
  // When we successfully connect to a remote endpoint and a bandwidth upgrade
  // medium has not yet been decided, we'll pick the highest bandwidth medium
  // supported by both us and the remote endpoint. Once we pick a medium, all
  // future connections will use it too. eg. If we chose Wifi LAN, we'll attempt
  // to upgrade the 2nd, 3rd, etc remote endpoints with Wifi LAN even if they're
  // on a different network (or had a better medium). This is a quick and easy
  // way to prevent mediums, like Wifi Hotspot, from interfering with active
  // connections (although it's suboptimal for bandwidth throughput). When all
  // endpoints disconnect, we reset the bandwidth upgrade medium.
  if (bandwidth_upgrade_medium_.Get() ==
      proto::connections::Medium::UNKNOWN_MEDIUM) {
    bandwidth_upgrade_medium_.Set(ChooseBestUpgradeMedium(supported_mediums));
  }

  if (AutoUpgradeBandwidth() && (bandwidth_upgrade_medium_.Get() !=
                                 proto::connections::Medium::UNKNOWN_MEDIUM)) {
    // TODO(apolyudov): Bring bandwidth upgrade back, when it is ready.
    //    bandwidth_upgrade_->InitiateBandwidthUpgradeForEndpoint(
    //        client, endpoint_id, bandwidth_upgrade_medium_.Get());
  }
}

proto::connections::Medium BasePcpHandler::ChooseBestUpgradeMedium(
    const std::vector<proto::connections::Medium>& their_supported_mediums) {
  // If the remote side did not report their supported mediums, choose an
  // appropriate default.
  std::vector<proto::connections::Medium> their_mediums =
      their_supported_mediums;
  if (their_supported_mediums.empty()) {
    their_mediums.push_back(GetDefaultUpgradeMedium());
  }

  // Otherwise, pick the best medium we support.
  std::vector<proto::connections::Medium> my_mediums =
      GetConnectionMediumsByPriority();
  for (const auto& my_medium : my_mediums) {
    for (const auto& their_medium : their_mediums) {
      if (my_medium == their_medium) {
        return my_medium;
      }
    }
  }

  return proto::connections::Medium::UNKNOWN_MEDIUM;
}

void BasePcpHandler::EvaluateConnectionResult(ClientProxy* client,
                                              const std::string& endpoint_id,
                                              bool can_close_immediately) {
  // Short-circuit immediately if we're not in an actionable state yet. We will
  // be called again once the other side has made their decision.
  if (!client->IsConnectionAccepted(endpoint_id) &&
      !client->IsConnectionRejected(endpoint_id)) {
    if (!client->HasLocalEndpointResponded(endpoint_id)) {
      NEARBY_LOG(INFO, "ConnectionResult: local client did not respond; id=%s",
                 endpoint_id.c_str());
    } else if (!client->HasRemoteEndpointResponded(endpoint_id)) {
      NEARBY_LOG(INFO, "ConnectionResult: remote client did not respond; id=%s",
                 endpoint_id.c_str());
    }
    return;
  }

  // Clean up the endpoint channel from our list of 'pending' connections. It's
  // no longer pending.
  auto it = pending_connections_.find(endpoint_id);
  if (it == pending_connections_.end()) {
    NEARBY_LOG(INFO, "No pending connection to evaluate; id=%s",
               endpoint_id.c_str());
    return;
  }

  auto pair = pending_connections_.extract(it);
  BasePcpHandler::PendingConnectionInfo& connection_info = pair.mapped();
  bool is_connection_accepted = client->IsConnectionAccepted(endpoint_id);

  Status response_code;
  if (is_connection_accepted) {
    NEARBY_LOG(INFO, "Pending connection accepted; id=%s", endpoint_id.c_str());
    response_code = {Status::kSuccess};

    // Both sides have accepted, so we can now start talking over encrypted
    // channels
    // Now, after both parties accepted connection (presumably after verifying &
    // matching security tokens), we are allowed to extract the shared key.
    auto ukey2 = std::move(connection_info.ukey2);
    bool succeeded = ukey2->VerifyHandshake();
    CHECK(succeeded);  // If this fails, it's a UKEY2 protocol bug.
    auto context = ukey2->ToConnectionContext();
    CHECK(context);  // there is no way how this can fail, if Verify succeeded.
    // If it did, it's a UKEY2 protocol bug.

    channel_manager_->EncryptChannelForEndpoint(endpoint_id,
                                                std::move(context));
  } else {
    NEARBY_LOG(INFO, "Pending connection rejected; id=%s", endpoint_id.c_str());
    response_code = {Status::kConnectionRejected};
  }

  // Invoke the client callback to let it know of the connection result.
  if (response_code.Ok()) {
    client->OnConnectionAccepted(endpoint_id);
  } else {
    client->OnConnectionRejected(endpoint_id, response_code);
  }

  // If the connection failed, clean everything up and short circuit.
  if (!is_connection_accepted) {
    // Clean up the channel in EndpointManager if it's no longer required.
    if (can_close_immediately) {
      endpoint_manager_->DiscardEndpoint(client, endpoint_id);
    } else {
      pending_alarms_.emplace(
          endpoint_id,
          CancelableAlarm(
              "BasePcpHandler.evaluateConnectionResult() delayed close",
              [this, client, endpoint_id]() {
                endpoint_manager_->DiscardEndpoint(client, endpoint_id);
              },
              kRejectedConnectionCloseDelay, &alarm_executor_));
    }

    return;
  }

  // Kick off the bandwidth upgrade for incoming connections.
  if (connection_info.is_incoming) {
    InitiateBandwidthUpgrade(client, endpoint_id,
                             connection_info.supported_mediums);
  }
}

ExceptionOr<OfflineFrame> BasePcpHandler::ReadConnectionRequestFrame(
    EndpointChannel* endpoint_channel) {
  if (endpoint_channel == nullptr) {
    return ExceptionOr<OfflineFrame>(Exception::kIo);
  }

  // To avoid a device connecting but never sending their introductory frame, we
  // time out the connection after a certain amount of time.
  CancelableAlarm timeout_alarm(
      absl::StrCat("PcpHandler(", this->GetStrategy().GetName(),
                   ")::ReadConnectionRequestFrame"),
      [endpoint_channel]() { endpoint_channel->Close(); },
      kConnectionRequestReadTimeout, &alarm_executor_);
  // Do a blocking read to try and find the ConnectionRequestFrame
  ExceptionOr<ByteArray> wrapped_bytes = endpoint_channel->Read();
  timeout_alarm.Cancel();

  if (!wrapped_bytes.ok()) {
    return ExceptionOr<OfflineFrame>(wrapped_bytes.exception());
  }

  ByteArray bytes = std::move(wrapped_bytes.result());
  ExceptionOr<OfflineFrame> wrapped_frame = parser::FromBytes(bytes);
  if (wrapped_frame.GetException().Raised(Exception::kInvalidProtocolBuffer)) {
    return ExceptionOr<OfflineFrame>(Exception::kIo);
  }

  OfflineFrame& frame = wrapped_frame.result();
  if (V1Frame::CONNECTION_REQUEST != parser::GetFrameType(frame)) {
    return ExceptionOr<OfflineFrame>(Exception::kIo);
  }

  return wrapped_frame;
}

///////////////////// BasePcpHandler::PendingConnectionInfo ///////////////////

BasePcpHandler::PendingConnectionInfo::~PendingConnectionInfo() {
  if (result != nullptr) {
    NEARBY_LOG(INFO, "Future was not set; destroying info");
    result->Set({Status::kError});
  }

  if (channel != nullptr) {
    channel->Close(proto::connections::DisconnectionReason::SHUTDOWN);
  }

  // Destroy crypto context now; for some reason, crypto context destructor
  // segfaults if it is not destroyed here.
  this->ukey2.reset();
}

void BasePcpHandler::PendingConnectionInfo::LocalEndpointAcceptedConnection(
    const std::string& endpoint_id, const PayloadListener& payload_listener) {
  client->LocalEndpointAcceptedConnection(endpoint_id, payload_listener);
}

void BasePcpHandler::PendingConnectionInfo::LocalEndpointRejectedConnection(
    const std::string& endpoint_id) {
  client->LocalEndpointRejectedConnection(endpoint_id);
}

mediums::PeerId BasePcpHandler::CreatePeerIdFromAdvertisement(
    const std::string& service_id, const std::string& endpoint_id,
    const std::string& endpoint_name) {
  std::string seed = absl::StrCat(service_id, endpoint_id, endpoint_name);
  return mediums::PeerId::FromSeed(ByteArray(std::move(seed)));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
