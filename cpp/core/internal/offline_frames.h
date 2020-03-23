#ifndef CORE_INTERNAL_OFFLINE_FRAMES_H_
#define CORE_INTERNAL_OFFLINE_FRAMES_H_

#include <cstdint>
#include <vector>

#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "proto/connections_enums.pb.h"

// Detects the right usage.
#include "google/protobuf/message_lite.h"
#define proto_ns google3_proto_compat


namespace location {
namespace nearby {
namespace connections {

class OfflineFrames {
 public:
  static ExceptionOr<ConstPtr<OfflineFrame> > fromBytes(
      ConstPtr<ByteArray>
          offline_frame_bytes);  // throws Exception::INVALID_PROTOCOL_BUFFER

  static V1Frame::FrameType getFrameType(ConstPtr<OfflineFrame> offline_frame);

  static ConstPtr<ByteArray> forConnectionRequest(
      const std::string& endpoint_id, const std::string& endpoint_name,
      std::int32_t nonce,
      const std::vector<proto::connections::Medium>& mediums);
  static ConstPtr<ByteArray> forConnectionResponse(std::int32_t status);

  static ConstPtr<ByteArray> forDataPayloadTransferFrame(
      const PayloadTransferFrame::PayloadHeader& header,
      const PayloadTransferFrame::PayloadChunk& chunk);
  static ConstPtr<ByteArray> forControlPayloadTransferFrame(
      const PayloadTransferFrame::PayloadHeader& header,
      const PayloadTransferFrame::ControlMessage& control);

  static ConstPtr<ByteArray>
  forWifiHotspotUpgradePathAvailableBandwidthUpgradeNegotiationEvent(
      const std::string& ssid, const std::string& password, std::int32_t port);
  static ConstPtr<ByteArray>
  forLastWriteToPriorChannelBandwidthUpgradeNegotiationEvent();
  static ConstPtr<ByteArray>
  forSafeToClosePriorChannelBandwidthUpgradeNegotiationEvent();
  static ConstPtr<ByteArray>
  forClientIntroductionBandwidthUpgradeNegotiationEvent(
      const std::string& endpoint_id);

  static ConstPtr<ByteArray> forKeepAlive();

  static ConnectionRequestFrame::Medium mediumToConnectionRequestMedium(
      proto::connections::Medium medium);
  static proto::connections::Medium connectionRequestMediumToMedium(
      ConnectionRequestFrame::Medium medium);
  static std::vector<proto::connections::Medium>
  connectionRequestMediumsToMediums(
      const ConnectionRequestFrame& connection_request_frame);
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_OFFLINE_FRAMES_H_
