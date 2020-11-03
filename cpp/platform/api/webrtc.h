#ifndef PLATFORM_API_WEBRTC_H_
#define PLATFORM_API_WEBRTC_H_

#include <memory>

#include "proto/connections/offline_wire_formats.pb.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/base/byte_array.h"
#include "absl/strings/string_view.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace api {

class WebRtcSignalingMessenger {
 public:
  using OnSignalingMessageCallback = std::function<void(const ByteArray&)>;

  virtual ~WebRtcSignalingMessenger() = default;

  virtual bool SendMessage(absl::string_view peer_id,
                           const ByteArray& message) = 0;

  virtual bool StartReceivingMessages(OnSignalingMessageCallback listener) = 0;
  virtual void StopReceivingMessages() = 0;
};

class WebRtcMedium {
 public:
  using PeerConnectionCallback =
      std::function<void(rtc::scoped_refptr<webrtc::PeerConnectionInterface>)>;

  virtual ~WebRtcMedium() = default;

  // Gets the default two-letter country code associated with current locale.
  // For example, en_US locale resolves to "US".
  virtual const std::string GetDefaultCountryCode() = 0;

  // Creates and returns a new webrtc::PeerConnectionInterface object via
  // |callback|.
  virtual void CreatePeerConnection(webrtc::PeerConnectionObserver* observer,
                                    PeerConnectionCallback callback) = 0;

  // Returns a signaling messenger for sending WebRTC signaling messages.
  virtual std::unique_ptr<WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id,
      const connections::LocationHint& location_hint) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_WEBRTC_H_
