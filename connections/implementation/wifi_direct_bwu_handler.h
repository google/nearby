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

#ifndef CORE_INTERNAL_WIFI_DIRECT_BWU_HANDLER_H_
#define CORE_INTERNAL_WIFI_DIRECT_BWU_HANDLER_H_

#include <memory>
#include <string>

#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/mediums/mediums.h"

namespace nearby {
namespace connections {

// Defines the set of methods that need to be implemented to handle the
// per-Medium-specific operations needed to upgrade an EndpointChannel.
class WifiDirectBwuHandler : public BaseBwuHandler {
 public:
  explicit WifiDirectBwuHandler(Mediums& mediums,
                                BwuNotifications notifications);

 private:
  class WifiDirectIncomingSocket : public BwuHandler::IncomingSocket {
   public:
    explicit WifiDirectIncomingSocket(const std::string& name,
                                   WifiDirectSocket socket)
        : name_(name), socket_(socket) {}

    std::string ToString() override { return name_; }
    void Close() override { socket_.Close(); }

   private:
    std::string name_;
    WifiDirectSocket socket_;
  };

  // Called by BWU target. Retrieves a new medium info from incoming message,
  // Windows doesn't support WIFIDirect as GC because phone side uses simplified
  // WFD protocol to established connection while WINRT follow the standard WFD
  // spec to achieve the connection. So return fail to stop the upgrade request
  // from phone side.
  std::unique_ptr<EndpointChannel> CreateUpgradedEndpointChannel(
      ClientProxy* client, const std::string& service_id,
      const std::string& endpoint_id,
      const UpgradePathInfo& upgrade_path_info) final;
  Medium GetUpgradeMedium() const final { return Medium::WIFI_DIRECT; }
  void OnEndpointDisconnect(ClientProxy* client,
                            const std::string& endpoint_id) final {}

  // Called by BWU initiator. Set up WifiDirect upgraded medium for this
  // endpoint, and returns a upgrade path info (SSID, Password, Gateway used as
  // IPAddress, Port) for remote party to perform connection.
  ByteArray HandleInitializeUpgradedMediumForEndpoint(
      ClientProxy* client, const std::string& upgrade_service_id,
      const std::string& endpoint_id) final;

  // Revert the upgrade when the procedure fails or disconnection is called.
  void HandleRevertInitiatorStateForService(
      const std::string& upgrade_service_id) final;

  // Accept Connection Callback.
  void OnIncomingWifiDirectConnection(ClientProxy* client,
                                   const std::string& upgrade_service_id,
                                   WifiDirectSocket socket);

  Mediums& mediums_;
  Wifi& wifi_medium_ = mediums_.GetWifi();
  WifiDirect& wifi_direct_medium_ = mediums_.GetWifiDirect();
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_WIFI_DIRECT_BWU_HANDLER_H_
