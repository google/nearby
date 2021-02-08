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

#ifndef CORE_INTERNAL_STOPPABLE_SERVICE_CONTROLLER_H_
#define CORE_INTERNAL_STOPPABLE_SERVICE_CONTROLLER_H_

#include <memory>

#include "core/internal/service_controller.h"
#include "core/status.h"
#include "platform/public/atomic_boolean.h"

namespace location {
namespace nearby {
namespace connections {

// A ServiceController proxy that can be shut down.
// When shut down, the API calls are not forwarded to the real controller.
// StoppableServiceController takes over ownership of ServiceController.
class StoppableServiceController : public ServiceController {
 public:
  explicit StoppableServiceController(ServiceController* controller)
      : service_controller_{controller} {}
  ~StoppableServiceController() override = default;

  void Shutdown() { stopped_.Set(true); }

  Status StartAdvertising(ClientProxy* client, const std::string& service_id,
                          const ConnectionOptions& options,
                          const ConnectionRequestInfo& info) override {
    if (stopped_) {
      return {Status::kError};
    }
    return service_controller_->StartAdvertising(client, service_id, options,
                                                 info);
  }

  void StopAdvertising(ClientProxy* client) override {
    if (stopped_) {
      return;
    }
    service_controller_->StopAdvertising(client);
  }

  Status StartDiscovery(ClientProxy* client, const std::string& service_id,
                        const ConnectionOptions& options,
                        const DiscoveryListener& listener) override {
    if (stopped_) {
      return {Status::kError};
    }
    return service_controller_->StartDiscovery(client, service_id, options,
                                               listener);
  }
  void StopDiscovery(ClientProxy* client) override {
    if (stopped_) {
      return;
    }
    service_controller_->StopDiscovery(client);
  }

  void InjectEndpoint(ClientProxy* client, const std::string& service_id,
                      const OutOfBandConnectionMetadata& metadata) override {
    if (stopped_) {
      return;
    }
    service_controller_->InjectEndpoint(client, service_id, metadata);
  }

  Status RequestConnection(ClientProxy* client, const std::string& endpoint_id,
                           const ConnectionRequestInfo& info,
                           const ConnectionOptions& options) override {
    if (stopped_) {
      return {Status::kError};
    }
    return service_controller_->RequestConnection(client, endpoint_id, info,
                                                  options);
  }
  Status AcceptConnection(ClientProxy* client, const std::string& endpoint_id,
                          const PayloadListener& listener) override {
    if (stopped_) {
      return {Status::kError};
    }
    return service_controller_->AcceptConnection(client, endpoint_id, listener);
  }
  Status RejectConnection(ClientProxy* client,
                          const std::string& endpoint_id) override {
    if (stopped_) {
      return {Status::kError};
    }
    return service_controller_->RejectConnection(client, endpoint_id);
  }

  void InitiateBandwidthUpgrade(ClientProxy* client,
                                const std::string& endpoint_id) override {
    if (stopped_) {
      return;
    }
    service_controller_->InitiateBandwidthUpgrade(client, endpoint_id);
  }

  void SendPayload(ClientProxy* client,
                   const std::vector<std::string>& endpoint_ids,
                   Payload payload) override {
    if (stopped_) {
      return;
    }
    service_controller_->SendPayload(client, endpoint_ids, std::move(payload));
  }

  Status CancelPayload(ClientProxy* client, Payload::Id payload_id) override {
    if (stopped_) {
      return {Status::kError};
    }
    return service_controller_->CancelPayload(client, payload_id);
  }

  void DisconnectFromEndpoint(ClientProxy* client,
                              const std::string& endpoint_id) override {
    if (stopped_) {
      return;
    }
    service_controller_->DisconnectFromEndpoint(client, endpoint_id);
  }

 private:
  std::unique_ptr<ServiceController> service_controller_;
  AtomicBoolean stopped_{false};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_STOPPABLE_SERVICE_CONTROLLER_H_
