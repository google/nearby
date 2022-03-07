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

#ifndef CORE_INTERNAL_WIFI_HOTSPOT_BWU_HANDLER_H_
#define CORE_INTERNAL_WIFI_HOTSPOT_BWU_HANDLER_H_

#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/mediums/mediums.h"

namespace location {
namespace nearby {
namespace connections {

// Defines the set of methods that need to be implemented to handle the
// per-Medium-specific operations needed to upgrade an EndpointChannel.
class WifiHotspotBwuHandler : public BaseBwuHandler {
 public:
  WifiHotspotBwuHandler(Mediums& mediums,
                        EndpointChannelManager& channel_manager,
                        BwuNotifications notifications);
  ~WifiHotspotBwuHandler() override = default;

 private:
  ByteArray InitializeUpgradedMediumForEndpoint(
      ClientProxy* client, const std::string& service_id,
      const std::string& endpoint_id) override;

  void Revert() override;

  std::unique_ptr<EndpointChannel> CreateUpgradedEndpointChannel(
      ClientProxy* client, const std::string& service_id,
      const std::string& endpoint_id,
      const UpgradePathInfo& upgrade_path_info) override;

  Medium GetUpgradeMedium() const override { return Medium::WIFI_HOTSPOT; }

  void OnEndpointDisconnect(ClientProxy* client,
                            const std::string& endpoint_id) override {}

  void OnIncomingWifiHotspotConnection(ClientProxy* client,
                                       const std::string& service_id,
                                       WifiHotspotSocket socket);

  class WifiHotspotIncomingSocket : public BwuHandler::IncomingSocket {
   public:
    explicit WifiHotspotIncomingSocket(const std::string& name,
                                       WifiHotspotSocket socket)
        : name_(name), socket_(socket) {}
    ~WifiHotspotIncomingSocket() override = default;

    std::string ToString() override { return name_; }
    void Close() override { socket_.Close(); }

   private:
    std::string name_;
    WifiHotspotSocket socket_;
  };

  Mediums& mediums_;
  WifiHotspot& wifi_hotspot_medium_{mediums_.GetWifiHotspot()};
  absl::flat_hash_set<std::string> active_service_ids_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_WIFI_HOTSPOT_BWU_HANDLER_H_
