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

#include "connections/implementation/offline_frames_validator.h"

#include <cstddef>
#include <regex>  //NOLINT
#include <string>

#include "connections/implementation/internal_payload.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "internal/platform/exception.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {
namespace parser {
namespace {

using PayloadChunk =
    ::location::nearby::connections::PayloadTransferFrame::PayloadChunk;
using ControlMessage =
    ::location::nearby::connections::PayloadTransferFrame::ControlMessage;
using ClientIntroduction = ::location::nearby::connections::
    BandwidthUpgradeNegotiationFrame::ClientIntroduction;
using WifiHotspotCredentials = UpgradePathInfo::WifiHotspotCredentials;
using WifiLanSocket = UpgradePathInfo::WifiLanSocket;
using WifiAwareCredentials = UpgradePathInfo::WifiAwareCredentials;
using WifiDirectCredentials = UpgradePathInfo::WifiDirectCredentials;
using BluetoothCredentials = UpgradePathInfo::BluetoothCredentials;
using WebRtcCredentials = UpgradePathInfo::WebRtcCredentials;
using Medium = ::nearby::connections::Medium;
using ::location::nearby::connections::BandwidthUpgradeNegotiationFrame;
using ::location::nearby::connections::ConnectionRequestFrame;
using ::location::nearby::connections::ConnectionResponseFrame;
using ::location::nearby::connections::PayloadTransferFrame;
using ::location::nearby::connections::V1Frame;

constexpr absl::string_view kIpv4PatternString{
    "^([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\."
    "([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\."
    "([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\."
    "([01]?\\d\\d?|2[0-4]\\d|25[0-5])$"};
constexpr absl::string_view kIpv6PatternString{
    "^([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\."
    "([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\."
    "([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\."
    "([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\."
    "([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\."
    "([01]?\\d\\d?|2[0-4]\\d|25[0-5])$"};
constexpr absl::string_view kWifiDirectSsidPatternString{
    "^DIRECT-[a-zA-Z0-9]{2}.*$"};
constexpr int kWifiDirectSsidMaxLength = 32;
constexpr int kWifiPasswordSsidMinLength = 8;
constexpr int kWifiPasswordSsidMaxLength = 64;

inline bool WithinRange(int value, int min, int max) {
  return value >= min && value < max;
}

Exception EnsureValidConnectionRequestFrame(
    const ConnectionRequestFrame& frame) {
  if (frame.endpoint_id().empty()) return {Exception::kInvalidProtocolBuffer};
  if (frame.endpoint_name().empty()) return {Exception::kInvalidProtocolBuffer};

  // For backwards compatibility reasons, no other fields should be
  // null-checked for this frame. Parameter checking (eg. must be within this
  // range) is fine.
  return {Exception::kSuccess};
}

Exception EnsureValidConnectionResponseFrame(
    const ConnectionResponseFrame& frame) {
  // For backwards compatibility reasons, no other fields should be null-checked
  // for this frame. Parameter checking (eg. must be within this range) is fine.
  return {Exception::kSuccess};
}

Exception EnsureValidPayloadTransferDataFrame(const PayloadChunk& payload_chunk,
                                              std::int64_t totalSize) {
  if (!payload_chunk.has_flags()) {
    LOG(ERROR) << "Missing payload chunk flags";
    return {Exception::kInvalidProtocolBuffer};
  }

  // Special case. The body can be null iff the chunk is flagged as the last
  // chunk.
  bool is_last_chunk = (payload_chunk.flags() &
                        PayloadTransferFrame::PayloadChunk::LAST_CHUNK) != 0;
  if (!payload_chunk.has_body() && !is_last_chunk) {
    LOG(ERROR) << "Missing payload chunk body";
    return {Exception::kInvalidProtocolBuffer};
  }
  if (!payload_chunk.has_offset() || payload_chunk.offset() < 0) {
    LOG(ERROR) << "Invalid payload chunk offset";
    return {Exception::kInvalidProtocolBuffer};
  }
  if (totalSize != InternalPayload::kIndeterminateSize &&
      totalSize < payload_chunk.offset()) {
    LOG(ERROR) << "Payload chunk offset > totalSize";
    return {Exception::kInvalidProtocolBuffer};
  }

  // For backwards compatibility reasons, no other fields should be null-checked
  // for this frame. Parameter checking (eg. must be within this range) is fine.
  return {Exception::kSuccess};
}

Exception EnsureValidPayloadTransferControlFrame(
    const ControlMessage& control_message, std::int64_t totalSize) {
  if (!control_message.has_offset() || control_message.offset() < 0) {
    LOG(ERROR) << "Invalid control message offset";
    return {Exception::kInvalidProtocolBuffer};
  }
  if (totalSize != InternalPayload::kIndeterminateSize &&
      totalSize < control_message.offset()) {
    LOG(ERROR) << "Control message offset > totalSize";
    return {Exception::kInvalidProtocolBuffer};
  }

  // For backwards compatibility reasons, no other fields should be null-checked
  // for this frame. Parameter checking (eg. must be within this range) is fine.
  return {Exception::kSuccess};
}

bool CheckForIllegalCharacters(std::string toBeValidated,
                               const absl::string_view illegalPatterns[],
                               size_t illegalPatternsSize) {
  if (toBeValidated.empty()) {
    return false;
  }

  CHECK_GT(illegalPatternsSize, 0);

  size_t found = 0;
  for (int index = 0; index < illegalPatternsSize; index++) {
    found = toBeValidated.find(std::string(illegalPatterns[index]));

    if (found != std::string::npos) {
      // TODO(jfcarroll): Find a way to issue a log statement here.
      // Currently, this breaks the fuzzer, as a logging dep is not
      // included for it in the BUILD file.
      // NEARBY_LOGS(ERROR) << "In path " << toBeValidated
      //                   << " found illegal character/pattern "
      //                   << illegalPatterns[index];
      return true;
    }
  }
  return false;
}

Exception EnsureValidPayloadTransferFrame(const PayloadTransferFrame& frame) {
  if (!frame.has_payload_header()) {
    LOG(ERROR) << "Missing payload header";
    return {Exception::kInvalidProtocolBuffer};
  }
  if (frame.packet_type() == PayloadTransferFrame::PAYLOAD_ACK) {
    // Phone side code doesn't set "total_size" for "payload_header", so skip
    // checking it.
    return {Exception::kSuccess};
  }
  if (!frame.payload_header().has_total_size() ||
      (frame.payload_header().total_size() < 0 &&
       frame.payload_header().total_size() !=
           InternalPayload::kIndeterminateSize)) {
    LOG(ERROR) << "Invalid payload header size";
    return {Exception::kInvalidProtocolBuffer};
  }
  if (frame.payload_header().has_type() &&
        frame.payload_header().type() ==
            location::nearby::connections::PayloadTransferFrame::
                PayloadHeader::FILE) {
    if (frame.payload_header()
            .has_file_name()) {
      if (CheckForIllegalCharacters(frame.payload_header()
                                        .file_name(),
                                    kIllegalFileNamePatterns,
                                    kIllegalFileNamePatternsSize)) {
        return {Exception::kIllegalCharacters};
      }
    }
    if (frame.payload_header()
            .has_parent_folder()) {
      if (CheckForIllegalCharacters(frame.payload_header()
                                        .parent_folder(),
                                    kIllegalParentFolderPatterns,
                                    kIllegalParentFolderPatternsSize)) {
        return {Exception::kIllegalCharacters};
      }
    }
  }
  if (!frame.has_packet_type()) {
    LOG(ERROR) << "Missing packet type";
    return {Exception::kInvalidProtocolBuffer};
  }
  switch (frame.packet_type()) {
    case PayloadTransferFrame::DATA:
      if (frame.has_payload_chunk()) {
        return EnsureValidPayloadTransferDataFrame(
            frame.payload_chunk(), frame.payload_header().total_size());
      }
      LOG(ERROR) << "Missing payload chunk";
      return {Exception::kInvalidProtocolBuffer};

    case PayloadTransferFrame::CONTROL:
      if (frame.has_control_message()) {
        return EnsureValidPayloadTransferControlFrame(
            frame.control_message(), frame.payload_header().total_size());
      }
      LOG(ERROR) << "Missing control message";
      return {Exception::kInvalidProtocolBuffer};

    default:
      break;
  }

  // For backwards compatibility reasons, no other fields should be null-checked
  // for this frame. Parameter checking (eg. must be within this range) is fine.
  return {Exception::kSuccess};
}

Exception EnsureValidBandwidthUpgradeWifiHotspotPathAvailableFrame(
    const WifiHotspotCredentials& wifi_hotspot_credentials) {
  if (!wifi_hotspot_credentials.has_ssid())
    return {Exception::kInvalidProtocolBuffer};
  if (!wifi_hotspot_credentials.has_password() ||
      !WithinRange(wifi_hotspot_credentials.password().length(),
                   kWifiPasswordSsidMinLength, kWifiPasswordSsidMaxLength))
    return {Exception::kInvalidProtocolBuffer};
  if (!wifi_hotspot_credentials.has_gateway())
    return {Exception::kInvalidProtocolBuffer};
  const std::regex ip4_pattern(std::string(kIpv4PatternString).c_str());
  const std::regex ip6_pattern(std::string(kIpv6PatternString).c_str());
  if (!(std::regex_match(wifi_hotspot_credentials.gateway(), ip4_pattern) ||
        std::regex_match(wifi_hotspot_credentials.gateway(), ip6_pattern)))
    return {Exception::kInvalidProtocolBuffer};

  // For backwards compatibility reasons, no other fields should be null-checked
  // for this frame. Parameter checking (eg. must be within this range) is fine.
  return {Exception::kSuccess};
}

Exception EnsureValidBandwidthUpgradeWifiLanPathAvailableFrame(
    const WifiLanSocket& wifi_lan_socket) {
  if (!wifi_lan_socket.has_ip_address())
    return {Exception::kInvalidProtocolBuffer};
  if (!wifi_lan_socket.has_wifi_port() || wifi_lan_socket.wifi_port() < 0)
    return {Exception::kInvalidProtocolBuffer};

  // For backwards compatibility reasons, no other fields should be null-checked
  // for this frame. Parameter checking (eg. must be within this range) is fine.
  return {Exception::kSuccess};
}

Exception EnsureValidBandwidthUpgradeWifiAwarePathAvailableFrame(
    const WifiAwareCredentials& wifi_aware_credentials) {
  if (!wifi_aware_credentials.has_service_id())
    return {Exception::kInvalidProtocolBuffer};
  if (!wifi_aware_credentials.has_service_info())
    return {Exception::kInvalidProtocolBuffer};

  // For backwards compatibility reasons, no other fields should be null-checked
  // for this frame. Parameter checking (eg. must be within this range) is fine.
  return {Exception::kSuccess};
}

Exception EnsureValidBandwidthUpgradeWifiDirectPathAvailableFrame(
    const WifiDirectCredentials& wifi_direct_credentials) {
  const std::regex ssid_pattern(
      std::string(kWifiDirectSsidPatternString).c_str());
  if (!wifi_direct_credentials.has_ssid() ||
      !(wifi_direct_credentials.ssid().length() < kWifiDirectSsidMaxLength &&
        std::regex_match(wifi_direct_credentials.ssid(), ssid_pattern)))
    return {Exception::kInvalidProtocolBuffer};

  if (!wifi_direct_credentials.has_password() ||
      !WithinRange(wifi_direct_credentials.password().length(),
                   kWifiPasswordSsidMinLength, kWifiPasswordSsidMaxLength))
    return {Exception::kInvalidProtocolBuffer};

  if (!wifi_direct_credentials.has_frequency() ||
      wifi_direct_credentials.frequency() < -1)
    return {Exception::kInvalidProtocolBuffer};

  // For backwards compatibility reasons, no other fields should be null-checked
  // for this frame. Parameter checking (eg. must be within this range) is fine.
  return {Exception::kSuccess};
}

Exception EnsureValidBandwidthUpgradeBluetoothPathAvailableFrame(
    const BluetoothCredentials& bluetooth_credentials) {
  if (!bluetooth_credentials.has_service_name())
    return {Exception::kInvalidProtocolBuffer};
  if (!bluetooth_credentials.has_mac_address())
    return {Exception::kInvalidProtocolBuffer};

  // For backwards compatibility reasons, no other fields should be null-checked
  // for this frame. Parameter checking (eg. must be within this range) is fine.
  return {Exception::kSuccess};
}

Exception EnsureValidBandwidthUpgradeWebRtcPathAvailableFrame(
    const WebRtcCredentials& web_rtc_credentials) {
  if (!web_rtc_credentials.has_peer_id())
    return {Exception::kInvalidProtocolBuffer};

  // For backwards compatibility reasons, no other fields should be null-checked
  // for this frame. Parameter checking (eg. must be within this range) is fine.
  return {Exception::kSuccess};
}

Exception EnsureValidBandwidthUpgradePathAvailableFrame(
    const UpgradePathInfo& upgrade_path_info) {
  if (!upgrade_path_info.has_medium())
    return {Exception::kInvalidProtocolBuffer};
  switch (static_cast<Medium>(upgrade_path_info.medium())) {
    case Medium::WIFI_HOTSPOT:
      if (upgrade_path_info.has_wifi_hotspot_credentials()) {
        return EnsureValidBandwidthUpgradeWifiHotspotPathAvailableFrame(
            upgrade_path_info.wifi_hotspot_credentials());
      }
      return {Exception::kInvalidProtocolBuffer};

    case Medium::WIFI_LAN:
      if (upgrade_path_info.has_wifi_lan_socket()) {
        return EnsureValidBandwidthUpgradeWifiLanPathAvailableFrame(
            upgrade_path_info.wifi_lan_socket());
      }
      return {Exception::kInvalidProtocolBuffer};

    case Medium::WIFI_AWARE:
      if (upgrade_path_info.has_wifi_aware_credentials()) {
        return EnsureValidBandwidthUpgradeWifiAwarePathAvailableFrame(
            upgrade_path_info.wifi_aware_credentials());
      }
      return {Exception::kInvalidProtocolBuffer};

    case Medium::WIFI_DIRECT:
      if (upgrade_path_info.has_wifi_direct_credentials()) {
        return EnsureValidBandwidthUpgradeWifiDirectPathAvailableFrame(
            upgrade_path_info.wifi_direct_credentials());
      }
      return {Exception::kInvalidProtocolBuffer};

    case Medium::BLUETOOTH:
      if (upgrade_path_info.has_bluetooth_credentials()) {
        return EnsureValidBandwidthUpgradeBluetoothPathAvailableFrame(
            upgrade_path_info.bluetooth_credentials());
      }
      return {Exception::kInvalidProtocolBuffer};

    case Medium::WEB_RTC:
      if (upgrade_path_info.has_web_rtc_credentials()) {
        return EnsureValidBandwidthUpgradeWebRtcPathAvailableFrame(
            upgrade_path_info.web_rtc_credentials());
      }
      return {Exception::kInvalidProtocolBuffer};

    default:
      break;
  }

  // For backwards compatibility reasons, no other fields should be null-checked
  // for this frame. Parameter checking (eg. must be within this range) is fine.
  return {Exception::kSuccess};
}

Exception EnsureValidBandwidthUpgradeClientIntroductionFrame(
    const ClientIntroduction& client_introduction) {
  if (!client_introduction.has_endpoint_id())
    return {Exception::kInvalidProtocolBuffer};

  // For backwards compatibility reasons, no other fields should be null-checked
  // for this frame. Parameter checking (eg. must be within this range) is fine.
  return {Exception::kSuccess};
}

Exception EnsureValidBandwidthUpgradeNegotiationFrame(
    const BandwidthUpgradeNegotiationFrame& frame) {
  if (!frame.has_event_type()) return {Exception::kInvalidProtocolBuffer};

  switch (frame.event_type()) {
    case BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE:
      if (frame.has_upgrade_path_info()) {
        return EnsureValidBandwidthUpgradePathAvailableFrame(
            frame.upgrade_path_info());
      }
      return {Exception::kInvalidProtocolBuffer};

    case BandwidthUpgradeNegotiationFrame::CLIENT_INTRODUCTION:
      if (frame.has_client_introduction()) {
        return EnsureValidBandwidthUpgradeClientIntroductionFrame(
            frame.client_introduction());
      }
      return {Exception::kInvalidProtocolBuffer};

    default:
      break;
  }

  // For backwards compatibility reasons, no other fields should be null-checked
  // for this frame. Parameter checking (eg. must be within this range) is fine.
  return {Exception::kSuccess};
}

}  // namespace

Exception EnsureValidOfflineFrame(
    const location::nearby::connections::OfflineFrame& offline_frame) {
  V1Frame::FrameType frame_type = GetFrameType(offline_frame);
  switch (frame_type) {
    case V1Frame::CONNECTION_REQUEST:
      if (offline_frame.has_v1() &&
          offline_frame.v1().has_connection_request()) {
        return EnsureValidConnectionRequestFrame(
            offline_frame.v1().connection_request());
      }
      LOG(ERROR) << "Missing connection request";
      return {Exception::kInvalidProtocolBuffer};

    case V1Frame::CONNECTION_RESPONSE:
      if (offline_frame.has_v1() &&
          offline_frame.v1().has_connection_response()) {
        return EnsureValidConnectionResponseFrame(
            offline_frame.v1().connection_response());
      }
      LOG(ERROR) << "Missing connection response";
      return {Exception::kInvalidProtocolBuffer};

    case V1Frame::PAYLOAD_TRANSFER:
      if (offline_frame.has_v1() && offline_frame.v1().has_payload_transfer()) {
        return EnsureValidPayloadTransferFrame(
            offline_frame.v1().payload_transfer());
      }
      LOG(ERROR) << "Missing payload transfer";
      return {Exception::kInvalidProtocolBuffer};

    case V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION:
      if (offline_frame.has_v1() &&
          offline_frame.v1().has_bandwidth_upgrade_negotiation()) {
        return EnsureValidBandwidthUpgradeNegotiationFrame(
            offline_frame.v1().bandwidth_upgrade_negotiation());
      }
      LOG(ERROR) << "Missing bandwidth upgrade negotiation";
      return {Exception::kInvalidProtocolBuffer};

    case V1Frame::KEEP_ALIVE:
    case V1Frame::UNKNOWN_FRAME_TYPE:
    default:
      // Nothing to check for these frames.
      break;
  }
  return {Exception::kSuccess};
}

}  // namespace parser
}  // namespace connections
}  // namespace nearby
