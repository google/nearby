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

#ifndef CORE_V2_INTERNAL_WEBRTC_BWU_HANDLER_H_
#define CORE_V2_INTERNAL_WEBRTC_BWU_HANDLER_H_

#include "core_v2/internal/base_bwu_handler.h"
#include "core_v2/internal/client_proxy.h"
#include "core_v2/internal/endpoint_channel_manager.h"
#include "core_v2/internal/mediums/mediums.h"
#include "core_v2/internal/mediums/webrtc/webrtc_socket_wrapper.h"

namespace location {
namespace nearby {
namespace connections {

using BwuNegotiationFrame = BandwidthUpgradeNegotiationFrame;

// Defines the set of methods that need to be implemented to handle the
// per-Medium-specific operations needed to upgrade an EndpointChannel.
class WebrtcBwuHandler : public BaseBwuHandler {
 public:
  WebrtcBwuHandler(Mediums& mediums, EndpointChannelManager& channel_manager,
                   BwuNotifications notifications);
  ~WebrtcBwuHandler() override = default;

 private:
  // Called by the Initiator to setup the upgraded medium for this endpoint (if
  // that hasn't already been done), and returns a serialized UpgradePathInfo
  // that can be sent to the Responder.
  // @BwuHandlerThread
  ByteArray InitializeUpgradedMediumForEndpoint(
      ClientProxy* client, const std::string& service_id,
      const std::string& endpoint_id) override;
  // Called to revert any state changed by the Initiator to setup the upgraded
  // medium for an endpoint.
  // @BwuHandlerThread
  void Revert() override;

  // Called by the Responder to setup the upgraded medium for this endpoint (if
  // that hasn't already been done) using the UpgradePathInfo sent by the
  // Initiator, and returns a new EndpointChannel for the upgraded medium.
  // @BwuHandlerThread
  std::unique_ptr<EndpointChannel> CreateUpgradedEndpointChannel(
      ClientProxy* client, const std::string& service_id,
      const std::string& endpoint_id,
      const UpgradePathInfo& upgrade_path_info) override;
  // Returns the upgrade medium of the BwuHandler.
  // @BwuHandlerThread
  Medium GetUpgradeMedium() const override { return Medium::WEB_RTC; }

  void OnIncomingWebrtcConnection(ClientProxy* client,
                                  const std::string& service_id,
                                  mediums::WebRtcSocketWrapper socket);

  void OnEndpointDisconnect(ClientProxy* client,
                            const std::string& endpoint_id) override;

  class WebrtcIncomingSocket : public BwuHandler::IncomingSocket {
   public:
    explicit WebrtcIncomingSocket(const std::string& name,
                                  mediums::WebRtcSocketWrapper socket);
    ~WebrtcIncomingSocket() override = default;

    std::string ToString() override;
    void Close() override;

   private:
    std::string name_;
    mediums::WebRtcSocketWrapper socket_;
  };

  Mediums& mediums_;
  mediums::WebRtc& webrtc_{mediums_.GetWebRtc()};
  absl::flat_hash_set<std::string> active_service_ids_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_WEBRTC_BWU_HANDLER_H_
