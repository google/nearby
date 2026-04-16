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

#include "absl/strings/string_view.h"
#include "connections/connection_options.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/medium_selector.h"
#include "internal/platform/exception.h"
#include "internal/platform/mac_address.h"
#include "internal/platform/service_address.h"

namespace nearby {
namespace connections {
namespace parser {

using UpgradePathInfo = ::location::nearby::connections::
    BandwidthUpgradeNegotiationFrame::UpgradePathInfo;
using MediumMetadata = ::location::nearby::connections::MediumMetadata;
using WifiDirectAuthType =
    ::location::nearby::proto::connections::WifiDirectAuthType;

// Serialize/Deserialize Nearby Connections Protocol messages.

// Parses incoming message.
// Returns OfflineFrame if parser was able to understand it, or
// Exception::kInvalidProtocolBuffer, if parser failed.
ExceptionOr<location::nearby::connections::OfflineFrame> FromBytes(
    absl::string_view offline_frame_bytes);

// Returns FrameType of a parsed message, or
// V1Frame::UNKNOWN_FRAME_TYPE, if frame contents is not recognized.
location::nearby::connections::V1Frame::FrameType GetFrameType(
    const location::nearby::connections::OfflineFrame& offline_frame);

// Builds Connection Request / Response messages.
std::string ForConnectionRequestConnections(
    const location::nearby::connections::ConnectionsDevice&
        proto_connections_device,
    const ConnectionInfo& connection_info);
std::string ForConnectionRequestPresence(
    const location::nearby::connections::PresenceDevice& proto_presence_device,
    const ConnectionInfo& connection_info);
std::string ForConnectionResponse(
    std::int32_t status, const location::nearby::connections::OsInfo& os_info,
    std::int32_t multiplex_socket_bitmask);

// Builds Payload transfer messages.
std::string ForDataPayloadTransfer(
    const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
        header,
    const location::nearby::connections::PayloadTransferFrame::PayloadChunk&
        chunk);
std::string ForControlPayloadTransfer(
    const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
        header,
    const location::nearby::connections::PayloadTransferFrame::ControlMessage&
        control);
std::string ForPayloadAckPayloadTransfer(std::int64_t payload_id);

// Builds Bandwidth Upgrade [BWU] messages.
std::string ForBwuIntroduction(const std::string& endpoint_id,
                             bool supports_disabling_encryption);
std::string ForBwuIntroductionAck();
std::string ForBwuWifiHotspotPathAvailable(
    location::nearby::connections::BandwidthUpgradeNegotiationFrame::
        UpgradePathInfo::WifiHotspotCredentials credentials,
    bool supports_disabling_encryption);
std::string ForBwuWifiLanPathAvailable(
    const std::vector<ServiceAddress>& addresses);
std::string ForBwuAwdlPathAvailable(const std::string& service_name,
                                  const std::string& service_type,
                                  const std::string& password,
                                  bool supports_disabling_encryption);
std::string ForBwuWifiAwarePathAvailable(const std::string& service_id,
                                       const std::string& service_info,
                                       const std::string& password,
                                       bool supports_disabling_encryption);
std::string ForBwuWifiDirectPathAvailable(const std::string& ssid,
                                        const std::string& password,
                                        std::int32_t port,
                                        std::int32_t frequency,
                                        bool supports_disabling_encryption,
                                        const std::string& gateway,
                                        const std::string& service_name,
                                        const std::string& pin);
std::string ForBwuBluetoothPathAvailable(const std::string& service_id,
                                       MacAddress mac_address);
std::string ForBwuWebrtcPathAvailable(
    const std::string& peer_id,
    const location::nearby::connections::LocationHint& location_hint_a);
std::string ForBwuFailure(const UpgradePathInfo& info);
std::string ForBwuPathRequest(
    const std::vector<Medium>& mediums,
    const location::nearby::connections::MediumRole& medium_role);
std::string ForBwuLastWrite();
std::string ForBwuSafeToClose();

std::string ForKeepAlive();
std::string ForKeepAlive(bool ack, uint32_t seq_num);
std::string ForDisconnection(bool request_safe_to_disconnect,
                           bool ack_safe_to_disconnect);
UpgradePathInfo::Medium MediumToUpgradePathInfoMedium(Medium medium);
Medium UpgradePathInfoMediumToMedium(UpgradePathInfo::Medium medium);

location::nearby::connections::ConnectionRequestFrame::Medium
MediumToConnectionRequestMedium(Medium medium);
Medium ConnectionRequestMediumToMedium(
    location::nearby::connections::ConnectionRequestFrame::Medium medium);
std::vector<Medium> ConnectionRequestMediumsToMediums(
    const location::nearby::connections::ConnectionRequestFrame&
        connection_request_frame);
MediumMetadata::WifiDirectAuthType WFDAuthTypeToMediumMetadataWFDAuthType(
    WifiDirectAuthType wifi_direct_auth_type);
WifiDirectAuthType MediumMetadataWFDAuthTypeToWFDAuthType(
    MediumMetadata::WifiDirectAuthType wifi_direct_auth_type);
std::vector<WifiDirectAuthType> MediumMetadataWFDAuthTypesToWFDAuthTypes(
    const MediumMetadata& medium_metadata);
}  // namespace parser
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_OFFLINE_FRAMES_H_
