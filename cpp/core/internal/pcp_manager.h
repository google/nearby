#ifndef CORE_INTERNAL_PCP_MANAGER_H_
#define CORE_INTERNAL_PCP_MANAGER_H_

#include <string>

#include "core/internal/base_pcp_handler.h"
#include "core/internal/bwu_manager.h"
#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/mediums/mediums.h"
#include "core/listeners.h"
#include "core/options.h"
#include "core/status.h"
#include "core/strategy.h"
#include "platform/public/atomic_boolean.h"
#include "absl/container/flat_hash_map.h"

namespace location {
namespace nearby {
namespace connections {

// Manages all known PcpHandler implementations, delegating operations to the
// appropriate one as per the parameters passed in.
//
// This will only ever be used by the OfflineServiceController, which has all
// of its entrypoints invoked serially, so there's no synchronization needed.
// Public method semantics matches definition in the
// https://source.corp.google.com/piper///depot/google3/core/internal/service_controller.h
class PcpManager {
 public:
  PcpManager(Mediums& mediums, EndpointChannelManager& channel_manager,
             EndpointManager& endpoint_manager, BwuManager& bwu_manager);
  ~PcpManager();

  Status StartAdvertising(ClientProxy* client, const string& service_id,
                          const ConnectionOptions& options,
                          const ConnectionRequestInfo& info);
  void StopAdvertising(ClientProxy* client);

  Status StartDiscovery(ClientProxy* client, const string& service_id,
                        const ConnectionOptions& options,
                        DiscoveryListener listener);
  void StopDiscovery(ClientProxy* client);

  void InjectEndpoint(ClientProxy* client,
                      const std::string& service_id,
                      const OutOfBandConnectionMetadata& metadata);

  Status RequestConnection(ClientProxy* client, const string& endpoint_id,
                           const ConnectionRequestInfo& info,
                           const ConnectionOptions& options);
  Status AcceptConnection(ClientProxy* client, const string& endpoint_id,
                          const PayloadListener& payload_listener);
  Status RejectConnection(ClientProxy* client, const string& endpoint_id);

  proto::connections::Medium GetBandwidthUpgradeMedium();
  void DisconnectFromEndpointManager();

 private:
  bool SetCurrentPcpHandler(Strategy strategy);
  PcpHandler* GetPcpHandler(Pcp pcp) const;

  AtomicBoolean shutdown_{false};
  absl::flat_hash_map<Pcp, std::unique_ptr<BasePcpHandler>> handlers_;
  PcpHandler* current_ = nullptr;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_PCP_MANAGER_H_
