#ifndef CORE_V2_INTERNAL_OFFLINE_SERVICE_CONTROLLER_H_
#define CORE_V2_INTERNAL_OFFLINE_SERVICE_CONTROLLER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "core_v2/internal/client_proxy.h"
#include "core_v2/internal/endpoint_channel_manager.h"
#include "core_v2/internal/endpoint_manager.h"
#include "core_v2/internal/mediums/mediums.h"
#include "core_v2/internal/payload_manager.h"
#include "core_v2/internal/pcp_manager.h"
#include "core_v2/internal/service_controller.h"
#include "core_v2/listeners.h"
#include "core_v2/options.h"
#include "core_v2/payload.h"
#include "core_v2/status.h"

namespace location {
namespace nearby {
namespace connections {

class OfflineServiceController : public ServiceController {
 public:
  OfflineServiceController() = default;
  ~OfflineServiceController() override;

  Status StartAdvertising(ClientProxy* client,
                          const std::string& service_id,
                          const ConnectionOptions& options,
                          const ConnectionRequestInfo& info) override;
  void StopAdvertising(ClientProxy* client) override;

  Status StartDiscovery(ClientProxy* client,
                        const std::string& service_id,
                        const ConnectionOptions& options,
                        const DiscoveryListener& listener) override;
  void StopDiscovery(ClientProxy* client) override;

  Status RequestConnection(ClientProxy* client,
                           const std::string& endpoint_id,
                           const ConnectionRequestInfo& info,
                           const ConnectionOptions& options) override;
  Status AcceptConnection(ClientProxy* client,
                          const std::string& endpoint_id,
                          const PayloadListener& listener) override;
  Status RejectConnection(ClientProxy* client,
                          const std::string& endpoint_id) override;

  void InitiateBandwidthUpgrade(ClientProxy* client,
                                const std::string& endpoint_id) override;

  void SendPayload(ClientProxy* client,
                   const std::vector<std::string>& endpoint_ids,
                   Payload payload) override;
  Status CancelPayload(ClientProxy* client,
                       Payload::Id payload_id) override;

  void DisconnectFromEndpoint(ClientProxy* client,
                              const std::string& endpoint_id) override;

  void Stop();

 private:
  // Note that the order of declaration of these is crucial, because we depend
  // on the destructors running (strictly) in the reverse order; a deviation
  // from that will lead to crashes at runtime.
  AtomicBoolean stop_{false};
  Mediums mediums_;
  EndpointChannelManager channel_manager_;
  EndpointManager endpoint_manager_{&channel_manager_};
  PayloadManager payload_manager_{endpoint_manager_};
  PcpManager pcp_manager_{mediums_, channel_manager_, endpoint_manager_};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_OFFLINE_SERVICE_CONTROLLER_H_
