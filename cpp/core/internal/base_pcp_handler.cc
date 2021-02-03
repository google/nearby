#include "core/internal/base_pcp_handler.h"

#include <cassert>
#include <cinttypes>
#include <cstdlib>
#include <limits>
#include <memory>

#include "core/internal/offline_frames.h"
#include "core/internal/pcp_handler.h"
#include "core/options.h"
#include "platform/base/bluetooth_utils.h"
#include "platform/public/logging.h"
#include "platform/public/system_clock.h"
#include "securegcm/d2d_connection_context_v1.h"
#include "securegcm/ukey2_handshake.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/escaping.h"
#include "absl/types/span.h"

namespace location {
namespace nearby {
namespace connections {

using ::location::nearby::proto::connections::Medium;
using ::securegcm::UKey2Handshake;

constexpr absl::Duration BasePcpHandler::kConnectionRequestReadTimeout;
constexpr absl::Duration BasePcpHandler::kRejectedConnectionCloseDelay;

BasePcpHandler::BasePcpHandler(Mediums* mediums,
                               EndpointManager* endpoint_manager,
                               EndpointChannelManager* channel_manager,
                               BwuManager* bwu_manager, Pcp pcp)
    : mediums_(mediums),
      endpoint_manager_(endpoint_manager),
      channel_manager_(channel_manager),
      pcp_(pcp),
      bwu_manager_(bwu_manager) {}

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
  NEARBY_LOG(INFO, "StartAdvertising with supported mediums: %s",
             GetStringValueOfSupportedMediums(options).c_str());
  ConnectionOptions advertising_options = options.CompatibleOptions();
  RunOnPcpHandlerThread([this, client, &service_id, &info, &advertising_options,
                         &response]() {
    auto result =
        StartAdvertisingImpl(client, service_id, client->GetLocalEndpointId(),
                             info.endpoint_info, advertising_options);
    if (!result.status.Ok()) {
      response.Set(result.status);
      return;
    }

    // Now that we've succeeded, mark the client as advertising.
    // Save the advertising options for local reference in later process like
    // upgrading bandwidth.
    std::vector<proto::connections::Medium> supported_mediums =
        advertising_options.GetMediums();
    advertising_listener_ = info.listener;
    client->StartedAdvertising(service_id, GetStrategy(), info.listener,
                               absl::MakeSpan(result.mediums),
                               advertising_options);
    response.Set({Status::kSuccess});
  });
  return WaitForResult(
      absl::StrCat("StartAdvertising(", std::string(info.endpoint_info), ")"),
      client->GetClientId(), &response);
}

void BasePcpHandler::StopAdvertising(ClientProxy* client) {
  CountDownLatch latch(1);
  RunOnPcpHandlerThread([this, client, &latch]() {
    StopAdvertisingImpl(client);
    client->StoppedAdvertising();
    latch.CountDown();
  });
  WaitForLatch("StopAdvertising", &latch);
}

std::string BasePcpHandler::GetStringValueOfSupportedMediums(
    const ConnectionOptions& options) const {
  std::ostringstream result;
  result << "{ ";
  if (options.allowed.bluetooth) result << "bluetooth ";
  if (options.allowed.ble) result << "ble ";
  if (options.allowed.web_rtc) result << "webrtc ";
  if (options.allowed.wifi_lan) result << "wifilan ";
  result << "}";
  return result.str();
}

Status BasePcpHandler::StartDiscovery(ClientProxy* client,
                                      const std::string& service_id,
                                      const ConnectionOptions& options,
                                      const DiscoveryListener& listener) {
  Future<Status> response;
  ConnectionOptions discovery_options = options.CompatibleOptions();

  NEARBY_LOG(INFO, "StartDiscovery with supported mediums: %s",
             GetStringValueOfSupportedMediums(options).c_str());
  RunOnPcpHandlerThread([this, client, service_id, discovery_options, &listener,
                         &response]() {
    // Ask the implementation to attempt to start discovery.
    auto result = StartDiscoveryImpl(client, service_id, discovery_options);
    if (!result.status.Ok()) {
      response.Set(result.status);
      return;
    }

    // Now that we've succeeded, mark the client as discovering and clear
    // out any old endpoints we had discovered.
    discovered_endpoints_.clear();
    client->StartedDiscovery(service_id, GetStrategy(), listener,
                             absl::MakeSpan(result.mediums), discovery_options);
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
    latch.CountDown();
  });

  WaitForLatch("StopDiscovery", &latch);
}

void BasePcpHandler::InjectEndpoint(
    ClientProxy* client, const std::string& service_id,
    const OutOfBandConnectionMetadata& metadata) {
  CountDownLatch latch(1);
  RunOnPcpHandlerThread([this, client, service_id, metadata, &latch]() {
    InjectEndpointImpl(client, service_id, metadata);
    latch.CountDown();
  });

  WaitForLatch(absl::StrCat("InjectEndpoint(", service_id, ")"), &latch);
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
    NEARBY_LOG(INFO, "Future:[%s] completed with exception: %d",
               method_name.c_str(), result.exception());
    return {Status::kError};
  }
  NEARBY_LOG(INFO, "Future:[%s] completed with status: %d", method_name.c_str(),
             result.result().value);
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
              NEARBY_LOG(ERROR, "Encryption failed for %s on medium %d",
                         endpoint_id.c_str(), channel->GetMedium());
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
    ProcessPreConnectionInitiationFailure(
        endpoint_id, connection_info.channel.get(), {Status::kEndpointIoError},
        connection_info.result.lock().get());
    connection_info.result.reset();
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
          .remote_endpoint_info = connection_info.remote_endpoint_info,
          .authentication_token = auth_token,
          .raw_authentication_token = raw_auth_token,
          .is_incoming_connection = connection_info.is_incoming,
      },
      connection_info.options, std::move(connection_info.channel),
      connection_info.listener);

  if (auto future_status = connection_info.result.lock()) {
    NEARBY_LOG(INFO, "Connection established; Finalising future OK");
    future_status->Set({Status::kSuccess});
    connection_info.result.reset();
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
                                        info.result.lock().get());
  info.result.reset();
}

Status BasePcpHandler::RequestConnection(ClientProxy* client,
                                         const std::string& endpoint_id,
                                         const ConnectionRequestInfo& info,
                                         const ConnectionOptions& options) {
  auto result = std::make_shared<Future<Status>>();
  RunOnPcpHandlerThread([this, client, &info, options, endpoint_id, result]() {
    absl::Time start_time = SystemClock::ElapsedRealtime();

    // If we already have a pending connection, then we shouldn't allow any more
    // outgoing connections to this endpoint.
    if (pending_connections_.count(endpoint_id)) {
      NEARBY_LOG(INFO, "Connection already exists: id=%s", endpoint_id.c_str());
      result->Set({Status::kAlreadyConnectedToEndpoint});
      return;
    }

    // If our child class says we can't send any more outgoing connections,
    // listen to them.
    if (ShouldEnforceTopologyConstraints(client->GetAdvertisingOptions()) &&
        !CanSendOutgoingConnection(client)) {
      NEARBY_LOG(INFO, "Outgoing connection not allowed: id=%s",
                 endpoint_id.c_str());
      result->Set({Status::kOutOfOrderApiCall});
      return;
    }

    DiscoveredEndpoint* endpoint = GetDiscoveredEndpoint(endpoint_id);
    if (endpoint == nullptr) {
      NEARBY_LOG(INFO, "Discovered endpoint not found: id=%s",
                 endpoint_id.c_str());
      result->Set({Status::kEndpointUnknown});
      return;
    }

    auto remote_bluetooth_mac_address =
        BluetoothUtils::ToString(options.remote_bluetooth_mac_address);
    if (!remote_bluetooth_mac_address.empty()) {
      if (AppendRemoteBluetoothMacAddressEndpoint(
              endpoint_id, remote_bluetooth_mac_address,
              client->GetDiscoveryOptions()))
        NEARBY_LOGS(INFO) << "Appended remote Bluetooth MAC Address endpoint "
                          << "[" << remote_bluetooth_mac_address << "]";
    }

    if (AppendWebRTCEndpoint(endpoint_id, client->GetDiscoveryOptions()))
      NEARBY_LOGS(INFO) << "Appended Web RTC endpoint.";

    auto discovered_endpoints = GetDiscoveredEndpoints(endpoint_id);
    std::unique_ptr<EndpointChannel> channel;
    ConnectImplResult connect_impl_result;

    for (auto connect_endpoint : discovered_endpoints) {
      if (!MediumSupportedByClientOptions(connect_endpoint->medium,
                                          client->GetDiscoveryOptions()))
        continue;
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
          endpoint_id, channel.get(), connect_impl_result.status, result.get());
      return;
    }

    NEARBY_LOG(INFO, "Sending connection request: id=%s", endpoint_id.c_str());
    // Generate the nonce to use for this connection.
    std::int32_t nonce = prng_.NextInt32();

    // The first message we have to send, after connecting, is to tell the
    // endpoint about ourselves.
    Exception write_exception = WriteConnectionRequestFrame(
        channel.get(), client->GetLocalEndpointId(), info.endpoint_info, nonce,
        GetSupportedConnectionMediumsByPriority(client->GetDiscoveryOptions()));
    if (!write_exception.Ok()) {
      NEARBY_LOG(INFO, "Failed to send connection request: id=%s",
                 endpoint_id.c_str());
      ProcessPreConnectionInitiationFailure(
          endpoint_id, channel.get(), {Status::kEndpointIoError}, result.get());
      return;
    }

    NEARBY_LOG(INFO, "adding connection to pending set: id=%s",
               endpoint_id.c_str());

    // We've successfully connected to the device, and are now about to jump on
    // to the EncryptionRunner thread to start running our encryption protocol.
    // We'll mark ourselves as pending in case we get another call to
    // RequestConnection or OnIncomingConnection, so that we can cancel the
    // connection if needed.
    EndpointChannel* endpoint_channel =
        pending_connections_
            .emplace(endpoint_id,
                     PendingConnectionInfo{
                         .client = client,
                         .remote_endpoint_info = endpoint->endpoint_info,
                         .nonce = nonce,
                         .is_incoming = false,
                         .start_time = start_time,
                         .listener = info.listener,
                         .options = options,
                         .result = result,
                         .channel = std::move(channel),
                     })
            .first->second.channel.get();

    NEARBY_LOG(INFO, "Initiating secure connection: id=%s",
               endpoint_id.c_str());
    // Next, we'll set up encryption. When it's done, our future will return and
    // RequestConnection() will finish.
    encryption_runner_.StartClient(client, endpoint_id, endpoint_channel,
                                   GetResultListener());
  });
  NEARBY_LOG(INFO, "Waiting for connection to complete: id=%s",
             endpoint_id.c_str());
  auto status =
      WaitForResult(absl::StrCat("RequestConnection(", endpoint_id, ")"),
                    client->GetClientId(), result.get());
  NEARBY_LOG(INFO, "Wait is complete: id=%s; status=%d", endpoint_id.c_str(),
             status.value);
  return status;
}

bool BasePcpHandler::MediumSupportedByClientOptions(
    const proto::connections::Medium& medium,
    const ConnectionOptions& client_options) const {
  for (auto supported_medium : client_options.GetMediums()) {
    if (medium == supported_medium) {
      return true;
    }
  }
  return false;
}

// Get ordered supported connection medium based on local advertising/discovery
// option.
std::vector<proto::connections::Medium>
BasePcpHandler::GetSupportedConnectionMediumsByPriority(
    const ConnectionOptions& local_option) {
  std::vector<proto::connections::Medium> supported_mediums_by_priority;
  for (auto medium_by_priority : GetConnectionMediumsByPriority()) {
    if (MediumSupportedByClientOptions(medium_by_priority, local_option)) {
      supported_mediums_by_priority.push_back(medium_by_priority);
    }
  }
  return supported_mediums_by_priority;
}

// Get any single discovered endpoint for a given endpoint_id.
BasePcpHandler::DiscoveredEndpoint* BasePcpHandler::GetDiscoveredEndpoint(
    const std::string& endpoint_id) {
  auto it = discovered_endpoints_.find(endpoint_id);
  if (it == discovered_endpoints_.end()) {
    return nullptr;
  }
  return it->second.get();
}

std::vector<BasePcpHandler::DiscoveredEndpoint*>
BasePcpHandler::GetDiscoveredEndpoints(const std::string& endpoint_id) {
  std::vector<BasePcpHandler::DiscoveredEndpoint*> result;
  auto it = discovered_endpoints_.equal_range(endpoint_id);
  for (auto item = it.first; item != it.second; item++) {
    result.push_back(item->second.get());
  }
  std::sort(result.begin(), result.end(),
            [this](DiscoveredEndpoint* a, DiscoveredEndpoint* b) -> bool {
              return IsPreferred(*a, *b);
            });

  return result;
}

std::vector<BasePcpHandler::DiscoveredEndpoint*>
BasePcpHandler::GetDiscoveredEndpoints(
    const proto::connections::Medium medium) {
  std::vector<BasePcpHandler::DiscoveredEndpoint*> result;
  for (const auto& item : discovered_endpoints_) {
    if (item.second->medium == medium) {
      result.push_back(item.second.get());
    }
  }
  return result;
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
    const ByteArray& local_endpoint_info, std::int32_t nonce,
    const std::vector<proto::connections::Medium>& supported_mediums) {
  // TODO(b/172178926): Add WifiLan 5GHz and BSSID support.
  return endpoint_channel->Write(parser::ForConnectionRequest(
      local_endpoint_id, local_endpoint_info, nonce, /*supports_5_ghz =*/false,
      /*bssid=*/std::string{}, supported_mediums));
}

void BasePcpHandler::ProcessPreConnectionInitiationFailure(
    const std::string& endpoint_id, EndpointChannel* channel, Status status,
    Future<Status>* result) {
  if (channel != nullptr) {
    channel->Close();
  }

  if (result != nullptr) {
    NEARBY_LOG(INFO, "Connection failed; aborting future");
    result->Set(status);
  }

  // result is hold inside a swapper, and saved in PendingConnectionInfo.
  // PendingConnectionInfo destructor will clear the memory of SettableFuture
  // shared_ptr for result.
  pending_connections_.erase(endpoint_id);
}

void BasePcpHandler::ProcessPreConnectionResultFailure(
    ClientProxy* client, const std::string& endpoint_id) {
  auto item = pending_connections_.extract(endpoint_id);
  endpoint_manager_->DiscardEndpoint(client, endpoint_id);
  client->OnConnectionRejected(endpoint_id, {Status::kError});
}

bool BasePcpHandler::ShouldEnforceTopologyConstraints(
    const ConnectionOptions& local_advertising_options) const {
  // Topology constraints only matter for the advertiser.
  // For discoverers, we'll always enforce them.
  if (local_advertising_options.strategy.IsNone()) {
    return true;
  }

  return local_advertising_options.enforce_topology_constraints;
}

bool BasePcpHandler::AutoUpgradeBandwidth(
    const ConnectionOptions& local_advertising_options) const {
  if (local_advertising_options.strategy.IsNone()) {
    return true;
  }

  return local_advertising_options.auto_upgrade_bandwidth;
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

  return WaitForResult(absl::StrCat("AcceptConnection(", endpoint_id, ")"),
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

  return WaitForResult(absl::StrCat("RejectConnection(", endpoint_id, ")"),
                       client->GetClientId(), &response);
}

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

    // For backward compatible, here still check both status and
    // response parameters until the response feature is roll out in all
    // supported devices.
    bool accepted = false;
    if (connection_response.has_response()) {
      accepted =
          connection_response.response() == ConnectionResponseFrame::ACCEPT;
    } else {
      accepted = connection_response.status() == Status::kSuccess;
    }
    if (accepted) {
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
                                          CountDownLatch barrier) {
  if (stop_.Get()) {
    barrier.CountDown();
    return;
  }
  RunOnPcpHandlerThread([this, client, endpoint_id, barrier]() mutable {
    auto item = pending_alarms_.find(endpoint_id);
    if (item != pending_alarms_.end()) {
      auto& alarm = item->second;
      alarm.Cancel();
      pending_alarms_.erase(item);
    }
    ProcessPreConnectionResultFailure(client, endpoint_id);
    barrier.CountDown();
  });
}

BluetoothDevice BasePcpHandler::GetRemoteBluetoothDevice(
    const std::string& remote_bluetooth_mac_address) {
  return mediums_->GetBluetoothClassic().GetRemoteDevice(
      remote_bluetooth_mac_address);
}

void BasePcpHandler::OnEndpointFound(
    ClientProxy* client, std::shared_ptr<DiscoveredEndpoint> endpoint) {
  // Check if we've seen this endpoint ID before.
  std::string& endpoint_id = endpoint->endpoint_id;
  NEARBY_LOG(INFO, "OnEndpointFound: id='%s' [enter]", endpoint_id.c_str());

  auto range = discovered_endpoints_.equal_range(endpoint->endpoint_id);

  DiscoveredEndpoint* owned_endpoint = nullptr;
  for (auto& item = range.first; item != range.second; ++item) {
    auto& discovered_endpoint = item->second;
    if (discovered_endpoint->medium != endpoint->medium) continue;
    // Check if there was a info change. If there was, report the previous
    // endpoint as lost.
    if (discovered_endpoint->endpoint_info != endpoint->endpoint_info) {
      OnEndpointLost(client, *discovered_endpoint);
      discovered_endpoint = endpoint;  // Replace endpoint.
      OnEndpointFound(client, std::move(endpoint));
      return;
    } else {
      owned_endpoint = endpoint.get();
      break;
    }
  }

  if (!owned_endpoint) {
    owned_endpoint =
        discovered_endpoints_.emplace(endpoint_id, std::move(endpoint))
            ->second.get();
  }

  // Range is empty: this is the first endpoint we discovered so far.
  // Report this endpoint_id to client.
  if (range.first == range.second) {
    NEARBY_LOG(INFO, "Adding new endpoint: id=%s", endpoint_id.c_str());
    // And, as it's the first time, report it to the client.
    client->OnEndpointFound(
        owned_endpoint->service_id, owned_endpoint->endpoint_id,
        owned_endpoint->endpoint_info, owned_endpoint->medium);
  } else {
    NEARBY_LOGS(INFO) << "Adding new medium for endpoint: id=" << endpoint_id
                      << "; medium=" << owned_endpoint->medium;
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

  // Validate that the cached endpoint has the same info as the one reported as
  // onLost. If the info differs, then no-op. This likely means that the remote
  // device changed their info. We reported onFound for the new info and are
  // just now figuring out that we lost the old info.
  if (discovered_endpoint->endpoint_info != endpoint.endpoint_info) {
    NEARBY_LOG(INFO, "Previous endpoint name mismatch; passed=%s; expected=%s",
               absl::BytesToHexString(endpoint.endpoint_info.data()).c_str(),
               absl::BytesToHexString(discovered_endpoint->endpoint_info.data())
                   .c_str());
    return;
  }

  auto item = discovered_endpoints_.extract(endpoint.endpoint_id);
  if (!discovered_endpoints_.count(endpoint.endpoint_id)) {
    client->OnEndpointLost(endpoint.service_id, endpoint.endpoint_id);
  }
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
  std::string medium_string;
  for (const auto& medium : mediums) {
    absl::StrAppend(&medium_string, medium, "; ");
  }
  NEARBY_LOG(FATAL,
             "Failed to determine preferred medium; bailing out; mediums=%s; "
             "new=%d; old=%d",
             medium_string.c_str(), new_endpoint.medium, old_endpoint.medium);
  return false;
}

Exception BasePcpHandler::OnIncomingConnection(
    ClientProxy* client, const ByteArray& remote_endpoint_info,
    std::unique_ptr<EndpointChannel> channel,
    proto::connections::Medium medium) {
  absl::Time start_time = SystemClock::ElapsedRealtime();

  //  Fixes an NPE in ClientProxy.OnConnectionAccepted. The crash happened when
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
          client->GetClientId(),
          absl::BytesToHexString(remote_endpoint_info.data()).c_str());
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
             client->GetClientId(),
             absl::BytesToHexString(remote_endpoint_info.data()).c_str(),
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
  if (ShouldEnforceTopologyConstraints(client->GetAdvertisingOptions()) &&
      !CanReceiveIncomingConnection(client)) {
    return {Exception::kIo};
  }

  // The ConnectionRequest frame has two fields that both contain the
  // EndpointInfo. The legacy field stores it as a string while the newer field
  // stores it as a byte array. We'll attempt to grab from the newer field, but
  // will accept the older string if it's all that exists.
  const ByteArray endpoint_info{connection_request.has_endpoint_info()
                                    ? connection_request.endpoint_info()
                                    : connection_request.endpoint_name()};

  // We've successfully connected to the device, and are now about to jump on to
  // the EncryptionRunner thread to start running our encryption protocol. We'll
  // mark ourselves as pending in case we get another call to RequestConnection
  // or OnIncomingConnection, so that we can cancel the connection if needed.
  auto* owned_channel =
      pending_connections_
          .emplace(connection_request.endpoint_id(),
                   PendingConnectionInfo{
                       .client = client,
                       .remote_endpoint_info = endpoint_info,
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
                                        info->result.lock().get());
  info->result.reset();
  ProcessPreConnectionResultFailure(client, endpoint_id);
}

void BasePcpHandler::InitiateBandwidthUpgrade(
    ClientProxy* client, const std::string& endpoint_id,
    const std::vector<Medium>& their_supported_mediums) {
  // There was comments for re-using the same highest medium for upgrading. But
  // given that end users may change data options all the time, it makes more
  // sense to dynamically select the proper medium for upgrading.
  // TODO(hais): when we add more mediums like Wifi Hotspot, we need to prevent
  // upgrading interfering with active connections.
  Medium bwu_medium = ChooseBestUpgradeMedium(their_supported_mediums,
                                              client->GetAdvertisingOptions());

  if (AutoUpgradeBandwidth(client->GetAdvertisingOptions()) &&
      bwu_medium != Medium::UNKNOWN_MEDIUM) {
    bwu_manager_->InitiateBwuForEndpoint(client, endpoint_id, bwu_medium);
  }
}

proto::connections::Medium BasePcpHandler::ChooseBestUpgradeMedium(
    const std::vector<proto::connections::Medium>& their_supported_mediums,
    const ConnectionOptions& local_advertising_options) {
  // If the remote side did not report their supported mediums, choose an
  // appropriate default.
  std::vector<proto::connections::Medium> their_mediums =
      their_supported_mediums;
  if (their_supported_mediums.empty()) {
    their_mediums.push_back(GetDefaultUpgradeMedium());
  }

  // Otherwise, pick the best medium we support.
  std::vector<proto::connections::Medium> my_mediums =
      GetSupportedConnectionMediumsByPriority(local_advertising_options);
  for (const auto& my_medium : my_mediums) {
    for (const auto& their_medium : their_mediums) {
      if (my_medium == their_medium) {
        return my_medium;
      }
    }
  }

  return proto::connections::Medium::UNKNOWN_MEDIUM;
}

bool BasePcpHandler::AppendRemoteBluetoothMacAddressEndpoint(
    const std::string& endpoint_id,
    const std::string& remote_bluetooth_mac_address,
    const ConnectionOptions& local_discovery_options) {
  if (!local_discovery_options.allowed.bluetooth) {
    return false;
  }

  auto it = discovered_endpoints_.equal_range(endpoint_id);
  if (it.first == it.second) {
    return false;
  }
  auto endpoint = it.first->second.get();
  for (auto item = it.first; item != it.second; item++) {
    if (item->second->medium == proto::connections::Medium::BLUETOOTH) {
      NEARBY_LOGS(INFO)
          << "Cannot append remote Bluetooth MAC Address endpoint, because the "
             "endpoint has already been found over Bluetooth "
          << "[" << remote_bluetooth_mac_address << "]";
      return false;
    }
  }

  auto remote_bluetooth_device =
      GetRemoteBluetoothDevice(remote_bluetooth_mac_address);
  if (!remote_bluetooth_device.IsValid()) {
    NEARBY_LOGS(INFO) << "Cannot append remote Bluetooth MAC Address endpoint, "
                         "because a valid "
                         "Bluetooth device could not be derived "
                      << "[" << remote_bluetooth_mac_address << "]";
    return false;
  }

  auto bluetooth_endpoint =
      std::make_shared<BluetoothEndpoint>(BluetoothEndpoint{
          {endpoint_id, endpoint->endpoint_info, endpoint->service_id,
           proto::connections::Medium::BLUETOOTH, WebRtcState::kUnconnectable},
          remote_bluetooth_device,
      });

  discovered_endpoints_.emplace(endpoint_id, std::move(bluetooth_endpoint));
  return true;
}

bool BasePcpHandler::AppendWebRTCEndpoint(
    const std::string& endpoint_id,
    const ConnectionOptions& local_discovery_options) {
  if (!local_discovery_options.allowed.web_rtc) {
    return false;
  }

  bool should_connect_web_rtc = false;
  auto it = discovered_endpoints_.equal_range(endpoint_id);
  if (it.first == it.second) return false;
  auto endpoint = it.first->second.get();
  for (auto item = it.first; item != it.second; item++) {
    if (item->second->web_rtc_state != WebRtcState::kUnconnectable) {
      should_connect_web_rtc = true;
      break;
    }
  }
  if (!should_connect_web_rtc) return false;

  auto webrtc_endpoint = std::make_shared<WebRtcEndpoint>(WebRtcEndpoint{
      {endpoint_id, endpoint->endpoint_info, endpoint->service_id,
       proto::connections::Medium::WEB_RTC, WebRtcState::kConnectable},
      CreatePeerIdFromAdvertisement(endpoint->service_id, endpoint->endpoint_id,
                                    endpoint->endpoint_info),
  });

  discovered_endpoints_.emplace(endpoint_id, std::move(webrtc_endpoint));
  return true;
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
  auto future_status = result.lock();
  if (future_status && !future_status->IsSet()) {
    NEARBY_LOG(INFO, "Future was not set; destroying info");
    future_status->Set({Status::kError});
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
    const ByteArray& endpoint_info) {
  std::string seed =
      absl::StrCat(service_id, endpoint_id, std::string(endpoint_info));
  return mediums::PeerId::FromSeed(ByteArray(std::move(seed)));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
