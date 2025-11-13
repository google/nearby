// Copyright 2025 Google LLC
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

#ifndef CORE_INTERNAL_AWDL_BWU_HANDLER_H_
#define CORE_INTERNAL_AWDL_BWU_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/awdl.h"
#include "connections/implementation/mediums/mediums.h"
#include "internal/platform/awdl.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/expected.h"
#include "internal/platform/nsd_service_info.h"

namespace nearby {
namespace connections {

// Defines the set of methods that need to be implemented to handle the
// per-Medium-specific operations needed to upgrade an EndpointChannel.
class AwdlBwuHandler : public BaseBwuHandler {
 public:
  explicit AwdlBwuHandler(
      Mediums& mediums,
      IncomingConnectionCallback incoming_connection_callback);

 private:
  class AwdlIncomingSocket : public BwuHandler::IncomingSocket {
   public:
    explicit AwdlIncomingSocket(const std::string& name, AwdlSocket socket)
        : name_(name), socket_(socket) {}

    std::string ToString() override { return name_; }
    void Close() override { socket_.Close(); }

   private:
    std::string name_;
    AwdlSocket socket_;
  };

  // BwuHandler implementation:
  ErrorOr<std::unique_ptr<EndpointChannel>> CreateUpgradedEndpointChannel(
      ClientProxy* client, const std::string& service_id,
      const std::string& endpoint_id,
      const location::nearby::connections::BandwidthUpgradeNegotiationFrame::
          UpgradePathInfo& upgrade_path_info) override;
  location::nearby::proto::connections::Medium GetUpgradeMedium() const final {
    return location::nearby::proto::connections::Medium::AWDL;
  }
  void OnEndpointDisconnect(ClientProxy* client,
                            const std::string& endpoint_id) final {}

  // BaseBwuHandler implementation:
  ByteArray HandleInitializeUpgradedMediumForEndpoint(
      ClientProxy* client, const std::string& upgrade_service_id,
      const std::string& endpoint_id) final;
  void HandleRevertInitiatorStateForService(
      const std::string& upgrade_service_id) final;

  void OnIncomingAwdlConnection(ClientProxy* client,
                                const std::string& upgrade_service_id,
                                AwdlSocket socket);

  std::string GenerateServiceType(const std::string& service_id);
  std::string GenerateServiceName();
  std::string GeneratePassword();

  Mediums& mediums_;
  Awdl& awdl_medium_{mediums_.GetAwdl()};
  NsdServiceInfo nsd_service_info_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_AWDL_BWU_HANDLER_H_
