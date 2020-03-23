#include "core/internal/bandwidth_upgrade_manager.h"

#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
BandwidthUpgradeManager<Platform>::BandwidthUpgradeManager(
    Ptr<MediumManager<Platform> > medium_manager,
    Ptr<EndpointChannelManager<Platform> > endpoint_channel_manager,
    Ptr<EndpointManager<Platform> > endpoint_manager)
    : endpoint_manager_(endpoint_manager),
      bandwidth_upgrade_handlers_(),
      current_bandwidth_upgrade_handler_() {}

template <typename Platform>
BandwidthUpgradeManager<Platform>::~BandwidthUpgradeManager() {
  // TODO(ahlee): Make sure we don't repeat the mistake fixed in cl/201883908.
}

template <typename Platform>
void BandwidthUpgradeManager<Platform>::initiateBandwidthUpgradeForEndpoint(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    proto::connections::Medium medium) {}

template <typename Platform>
void BandwidthUpgradeManager<Platform>::processIncomingOfflineFrame(
    ConstPtr<OfflineFrame> offline_frame, const string& from_endpoint_id,
    Ptr<ClientProxy<Platform> > to_client_proxy,
    proto::connections::Medium current_medium) {}

template <typename Platform>
void BandwidthUpgradeManager<Platform>::processEndpointDisconnection(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    Ptr<CountDownLatch> process_disconnection_barrier) {}

template <typename Platform>
bool BandwidthUpgradeManager<Platform>::setCurrentBandwidthUpgradeHandler(
    proto::connections::Medium medium) {
  return false;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
