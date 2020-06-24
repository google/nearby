#include "core_v2/internal/webrtc_endpoint_channel.h"

namespace location {
namespace nearby {
namespace connections {

WebRtcEndpointChannel::WebRtcEndpointChannel(
    const std::string& channel_name, mediums::WebRtcSocketWrapper socket)
    : BaseEndpointChannel(channel_name, &socket.GetInputStream(),
                          &socket.GetOutputStream()),
      webrtc_socket_(std::move(socket)) {}

proto::connections::Medium WebRtcEndpointChannel::GetMedium() const {
  return proto::connections::Medium::WEB_RTC;
}

void WebRtcEndpointChannel::CloseImpl() {
  webrtc_socket_.Close();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
