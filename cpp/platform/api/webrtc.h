#ifndef PLATFORM_API_WEBRTC_H_
#define PLATFORM_API_WEBRTC_H_

#include <vector>

#include "platform/byte_array.h"
#include "platform/ptr.h"
//#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
#if 0
class WebRtcSignalingMessenger {
 public:
  virtual ~WebRtcSignalingMessenger() = default;

  /** Called whenever we receive an inbox message from tachyon. */
  class SignalingMessageListener {
   public:
    virtual ~SignalingMessageListener() = default;

    virtual void onSignalingMessage(ConstPtr<ByteArray> message) = 0;
  };

  class IceServersListener {
   public:
    virtual ~IceServersListener() = default;

    virtual void OnIceServersFetched(
        std::vector<Ptr<webrtc::PeerConnectionInterface::IceServer>>
            ice_servers) = 0;
  };

  virtual bool registerSignaling() = 0;
  virtual bool unregisterSignaling() = 0;
  virtual bool sendMessage(const std::string& peer_id,
                           ConstPtr<ByteArray> message) = 0;
  virtual bool startReceivingMessages(
      Ptr<SignalingMessageListener> listener) = 0;
  virtual void getIceServers(Ptr<IceServersListener> ice_servers_listener) = 0;
};
#endif

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_WEBRTC_H_
