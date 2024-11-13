// Copyright 2022 Google LLC
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

#ifndef CORE_INTERNAL_WIFI_HOTSPOT_BWU_HANDLER_H_
#define CORE_INTERNAL_WIFI_HOTSPOT_BWU_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/mediums/wifi_hotspot.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/wifi_hotspot.h"

namespace nearby {
namespace connections {

// Defines the set of methods that need to be implemented to handle the
// per-Medium-specific operations needed to upgrade an EndpointChannel.
class WifiHotspotBwuHandler : public BaseBwuHandler {
 public:
  explicit WifiHotspotBwuHandler(
      Mediums& mediums,
      IncomingConnectionCallback incoming_connection_callback);

 private:
  class WifiHotspotIncomingSocket : public BwuHandler::IncomingSocket {
   public:
    explicit WifiHotspotIncomingSocket(const std::string& name,
                                       WifiHotspotSocket socket)
        : name_(name), socket_(socket) {}

    std::string ToString() override { return name_; }
    void Close() override { socket_.Close(); }

   private:
    std::string name_;
    WifiHotspotSocket socket_;
  };

  // BwuHandler implementation:
  std::pair<std::unique_ptr<EndpointChannel>,
            location::nearby::proto::connections::OperationResultCode>
  CreateUpgradedEndpointChannel(ClientProxy* client,
                                const std::string& service_id,
                                const std::string& endpoint_id,
                                const UpgradePathInfo& upgrade_path_info) final;
  location::nearby::proto::connections::Medium GetUpgradeMedium() const final {
    return location::nearby::proto::connections::Medium::WIFI_HOTSPOT;
  }
  void OnEndpointDisconnect(ClientProxy* client,
                            const std::string& endpoint_id) final {}

  // BaseBwuHandler implementation:
  ByteArray HandleInitializeUpgradedMediumForEndpoint(
      ClientProxy* client, const std::string& upgrade_service_id,
      const std::string& endpoint_id) final;
  void HandleRevertInitiatorStateForService(
      const std::string& upgrade_service_id) final;

  void OnIncomingWifiHotspotConnection(ClientProxy* client,
                                       const std::string& upgrade_service_id,
                                       WifiHotspotSocket socket);

  Mediums& mediums_;
  WifiHotspot& wifi_hotspot_medium_{mediums_.GetWifiHotspot()};
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_WIFI_HOTSPOT_BWU_HANDLER_H_
