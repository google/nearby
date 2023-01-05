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

#ifndef CORE_INTERNAL_WEBRTC_BWU_HANDLER_H_
#define CORE_INTERNAL_WEBRTC_BWU_HANDLER_H_

#include <string>

#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/mediums/mediums.h"
#ifdef NO_WEBRTC
#include "connections/implementation/mediums/webrtc_socket_stub.h"
#else
#include "connections/implementation/mediums/webrtc_socket.h"
#endif

namespace nearby {
namespace connections {

// Defines the set of methods that need to be implemented to handle the
// per-Medium-specific operations needed to upgrade an EndpointChannel.
class WebrtcBwuHandler : public BaseBwuHandler {
 public:
  explicit WebrtcBwuHandler(Mediums& mediums, BwuNotifications notifications);

 private:
  class WebrtcIncomingSocket : public BwuHandler::IncomingSocket {
   public:
    explicit WebrtcIncomingSocket(const std::string& name,
                                  mediums::WebRtcSocketWrapper socket);

    std::string ToString() override;
    void Close() override;

   private:
    std::string name_;
    mediums::WebRtcSocketWrapper socket_;
  };

  // BwuHandler implementation:
  std::unique_ptr<EndpointChannel> CreateUpgradedEndpointChannel(
      ClientProxy* client, const std::string& service_id,
      const std::string& endpoint_id,
      const UpgradePathInfo& upgrade_path_info) final;
  Medium GetUpgradeMedium() const final { return Medium::WEB_RTC; }
  void OnEndpointDisconnect(ClientProxy* client,
                            const std::string& endpoint_id) final {}

  // BaseBwuHandler implementation:
  ByteArray HandleInitializeUpgradedMediumForEndpoint(
      ClientProxy* client, const std::string& upgrade_service_id,
      const std::string& endpoint_id) final;
  void HandleRevertInitiatorStateForService(
      const std::string& upgrade_service_id) final;

  void OnIncomingWebrtcConnection(ClientProxy* client,
                                  const std::string& upgrade_service_id,
                                  mediums::WebRtcSocketWrapper socket);

  Mediums& mediums_;
  mediums::WebRtc& webrtc_{mediums_.GetWebRtc()};
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_WEBRTC_BWU_HANDLER_H_
