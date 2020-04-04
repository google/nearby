#ifndef PLATFORM_API2_WEBRTC_H_
#define PLATFORM_API2_WEBRTC_H_

#include <vector>

#include "platform/byte_array.h"
#include "webrtc/files/stable/webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {

class WebRtcSignalingMessenger {
 public:
  virtual ~WebRtcSignalingMessenger() = default;

  /** Called whenever we receive an inbox message from tachyon. */
  class SignalingMessageListener {
   public:
    virtual ~SignalingMessageListener() = default;

    virtual void OnSignalingMessage(const ByteArray& message) = 0;
  };

  class IceServersListener {
   public:
    virtual ~IceServersListener() = default;

    virtual void OnIceServersFetched(
        std::vector<webrtc::PeerConnectionInterface::IceServer>
            ice_servers) = 0;
  };

  virtual bool RegisterSignaling() = 0;
  virtual bool UnregisterSignaling() = 0;
  virtual bool SendMessage(std::string_view peer_id,
                           const ByteArray& message) = 0;
  virtual bool StartReceivingMessages(
      const SignalingMessageListener& listener) = 0;
  virtual void GetIceServers(
      const IceServersListener& ice_servers_listener) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_WEBRTC_H_
