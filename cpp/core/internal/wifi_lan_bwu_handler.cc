#include "core/internal/wifi_lan_bwu_handler.h"

#include <locale>
#include <string>

#include "core/internal/client_proxy.h"
#include "core/internal/mediums/utils.h"
#include "core/internal/offline_frames.h"
#include "core/internal/wifi_lan_endpoint_channel.h"
#include "platform/public/wifi_lan.h"
#include "absl/functional/bind_front.h"

namespace location {
namespace nearby {
namespace connections {

WifiLanBwuHandler::WifiLanBwuHandler(Mediums& mediums,
                                     EndpointChannelManager& channel_manager,
                                     BwuNotifications notifications)
    : BaseBwuHandler(channel_manager, std::move(notifications)),
      mediums_(mediums) {}

// Called by BWU initiator. Set up WifiLan upgraded medium for this endpoint,
// and returns a upgrade path info (ip address, port) for remote party to
// perform discovery.
ByteArray WifiLanBwuHandler::InitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id) {
  // Use wrapped service ID to avoid have the same ID with the one for
  // startAdvertising. Otherwise, the listening request would be ignored because
  // the medium already start accepting the connection because the client not
  // stop the advertising yet.
  std::string upgrade_service_id = Utils::WrapUpgradeServiceId(service_id);

  if (!wifi_lan_medium_.IsAcceptingConnections(upgrade_service_id)) {
    if (!wifi_lan_medium_.StartAcceptingConnections(
            upgrade_service_id,
            {
                .accepted_cb = absl::bind_front(
                    &WifiLanBwuHandler::OnIncomingWifiLanConnection, this,
                    client),
            })) {
      NEARBY_LOG(ERROR,
                 "WifiLanBwuHandler couldn't initiate the WifiLan upgrade for "
                 "endpoint %s because it failed to start listening for "
                 "incoming WifiLan connections.",
                 endpoint_id.c_str());
      return {};
    }
    NEARBY_LOG(INFO,
               "WifiLanBwuHandler successfully started listening for incoming "
               "WifiLan connections while upgrading endpoint %s",
               endpoint_id.c_str());
  }

  // cache service ID to revert
  active_service_ids_.emplace(upgrade_service_id);

  // TODO(b/169303360): Implements wifiLanCredntials for wif_lan_medium to
  // get ip_address and port.
  std::string ip_addresss;
  std::int32_t port = 0;
  return parser::ForBwuWifiLanPathAvailable(ip_addresss, port);
}

void WifiLanBwuHandler::Revert() {
  for (const std::string& service_id : active_service_ids_) {
    wifi_lan_medium_.StopAcceptingConnections(service_id);
  }
  active_service_ids_.clear();

  NEARBY_LOG(INFO, "WifiLanBwuHandler successfully reverted all states.");
}

// Called by BWU target. Retrieves a new medium info from incoming message,
// and establishes connection over WifiLan using this info.
std::unique_ptr<EndpointChannel>
WifiLanBwuHandler::CreateUpgradedEndpointChannel(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id, const UpgradePathInfo& upgrade_path_info) {
  // TODO(b/169303360): Implements connect WifiLan over ip address and port.
  WifiLanSocket socket;

  // Create a new WifiLanEndpointChannel.
  auto channel = std::make_unique<WifiLanEndpointChannel>(service_id, socket);
  if (channel == nullptr) {
    socket.Close();
    NEARBY_LOG(ERROR,
               "WifiLanBwuHandler failed to create new EndpointChannel for "
               "outgoing socket %p, aborting upgrade.",
               &socket.GetImpl());
  }

  return channel;
}

// Accept Connection Callback.
void WifiLanBwuHandler::OnIncomingWifiLanConnection(
    ClientProxy* client, WifiLanSocket socket,
    const std::string& upgrade_service_id) {
  std::string service_id = Utils::UnwrapUpgradeServiceId(upgrade_service_id);
  auto channel = std::make_unique<WifiLanEndpointChannel>(service_id, socket);
  auto wifi_lan_socket =
      std::make_unique<WifiLanIncomingSocket>(service_id, socket);
  std::unique_ptr<IncomingSocketConnection> connection(
      new IncomingSocketConnection{std::move(wifi_lan_socket),
                                   std::move(channel)});

  bwu_notifications_.incoming_connection_cb(client, std::move(connection));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location

