#ifndef CORE_V2_INTERNAL_OFFLINE_FRAMES_H_
#define CORE_V2_INTERNAL_OFFLINE_FRAMES_H_

#include <cstdint>
#include <vector>

#include "proto/connections/offline_wire_formats.pb.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/exception.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {
namespace parser {

// Serialize/Deserialize Nearby Connections Protocol messages.

// Parses incoming message.
// Returns OfflineFrame if parser was able to understand it, or
// Exception::kInvalidProtocolBuffer, if parser failed.
ExceptionOr<OfflineFrame> FromBytes(const ByteArray& offline_frame_bytes);

// Returns FrameType of a parsed message, or
// V1Frame::UNKNOWN_FRAME_TYPE, if frame contents is not recognized.
V1Frame::FrameType GetFrameType(const OfflineFrame& offline_frame);

// Build ConnectionRequest message.
ByteArray ForConnectionRequest(
    const std::string& endpoint_id, const std::string& endpoint_name,
    std::int32_t nonce, const std::vector<proto::connections::Medium>& mediums);
ByteArray ForConnectionResponse(std::int32_t status);

ByteArray ForDataPayloadTransfer(
    const PayloadTransferFrame::PayloadHeader& header,
    const PayloadTransferFrame::PayloadChunk& chunk);
ByteArray ForControlPayloadTransfer(
    const PayloadTransferFrame::PayloadHeader& header,
    const PayloadTransferFrame::ControlMessage& control);

ByteArray ForBandwidthUpgradeWifiHotspot(
    const std::string& ssid, const std::string& password, std::int32_t port);
ByteArray ForBandwidthUpgradeLastWrite();
ByteArray ForBandwidthUpgradeSafeToClose();
ByteArray ForBandwidthUpgradeIntroduction(const std::string& endpoint_id);

ByteArray ForKeepAlive();

ConnectionRequestFrame::Medium MediumToConnectionRequestMedium(
    proto::connections::Medium medium);
proto::connections::Medium ConnectionRequestMediumToMedium(
    ConnectionRequestFrame::Medium medium);
std::vector<proto::connections::Medium> ConnectionRequestMediumsToMediums(
    const ConnectionRequestFrame& connection_request_frame);

}  // namespace parser
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_OFFLINE_FRAMES_H_
