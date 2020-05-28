#include "core/internal/wifi_lan_upgrade_handler.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

namespace wifi_lan_upgrade_handler {

template <typename Platform>
class OnIncomingWifiConnectionRunnable : public Runnable {
 public:
  void run() {}
};

}  // namespace wifi_lan_upgrade_handler

template <typename Platform>
WifiLanUpgradeHandler<Platform>::WifiLanUpgradeHandler(
    Ptr<MediumManager<Platform> > medium_manager,
    Ptr<EndpointChannelManager> endpoint_channel_manager)
    : BaseBandwidthUpgradeHandler(endpoint_channel_manager),
      medium_manager_(medium_manager) {}

template <typename Platform>
WifiLanUpgradeHandler<Platform>::~WifiLanUpgradeHandler() {}

template <typename Platform>
proto::connections::Medium WifiLanUpgradeHandler<Platform>::getUpgradeMedium() {
  return proto::connections::Medium::WIFI_LAN;
}

template <typename Platform>
void WifiLanUpgradeHandler<Platform>::revertImpl() {}

template <typename Platform>
void WifiLanUpgradeHandler<Platform>::onIncomingWifiConnection(
    Ptr<Socket> socket) {}

// TODO(ahlee): This will differ from the Java code (previously threw an
// UpgradeException). Leaving the return type simple for the skeleton - I'll
// switch to a pair if the result enum is needed.
template <typename Platform>
ConstPtr<ByteArray>
WifiLanUpgradeHandler<Platform>::initializeUpgradedMediumForEndpoint(
    const string& endpoint_id) {
  return ConstPtr<ByteArray>();
}

// TODO(ahlee): This will differ from the Java code (previously threw an
// exception).
template <typename Platform>
Ptr<EndpointChannel>
WifiLanUpgradeHandler<Platform>::createUpgradedEndpointChannel(
    const string& endpoint_id,
    ConstPtr<BandwidthUpgradeNegotiationFrame::UpgradePathInfo>
        upgrade_path_info) {
  return Ptr<EndpointChannel>();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
