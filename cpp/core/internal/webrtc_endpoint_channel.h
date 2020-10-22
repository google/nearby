#ifndef CORE_INTERNAL_WEBRTC_ENDPOINT_CHANNEL_H_
#define CORE_INTERNAL_WEBRTC_ENDPOINT_CHANNEL_H_

#include "core/internal/base_endpoint_channel.h"
#include "core/internal/mediums/webrtc/webrtc_socket_wrapper.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

class WebRtcEndpointChannel final : public BaseEndpointChannel {
 public:
  WebRtcEndpointChannel(const std::string& channel_name,
                        mediums::WebRtcSocketWrapper webrtc_socket);

  proto::connections::Medium GetMedium() const override;

 private:
  void CloseImpl() override;

  mediums::WebRtcSocketWrapper webrtc_socket_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_WEBRTC_ENDPOINT_CHANNEL_H_
