// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CORE_INTERNAL_OFFLINE_FRAMES_H_
#define CORE_INTERNAL_OFFLINE_FRAMES_H_

#include <cstdint>
#include <string>
#include <vector>

#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/connection_options.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace connections {
namespace parser {

using UpgradePathInfo = ::location::nearby::connections::
    BandwidthUpgradeNegotiationFrame::UpgradePathInfo;

// Serialize/Deserialize Nearby Connections Protocol messages.

// Parses incoming message.
// Returns OfflineFrame if parser was able to understand it, or
// Exception::kInvalidProtocolBuffer, if parser failed.
ExceptionOr<location::nearby::connections::OfflineFrame> FromBytes(
    const ByteArray& offline_frame_bytes);

// Returns FrameType of a parsed message, or
// V1Frame::UNKNOWN_FRAME_TYPE, if frame contents is not recognized.
location::nearby::connections::V1Frame::FrameType GetFrameType(
    const location::nearby::connections::OfflineFrame& offline_frame);

// Builds Connection Request / Response messages.
ByteArray ForConnectionRequestConnections(
    const location::nearby::connections::ConnectionsDevice&
        proto_connections_device,
    const ConnectionInfo& conection_info);
ByteArray ForConnectionRequestPresence(
    const location::nearby::connections::PresenceDevice& proto_presence_device,
    const ConnectionInfo& connection_info);
ByteArray ForConnectionResponse(
    std::int32_t status, const location::nearby::connections::OsInfo& os_info,
    std::int32_t multiplex_socket_bitmask);

// Builds Payload transfer messages.
ByteArray ForDataPayloadTransfer(
    const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
        header,
    const location::nearby::connections::PayloadTransferFrame::PayloadChunk&
        chunk);
ByteArray ForControlPayloadTransfer(
    const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
        header,
    const location::nearby::connections::PayloadTransferFrame::ControlMessage&
        control);
ByteArray ForPayloadAckPayloadTransfer(std::int64_t payload_id);

// Builds Bandwidth Upgrade [BWU] messages.
ByteArray ForBwuIntroduction(const std::string& endpoint_id,
                             bool supports_disabling_encryption);
ByteArray ForBwuIntroductionAck();
ByteArray ForBwuWifiHotspotPathAvailable(const std::string& ssid,
                                         const std::string& password,
                                         std::int32_t port,
                                         std::int32_t frequency,
                                         const std::string& gateway,
                                         bool supports_disabling_encryption);
ByteArray ForBwuWifiLanPathAvailable(const std::string& ip_address,
                                     std::int32_t port);
ByteArray ForBwuWifiAwarePathAvailable(const std::string& service_id,
                                       const std::string& service_info,
                                       const std::string& password,
                                       bool supports_disabling_encryption);
ByteArray ForBwuWifiDirectPathAvailable(const std::string& ssid,
                                        const std::string& password,
                                        std::int32_t port,
                                        std::int32_t frequency,
                                        bool supports_disabling_encryption,
                                        const std::string& gateway);
ByteArray ForBwuBluetoothPathAvailable(const std::string& service_id,
                                       const std::string& mac_address);
ByteArray ForBwuWebrtcPathAvailable(
    const std::string& peer_id,
    const location::nearby::connections::LocationHint& location_hint_a);
ByteArray ForBwuFailure(const UpgradePathInfo& info);
ByteArray ForBwuLastWrite();
ByteArray ForBwuSafeToClose();

ByteArray ForKeepAlive();
ByteArray ForDisconnection(bool request_safe_to_disconnect,
                           bool ack_safe_to_disconnect);
ByteArray ForAutoReconnectIntroduction(const std::string& endpoint_id);
ByteArray ForAutoReconnectIntroductionAck(const std::string& endpoint_id);
UpgradePathInfo::Medium MediumToUpgradePathInfoMedium(Medium medium);
Medium UpgradePathInfoMediumToMedium(UpgradePathInfo::Medium medium);

location::nearby::connections::ConnectionRequestFrame::Medium
MediumToConnectionRequestMedium(Medium medium);
Medium ConnectionRequestMediumToMedium(
    location::nearby::connections::ConnectionRequestFrame::Medium medium);
std::vector<Medium> ConnectionRequestMediumsToMediums(
    const location::nearby::connections::ConnectionRequestFrame&
        connection_request_frame);

}  // namespace parser
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_OFFLINE_FRAMES_H_
