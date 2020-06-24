#include "core_v2/internal/wifi_lan_endpoint_channel.h"

#include <string>

#include "platform_v2/public/logging.h"
#include "platform_v2/public/wifi_lan.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

OutputStream* GetOutputStreamOrNull(WifiLanSocket& socket) {
  if (socket.GetRemoteWifiLanService().IsValid())
    return &socket.GetOutputStream();
  return nullptr;
}

InputStream* GetInputStreamOrNull(WifiLanSocket& socket) {
  if (socket.GetRemoteWifiLanService().IsValid())
    return &socket.GetInputStream();
  return nullptr;
}

}  // namespace

WifiLanEndpointChannel::WifiLanEndpointChannel(const std::string& channel_name,
                                               WifiLanSocket socket)
    : BaseEndpointChannel(channel_name, GetInputStreamOrNull(socket),
                          GetOutputStreamOrNull(socket)),
      wifi_lan_socket_(std::move(socket)) {}

proto::connections::Medium WifiLanEndpointChannel::GetMedium() const {
  return proto::connections::Medium::WIFI_LAN;
}

void WifiLanEndpointChannel::CloseImpl() {
  auto status = wifi_lan_socket_.Close();
  if (!status.Ok()) {
    NEARBY_LOG(INFO, "Failed to close WifiLan socket: exception=%d",
               status.value);
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
