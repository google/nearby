#include "core/internal/wifi_lan_endpoint_channel.h"

#include <string>

namespace location {
namespace nearby {
namespace connections {

Ptr<WifiLanEndpointChannel>
WifiLanEndpointChannel::CreateOutgoing(
    Ptr<MediumManager<Platform>> medium_manager,
    absl::string_view channel_name, Ptr<WifiLanSocket> wifi_lan_socket) {
  return MakePtr(
      new WifiLanEndpointChannel(channel_name, wifi_lan_socket));
}

Ptr<WifiLanEndpointChannel>
WifiLanEndpointChannel::CreateIncoming(
    Ptr<MediumManager<Platform>> medium_manager,
    absl::string_view channel_name, Ptr<WifiLanSocket> wifi_lan_socket) {
  return MakePtr(
      new WifiLanEndpointChannel(channel_name, wifi_lan_socket));
}

WifiLanEndpointChannel::WifiLanEndpointChannel(
    absl::string_view channel_name, Ptr<WifiLanSocket> wifi_lan_socket)
    : BaseEndpointChannel(channel_name,
                          wifi_lan_socket->GetInputStream(),
                          wifi_lan_socket->GetOutputStream()),
      wifi_lan_socket_(wifi_lan_socket) {}

WifiLanEndpointChannel::~WifiLanEndpointChannel() {}

proto::connections::Medium WifiLanEndpointChannel::getMedium() {
  return proto::connections::Medium::WIFI_LAN;
}

void WifiLanEndpointChannel::closeImpl() {
  Exception::Value exception = wifi_lan_socket_->Close();
  if (exception != Exception::NONE) {
    if (exception == Exception::IO) {
      // TODO(b/149806065): Add logging.
    }
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
