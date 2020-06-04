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

#include "core_v2/internal/offline_frames.h"

#include <memory>
#include <utility>

#include "core/internal/message_lite.h"
#include "platform_v2/base/byte_array.h"

namespace location {
namespace nearby {
namespace connections {
namespace parser {
namespace {

using ExceptionOrOfflineFrame = ExceptionOr<OfflineFrame>;
using Medium = proto::connections::Medium;
using MessageLite = ::google::protobuf::MessageLite;

ByteArray ToBytes(OfflineFrame&& frame) {
  ByteArray bytes(frame.ByteSizeLong());
  frame.set_version(OfflineFrame::V1);
  frame.SerializeToArray(bytes.data(), bytes.size());
  return bytes;
}

}  // namespace

ExceptionOrOfflineFrame FromBytes(const ByteArray& bytes) {
  OfflineFrame frame;

  if (frame.ParseFromString(std::string(bytes))) {
    return ExceptionOrOfflineFrame(std::move(frame));
  } else {
    return ExceptionOrOfflineFrame(Exception::kInvalidProtocolBuffer);
  }
}

V1Frame::FrameType GetFrameType(const OfflineFrame& frame) {
  if ((frame.version() == OfflineFrame::V1) && frame.has_v1()) {
    return frame.v1().type();
  }

  return V1Frame::UNKNOWN_FRAME_TYPE;
}

ByteArray ForConnectionRequest(const std::string& endpoint_id,
                               const std::string& endpoint_name,
                               std::int32_t nonce,
                               const std::vector<Medium>& mediums) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::CONNECTION_REQUEST);
  auto* connection_request = v1_frame->mutable_connection_request();
  connection_request->set_endpoint_id(endpoint_id);
  connection_request->set_endpoint_name(endpoint_name);
  connection_request->set_endpoint_info(endpoint_name);
  connection_request->set_nonce(nonce);
  for (const auto& medium : mediums) {
    connection_request->add_mediums(MediumToConnectionRequestMedium(medium));
  }

  return ToBytes(std::move(frame));
}

ByteArray ForConnectionResponse(std::int32_t status) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::CONNECTION_RESPONSE);
  auto* sub_frame = v1_frame->mutable_connection_response();
  sub_frame->set_status(status);

  return ToBytes(std::move(frame));
}

ByteArray ForDataPayloadTransfer(
    const PayloadTransferFrame::PayloadHeader& header,
    const PayloadTransferFrame::PayloadChunk& chunk) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::PAYLOAD_TRANSFER);
  auto* sub_frame = v1_frame->mutable_payload_transfer();
  sub_frame->set_packet_type(PayloadTransferFrame::DATA);
  *sub_frame->mutable_payload_header() = header;
  *sub_frame->mutable_payload_chunk() = chunk;

  return ToBytes(std::move(frame));
}

ByteArray ForControlPayloadTransfer(
    const PayloadTransferFrame::PayloadHeader& header,
    const PayloadTransferFrame::ControlMessage& control) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::PAYLOAD_TRANSFER);
  auto* sub_frame = v1_frame->mutable_payload_transfer();
  sub_frame->set_packet_type(PayloadTransferFrame::CONTROL);
  *sub_frame->mutable_payload_header() = header;
  *sub_frame->mutable_control_message() = control;

  return ToBytes(std::move(frame));
}

ByteArray ForBandwidthUpgradeWifiHotspot(const std::string& ssid,
                                         const std::string& password,
                                         std::int32_t port) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = sub_frame->mutable_upgrade_path_info();
  upgrade_path_info->set_medium(
      BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WIFI_HOTSPOT);
  auto* wifi_hotspot_credentials =
      upgrade_path_info->mutable_wifi_hotspot_credentials();
  wifi_hotspot_credentials->set_ssid(ssid);
  wifi_hotspot_credentials->set_password(password);
  wifi_hotspot_credentials->set_port(port);

  return ToBytes(std::move(frame));
}

ByteArray ForBandwidthUpgradeLastWrite() {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::LAST_WRITE_TO_PRIOR_CHANNEL);

  return ToBytes(std::move(frame));
}

ByteArray ForBandwidthUpgradeSafeToClose() {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::SAFE_TO_CLOSE_PRIOR_CHANNEL);

  return ToBytes(std::move(frame));
}

ByteArray ForBandwidthUpgradeIntroduction(const std::string& endpoint_id) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::CLIENT_INTRODUCTION);
  auto* client_introduction = sub_frame->mutable_client_introduction();
  client_introduction->set_endpoint_id(endpoint_id);

  return ToBytes(std::move(frame));
}

ByteArray ForKeepAlive() {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::KEEP_ALIVE);
  v1_frame->mutable_keep_alive();

  return ToBytes(std::move(frame));
}

ConnectionRequestFrame::Medium MediumToConnectionRequestMedium(
    proto::connections::Medium medium) {
  switch (medium) {
    case Medium::MDNS:
      return ConnectionRequestFrame::MDNS;
    case Medium::BLUETOOTH:
      return ConnectionRequestFrame::BLUETOOTH;
    case Medium::WIFI_HOTSPOT:
      return ConnectionRequestFrame::WIFI_HOTSPOT;
    case Medium::BLE:
      return ConnectionRequestFrame::BLE;
    case Medium::WIFI_LAN:
      return ConnectionRequestFrame::WIFI_LAN;
    case Medium::WIFI_AWARE:
      return ConnectionRequestFrame::WIFI_AWARE;
    case Medium::NFC:
      return ConnectionRequestFrame::NFC;
    case Medium::WIFI_DIRECT:
      return ConnectionRequestFrame::WIFI_DIRECT;
    case Medium::WEB_RTC:
      return ConnectionRequestFrame::WEB_RTC;
    default:
      return ConnectionRequestFrame::UNKNOWN_MEDIUM;
  }
}

proto::connections::Medium ConnectionRequestMediumToMedium(
    ConnectionRequestFrame::Medium medium) {
  switch (medium) {
    case ConnectionRequestFrame::MDNS:
      return Medium::MDNS;
    case ConnectionRequestFrame::BLUETOOTH:
      return Medium::BLUETOOTH;
    case ConnectionRequestFrame::WIFI_HOTSPOT:
      return Medium::WIFI_HOTSPOT;
    case ConnectionRequestFrame::BLE:
      return Medium::BLE;
    case ConnectionRequestFrame::WIFI_LAN:
      return Medium::WIFI_LAN;
    case ConnectionRequestFrame::WIFI_AWARE:
      return Medium::WIFI_AWARE;
    case ConnectionRequestFrame::NFC:
      return Medium::NFC;
    case ConnectionRequestFrame::WIFI_DIRECT:
      return Medium::WIFI_DIRECT;
    case ConnectionRequestFrame::WEB_RTC:
      return Medium::WEB_RTC;
    default:
      return Medium::UNKNOWN_MEDIUM;
  }
}

std::vector<proto::connections::Medium> ConnectionRequestMediumsToMediums(
    const ConnectionRequestFrame& frame) {
  std::vector<proto::connections::Medium> result;
  for (const auto& int_medium : frame.mediums()) {
    result.push_back(ConnectionRequestMediumToMedium(
        static_cast<ConnectionRequestFrame::Medium>(int_medium)));
  }
  return result;
}

}  // namespace parser
}  // namespace connections
}  // namespace nearby
}  // namespace location
