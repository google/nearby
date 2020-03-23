#ifndef CORE_INTERNAL_WIFI_LAN_UPGRADE_HANDLER_H_
#define CORE_INTERNAL_WIFI_LAN_UPGRADE_HANDLER_H_

#include "core/internal/base_bandwidth_upgrade_handler.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/medium_manager.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/api/socket.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

namespace wifi_lan_upgrade_handler {

template <typename>
class OnIncomingWifiConnectionRunnable;

}  // namespace wifi_lan_upgrade_handler

// Manages the WIFI_LAN-specific methods needed to upgrade an EndpointChannel
template <typename Platform>
class WifiLanUpgradeHandler : public BaseBandwidthUpgradeHandler<Platform> {
  // TODO(ahlee): Uncomment when WIFI_LAN plumbing is done.
  // public MediumManager<Platform>::IncomingWifiConnectionProcessor {
 public:
  WifiLanUpgradeHandler(
      Ptr<MediumManager<Platform> > medium_manager_,
      Ptr<EndpointChannelManager<Platform> > endpoint_channel_manager);
  ~WifiLanUpgradeHandler();

  void onIncomingWifiConnection(Ptr<Socket> socket);

 protected:
  // @BandwidthUpgradeHandlerThread
  ConstPtr<ByteArray> initializeUpgradedMediumForEndpoint(
      const string& endpoint_id);
  // @BandwidthUpgradeHandlerThread
  Ptr<EndpointChannel> createUpgradedEndpointChannel(
      const string& endpoint_id,
      ConstPtr<BandwidthUpgradeNegotiationFrame::UpgradePathInfo>
          upgrade_path_info);
  // TODO(ahlee): Change the java counterparts of these methods to private.
  proto::connections::Medium getUpgradeMedium();
  // @BandwidthUpgradeHandlerThread
  void revertImpl();

 private:
  class IncomingWifiLanSocketConnection
      : public BaseBandwidthUpgradeHandler<Platform>::IncomingSocketConnection {
   public:
    IncomingWifiLanSocketConnection(Ptr<Socket> socket)
        : new_endpoint_channel_(Ptr<EndpointChannel>()),
          // TODO(ahlee): Uncomment when plumbing for WIFI_LAN is done.
          // new_endpoint_channel_(getEndpointChannelManager()
          //                     .createOutgoingWifiLanEndpointChannel(socket)),
          wifi_socket_(socket) {}
    // TODO(ahlee): This is only used for logging which is not currently
    // implemented. If we want to match the Java code in the future, we'll need
    // to add toString() to socket.h.
    string socketToString() { return string(); }
    void closeSocket() {
      // Ignore the potential Exception returned by close(), as a counterpart
      // to Java's closeQuietly().
      wifi_socket_->close();
    }
    // TODO(ahlee): Double check that the ownership of this is correct when
    // this is fully implemented.
    Ptr<EndpointChannel> getEndpointChannel() {
      return new_endpoint_channel_.release();
    }

   private:
    ScopedPtr<Ptr<EndpointChannel> > new_endpoint_channel_;
    Ptr<Socket> wifi_socket_;
  };

  template <typename>
  friend class wifi_lan_upgrade_handler::OnIncomingWifiConnectionRunnable;

  Ptr<MediumManager<Platform> > medium_manager_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/wifi_lan_upgrade_handler.cc"

#endif  // CORE_INTERNAL_WIFI_LAN_UPGRADE_HANDLER_H_
