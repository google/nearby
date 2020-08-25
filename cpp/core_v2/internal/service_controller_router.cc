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

#include "core_v2/internal/service_controller_router.h"

#include <memory>
#include <string>
#include <utility>

#include "core_v2/internal/client_proxy.h"
#include "core_v2/listeners.h"
#include "core_v2/options.h"
#include "core_v2/params.h"
#include "core_v2/payload.h"
#include "platform_v2/public/logging.h"
#include "absl/time/clock.h"

namespace location {
namespace nearby {
namespace connections {

ServiceControllerRouter::~ServiceControllerRouter() {
  NEARBY_LOG(INFO, "ServiceControllerRouter going down.");

  // And make sure that cleanup is the last thing we do.
  serializer_.Shutdown();
}

void ServiceControllerRouter::StartAdvertising(
    ClientProxy* client, absl::string_view service_id,
    const ConnectionOptions& options, const ConnectionRequestInfo& info,
    const ResultCallback& callback) {
  RouteToServiceController([this, client, service_id = std::string(service_id),
                            options, info, callback]() {
    Status status = AcquireServiceControllerForClient(client, options.strategy);
    if (!status.Ok()) {
      callback.result_cb(status);
      return;
    }

    if (client->IsAdvertising()) {
      callback.result_cb({Status::kAlreadyAdvertising});
      return;
    }

    status = service_controller_->StartAdvertising(client, service_id, options,
                                                   info);
    callback.result_cb(status);
  });
}

void ServiceControllerRouter::StopAdvertising(ClientProxy* client,
                                              const ResultCallback& callback) {
  RouteToServiceController([this, client, callback]() {
    if (ClientHasAcquiredServiceController(client) && client->IsAdvertising()) {
      service_controller_->StopAdvertising(client);
    }
    callback.result_cb({Status::kSuccess});
  });
}

void ServiceControllerRouter::StartDiscovery(ClientProxy* client,
                                             absl::string_view service_id,
                                             const ConnectionOptions& options,
                                             const DiscoveryListener& listener,
                                             const ResultCallback& callback) {
  RouteToServiceController([this, client, service_id = std::string(service_id),
                            options, listener, callback]() {
    Status status = AcquireServiceControllerForClient(client, options.strategy);
    if (!status.Ok()) {
      callback.result_cb(status);
      return;
    }

    if (client->IsDiscovering()) {
      callback.result_cb({Status::kAlreadyDiscovering});
      return;
    }

    status = service_controller_->StartDiscovery(client, service_id, options,
                                                 listener);
    callback.result_cb(status);
  });
}

void ServiceControllerRouter::StopDiscovery(ClientProxy* client,
                                            const ResultCallback& callback) {
  RouteToServiceController([this, client, callback]() {
    if (ClientHasAcquiredServiceController(client) && client->IsDiscovering()) {
      service_controller_->StopDiscovery(client);
    }
    callback.result_cb({Status::kSuccess});
  });
}

void ServiceControllerRouter::RequestConnection(
    ClientProxy* client, absl::string_view endpoint_id,
    const ConnectionRequestInfo& info, const ConnectionOptions& options,
    const ResultCallback& callback) {
  RouteToServiceController([this, client,
                            endpoint_id = std::string(endpoint_id), info,
                            options, callback]() {
    if (!ClientHasAcquiredServiceController(client)) {
      callback.result_cb({Status::kOutOfOrderApiCall});
      return;
    }

    if (client->HasPendingConnectionToEndpoint(endpoint_id) ||
        client->IsConnectedToEndpoint(endpoint_id)) {
      callback.result_cb({Status::kAlreadyConnectedToEndpoint});
      return;
    }

    callback.result_cb(service_controller_->RequestConnection(
        client, endpoint_id, info, options));
  });
}

void ServiceControllerRouter::AcceptConnection(ClientProxy* client,
                                               absl::string_view endpoint_id,
                                               const PayloadListener& listener,
                                               const ResultCallback& callback) {
  RouteToServiceController([this, client,
                            endpoint_id = std::string(endpoint_id), listener,
                            callback]() {
    if (!ClientHasAcquiredServiceController(client)) {
      callback.result_cb({Status::kOutOfOrderApiCall});
      return;
    }

    if (client->IsConnectedToEndpoint(endpoint_id)) {
      callback.result_cb({Status::kAlreadyConnectedToEndpoint});
      return;
    }

    if (client->HasLocalEndpointResponded(endpoint_id)) {
      NEARBY_LOG(INFO,
                 "[ServiceControllerRouter:Accept]: Client has local "
                 "endpoint responded; id=%s",
                 endpoint_id.c_str());
      callback.result_cb({Status::kOutOfOrderApiCall});
      return;
    }

    callback.result_cb(
        service_controller_->AcceptConnection(client, endpoint_id, listener));
  });
}

void ServiceControllerRouter::RejectConnection(ClientProxy* client,
                                               absl::string_view endpoint_id,
                                               const ResultCallback& callback) {
  RouteToServiceController(
      [this, client, endpoint_id = std::string(endpoint_id), callback]() {
        if (!ClientHasAcquiredServiceController(client)) {
          callback.result_cb({Status::kOutOfOrderApiCall});
          return;
        }

        if (client->IsConnectedToEndpoint(endpoint_id)) {
          callback.result_cb({Status::kAlreadyConnectedToEndpoint});
          return;
        }

        if (client->HasLocalEndpointResponded(endpoint_id)) {
          NEARBY_LOG(INFO,
                     "[ServiceControllerRouter:Reject]: Client has local "
                     "endpoint responded; id=%s",
                     endpoint_id.c_str());
          callback.result_cb({Status::kOutOfOrderApiCall});
          return;
        }

        callback.result_cb(
            service_controller_->RejectConnection(client, endpoint_id));
      });
}

void ServiceControllerRouter::InitiateBandwidthUpgrade(
    ClientProxy* client, absl::string_view endpoint_id,
    const ResultCallback& callback) {
  RouteToServiceController(
      [this, client, endpoint_id = std::string(endpoint_id), callback]() {
        if (!ClientHasAcquiredServiceController(client) ||
            !client->IsConnectedToEndpoint(endpoint_id)) {
          callback.result_cb({Status::kOutOfOrderApiCall});
          return;
        }

        service_controller_->InitiateBandwidthUpgrade(client, endpoint_id);

        // Operation is triggered; the caller can listen to
        // ConnectionListener::OnBandwidthChanged() to determine its success.
        callback.result_cb({Status::kSuccess});
      });
}

void ServiceControllerRouter::SendPayload(
    ClientProxy* client, absl::Span<const std::string> endpoint_ids,
    Payload payload, const ResultCallback& callback) {
  // Payload is a move-only type.
  // We have to capture it by value inside the lambda, and pass it over to
  // the executor as an std::function<void()> instance.
  // Lambda must be copyable, in order ot satisfy std::function<> requirements.
  // To make it so, we need Payload wrapped by a copyable wrapper.
  // std::shared_ptr<> is used, because it is copyable.
  auto shared_payload = std::make_shared<Payload>(std::move(payload));
  const std::vector<std::string> endpoints =
      std::vector<std::string>(endpoint_ids.begin(), endpoint_ids.end());

  RouteToServiceController(
      [this, client, shared_payload, endpoints, callback]() {
        if (!ClientHasAcquiredServiceController(client)) {
          callback.result_cb({Status::kOutOfOrderApiCall});
          return;
        }

        if (!ClientHasConnectionToAtLeastOneEndpoint(client, endpoints)) {
          callback.result_cb({Status::kEndpointUnknown});
          return;
        }

        service_controller_->SendPayload(client, endpoints,
                                         std::move(*shared_payload));

        // At this point, we've queued up the send Payload request with the
        // ServiceController; any further failures (e.g. one of the endpoints is
        // unknown, goes away, or otherwise fails) will be returned to the
        // client as a PayloadTransferUpdate.
        callback.result_cb({Status::kSuccess});
      });
}

void ServiceControllerRouter::CancelPayload(ClientProxy* client,
                                            std::uint64_t payload_id,
                                            const ResultCallback& callback) {
  RouteToServiceController([this, client, payload_id, callback]() {
    if (!ClientHasAcquiredServiceController(client)) {
      callback.result_cb({Status::kOutOfOrderApiCall});
      return;
    }

    callback.result_cb(service_controller_->CancelPayload(client, payload_id));
  });
}

void ServiceControllerRouter::DisconnectFromEndpoint(
    ClientProxy* client, absl::string_view endpoint_id,
    const ResultCallback& callback) {
  RouteToServiceController(
      [this, client, endpoint_id = std::string(endpoint_id), callback]() {
        if (ClientHasAcquiredServiceController(client)) {
          if (!client->IsConnectedToEndpoint(endpoint_id) &&
              !client->HasPendingConnectionToEndpoint(endpoint_id)) {
            callback.result_cb({Status::kOutOfOrderApiCall});
            return;
          }
          service_controller_->DisconnectFromEndpoint(client, endpoint_id);
          callback.result_cb({Status::kSuccess});
        }
      });
}

void ServiceControllerRouter::StopAllEndpoints(ClientProxy* client,
                                               const ResultCallback& callback) {
  RouteToServiceController([this, client, callback]() {
    if (ClientHasAcquiredServiceController(client)) {
      DoneWithStrategySessionForClient(client);
    }
    callback.result_cb({Status::kSuccess});
  });
}

void ServiceControllerRouter::ClientDisconnecting(
    ClientProxy* client, const ResultCallback& callback) {
  RouteToServiceController([this, client, callback]() {
    if (ClientHasAcquiredServiceController(client)) {
      DoneWithStrategySessionForClient(client);
      NEARBY_LOG(INFO,
                 "[ServiceControllerRouter:Disconnect]: Client has completed "
                 "the client's connection");
    }
    callback.result_cb({Status::kSuccess});
  });
}

Status ServiceControllerRouter::AcquireServiceControllerForClient(
    ClientProxy* client, Strategy strategy) {
  if (current_strategy_.IsNone()) {
    // Case 1: There is no existing Strategy at all.

    // Set everything up for the first time.
    Status status = UpdateCurrentServiceControllerAndStrategy(strategy);
    if (!status.Ok()) {
      return status;
    }
    clients_.insert(client);
    return {Status::kSuccess};
  } else if (strategy == current_strategy_) {
    // Case 2: The existing Strategy matches.

    // The new client just needs to be added to the set of clients using the
    // current ServiceController.
    clients_.insert(client);
    return {Status::kSuccess};
  } else {
    // Case 3: The existing Strategy doesn't match.

    // It's only safe for a client to cause a switch if it's the only client
    // using the current ServiceController.
    bool is_the_only_client_of_service_controller =
        clients_.size() == 1 && ClientHasAcquiredServiceController(client);
    if (!is_the_only_client_of_service_controller) {
      NEARBY_LOG(INFO,
                 "[ServiceControllerRouter:AcquireServiceControllerForClient]: "
                 "Client has already active strategy.");
      return {Status::kAlreadyHaveActiveStrategy};
    }

    // If the client still has connected endpoints, they must disconnect before
    // they can switch.
    if (!client->GetConnectedEndpoints().empty()) {
      NEARBY_LOG(INFO,
                 "[ServiceControllerRouter:AcquireServiceControllerForClient]: "
                 "Client has connected endpoints.");
      return {Status::kOutOfOrderApiCall};
    }

    // By this point, it's safe to switch the Strategy and ServiceController
    // (and since it's the only client, there's no need to add it to the set of
    // clients using the current ServiceController).
    return UpdateCurrentServiceControllerAndStrategy(strategy);
  }
}

bool ServiceControllerRouter::ClientHasAcquiredServiceController(
    ClientProxy* client) const {
  return clients_.contains(client);
}

void ServiceControllerRouter::ReleaseServiceControllerForClient(
    ClientProxy* client) {
  clients_.erase(client);

  if (clients_.empty()) {
    service_controller_.reset();
    current_strategy_ = Strategy{};
  }
}

/** Clean up all state for this client. The client is now free to switch
 * strategies. */
void ServiceControllerRouter::DoneWithStrategySessionForClient(
    ClientProxy* client) {
  // Disconnect from all the connected endpoints tied to this clientProxy.
  for (auto& endpoint_id : client->GetPendingConnectedEndpoints()) {
    service_controller_->DisconnectFromEndpoint(client, endpoint_id);
  }

  for (auto& endpoint_id : client->GetConnectedEndpoints()) {
    service_controller_->DisconnectFromEndpoint(client, endpoint_id);
  }

  // Stop any advertising and discovery that may be underway due to this
  // clientProxy.
  service_controller_->StopAdvertising(client);
  service_controller_->StopDiscovery(client);

  ReleaseServiceControllerForClient(client);
}

void ServiceControllerRouter::RouteToServiceController(Runnable runnable) {
  serializer_.Execute(std::move(runnable));
}

bool ServiceControllerRouter::ClientHasConnectionToAtLeastOneEndpoint(
    ClientProxy* client, const std::vector<std::string>& remote_endpoint_ids) {
  for (auto& endpoint_id : remote_endpoint_ids) {
    if (client->IsConnectedToEndpoint(endpoint_id)) {
      return true;
    }
  }
  return false;
}

Status ServiceControllerRouter::UpdateCurrentServiceControllerAndStrategy(
    Strategy strategy) {
  if (!strategy.IsValid()) {
    NEARBY_LOG(INFO, "Strategy is not valid.");
    return {Status::kError};
  }

  service_controller_.reset(service_controller_factory_());
  current_strategy_ = strategy;

  return {Status::kSuccess};
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
