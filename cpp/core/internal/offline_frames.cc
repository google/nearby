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

#include "core/internal/offline_frames.h"

#include <memory>
#include <utility>

#include "platform/byte_array.h"

namespace location {
namespace nearby {
namespace connections {

using ExceptionOrOfflineFrame = ExceptionOr<ConstPtr<OfflineFrame>>;

namespace {
std::unique_ptr<OfflineFrame> NewOfflineFrame(
    V1Frame::FrameType frame_type,
    std::unique_ptr<proto_ns::MessageLite> message) {
  V1Frame *v1_frame = new V1Frame();
  v1_frame->set_type(frame_type);

  switch (frame_type) {
    case V1Frame::CONNECTION_REQUEST:
      v1_frame->set_allocated_connection_request(
          static_cast<ConnectionRequestFrame *>(message.release()));
      break;
    case V1Frame::CONNECTION_RESPONSE:
      v1_frame->set_allocated_connection_response(
          static_cast<ConnectionResponseFrame *>(message.release()));
      break;
    case V1Frame::PAYLOAD_TRANSFER:
      v1_frame->set_allocated_payload_transfer(
          static_cast<PayloadTransferFrame *>(message.release()));
      break;
    case V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION:
      v1_frame->set_allocated_bandwidth_upgrade_negotiation(
          static_cast<BandwidthUpgradeNegotiationFrame *>(message.release()));
      break;
    case V1Frame::KEEP_ALIVE:
      v1_frame->set_allocated_keep_alive(
          static_cast<KeepAliveFrame *>(message.release()));
      break;
    default:
      break;
  }

  auto offline_frame = std::make_unique<OfflineFrame>();
  offline_frame->set_version(OfflineFrame::V1);
  offline_frame->set_allocated_v1(v1_frame);
  return offline_frame;
}

ConstPtr<ByteArray> toBytes(std::unique_ptr<OfflineFrame> offline_frame) {
  auto *bytes = new ByteArray{offline_frame->ByteSizeLong()};
  offline_frame->SerializeToArray(bytes->getData(), bytes->size());
  return MakeConstPtr(bytes);
}

}  // namespace

ExceptionOrOfflineFrame OfflineFrames::fromBytes(
    ConstPtr<ByteArray> offline_frame_bytes) {
  auto offline_frame = std::make_unique<OfflineFrame>();

  if (!offline_frame->ParseFromArray(offline_frame_bytes->getData(),
                                     offline_frame_bytes->size())) {
    return ExceptionOrOfflineFrame(Exception::INVALID_PROTOCOL_BUFFER);
  }

  return ExceptionOrOfflineFrame(MakeConstPtr(offline_frame.release()));
}

V1Frame::FrameType OfflineFrames::getFrameType(
    ConstPtr<OfflineFrame> offline_frame) {
  if ((offline_frame->version() == OfflineFrame::V1) &&
      offline_frame->has_v1()) {
    return offline_frame->v1().type();
  }

  return V1Frame::UNKNOWN_FRAME_TYPE;
}

// TODO(b/155752436): Use byte array endpoint_info instead of endpoint_name.
ConstPtr<ByteArray> OfflineFrames::forConnectionRequest(
    const std::string &endpoint_id, const std::string &endpoint_name,
    std::int32_t nonce,
    const std::vector<proto::connections::Medium> &mediums) {
  auto connection_request = std::make_unique<ConnectionRequestFrame>();
  connection_request->set_endpoint_id(endpoint_id);
  connection_request->set_endpoint_name(endpoint_name);
  connection_request->set_endpoint_info(endpoint_name);
  connection_request->set_nonce(nonce);

  for (std::vector<proto::connections::Medium>::const_iterator it =
           mediums.begin();
       it != mediums.end(); it++) {
    connection_request->add_mediums(mediumToConnectionRequestMedium(*it));
  }

  return toBytes(NewOfflineFrame(V1Frame::CONNECTION_REQUEST,
                                 std::move(connection_request)));
}

ConstPtr<ByteArray> OfflineFrames::forConnectionResponse(std::int32_t status) {
  auto connection_response = std::make_unique<ConnectionResponseFrame>();
  connection_response->set_status(status);

  return toBytes(NewOfflineFrame(V1Frame::CONNECTION_RESPONSE,
                                 std::move(connection_response)));
}

ConstPtr<ByteArray> OfflineFrames::forDataPayloadTransferFrame(
    const PayloadTransferFrame::PayloadHeader &header,
    const PayloadTransferFrame::PayloadChunk &chunk) {
  auto payload_transfer = std::make_unique<PayloadTransferFrame>();
  payload_transfer->set_packet_type(PayloadTransferFrame::DATA);
  *payload_transfer->mutable_payload_header() = header;
  *payload_transfer->mutable_payload_chunk() = chunk;

  return toBytes(
      NewOfflineFrame(V1Frame::PAYLOAD_TRANSFER, std::move(payload_transfer)));
}

ConstPtr<ByteArray> OfflineFrames::forControlPayloadTransferFrame(
    const PayloadTransferFrame::PayloadHeader &header,
    const PayloadTransferFrame::ControlMessage &control) {
  auto payload_transfer = std::make_unique<PayloadTransferFrame>();
  payload_transfer->set_packet_type(PayloadTransferFrame::CONTROL);
  *payload_transfer->mutable_payload_header() = header;
  *payload_transfer->mutable_control_message() = control;

  return toBytes(
      NewOfflineFrame(V1Frame::PAYLOAD_TRANSFER, std::move(payload_transfer)));
}

ConstPtr<ByteArray> OfflineFrames::
    forWifiHotspotUpgradePathAvailableBandwidthUpgradeNegotiationEvent(
        const std::string &ssid, const std::string &password,
        std::int32_t port) {
  auto *wifi_hotspot_credentials = new BandwidthUpgradeNegotiationFrame::
      UpgradePathInfo::WifiHotspotCredentials();
  wifi_hotspot_credentials->set_ssid(ssid);
  wifi_hotspot_credentials->set_password(password);
  wifi_hotspot_credentials->set_port(port);

  auto *upgrade_path_info =
      new BandwidthUpgradeNegotiationFrame::UpgradePathInfo();
  upgrade_path_info->set_medium(
      BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WIFI_HOTSPOT);
  upgrade_path_info->set_allocated_wifi_hotspot_credentials(
      wifi_hotspot_credentials);

  auto bandwidth_upgrade_negotiation =
      std::make_unique<BandwidthUpgradeNegotiationFrame>();
  bandwidth_upgrade_negotiation->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  bandwidth_upgrade_negotiation->set_allocated_upgrade_path_info(
      upgrade_path_info);

  return toBytes(NewOfflineFrame(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION,
                                 std::move(bandwidth_upgrade_negotiation)));
}

ConstPtr<ByteArray>
OfflineFrames::forLastWriteToPriorChannelBandwidthUpgradeNegotiationEvent() {
  auto bandwidth_upgrade_negotiation =
      std::make_unique<BandwidthUpgradeNegotiationFrame>();
  bandwidth_upgrade_negotiation->set_event_type(
      BandwidthUpgradeNegotiationFrame::LAST_WRITE_TO_PRIOR_CHANNEL);

  return toBytes(NewOfflineFrame(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION,
                                 std::move(bandwidth_upgrade_negotiation)));
}

ConstPtr<ByteArray>
OfflineFrames::forSafeToClosePriorChannelBandwidthUpgradeNegotiationEvent() {
  auto bandwidth_upgrade_negotiation =
      std::make_unique<BandwidthUpgradeNegotiationFrame>();
  bandwidth_upgrade_negotiation->set_event_type(
      BandwidthUpgradeNegotiationFrame::SAFE_TO_CLOSE_PRIOR_CHANNEL);

  return toBytes(NewOfflineFrame(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION,
                                 std::move(bandwidth_upgrade_negotiation)));
}

ConstPtr<ByteArray>
OfflineFrames::forClientIntroductionBandwidthUpgradeNegotiationEvent(
    const std::string &endpoint_id) {
  auto *client_introduction =
      new BandwidthUpgradeNegotiationFrame::ClientIntroduction();
  client_introduction->set_endpoint_id(endpoint_id);

  auto bandwidth_upgrade_negotiation =
      std::make_unique<BandwidthUpgradeNegotiationFrame>();
  bandwidth_upgrade_negotiation->set_event_type(
      BandwidthUpgradeNegotiationFrame::CLIENT_INTRODUCTION);
  bandwidth_upgrade_negotiation->set_allocated_client_introduction(
      client_introduction);

  return toBytes(NewOfflineFrame(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION,
                                 std::move(bandwidth_upgrade_negotiation)));
}

ConstPtr<ByteArray> OfflineFrames::forKeepAlive() {
  return toBytes(
      NewOfflineFrame(V1Frame::KEEP_ALIVE, std::make_unique<KeepAliveFrame>()));
}

ConnectionRequestFrame::Medium OfflineFrames::mediumToConnectionRequestMedium(
    proto::connections::Medium medium) {
  switch (medium) {
    case proto::connections::MDNS:
      return ConnectionRequestFrame::MDNS;
    case proto::connections::BLUETOOTH:
      return ConnectionRequestFrame::BLUETOOTH;
    case proto::connections::WIFI_HOTSPOT:
      return ConnectionRequestFrame::WIFI_HOTSPOT;
    case proto::connections::BLE:
      return ConnectionRequestFrame::BLE;
    case proto::connections::WIFI_LAN:
      return ConnectionRequestFrame::WIFI_LAN;
    default:
      return ConnectionRequestFrame::UNKNOWN_MEDIUM;
  }
}

proto::connections::Medium OfflineFrames::connectionRequestMediumToMedium(
    ConnectionRequestFrame::Medium medium) {
  switch (medium) {
    case ConnectionRequestFrame::MDNS:
      return proto::connections::Medium::MDNS;
    case ConnectionRequestFrame::BLUETOOTH:
      return proto::connections::Medium::BLUETOOTH;
    case ConnectionRequestFrame::WIFI_HOTSPOT:
      return proto::connections::Medium::WIFI_HOTSPOT;
    case ConnectionRequestFrame::BLE:
      return proto::connections::Medium::BLE;
    case ConnectionRequestFrame::WIFI_LAN:
      return proto::connections::Medium::WIFI_LAN;
    default:
      return proto::connections::Medium::UNKNOWN_MEDIUM;
  }
}

std::vector<proto::connections::Medium>
OfflineFrames::connectionRequestMediumsToMediums(
    const ConnectionRequestFrame &connection_request_frame) {
  std::vector<proto::connections::Medium> result;
  for (size_t i = 0; i < connection_request_frame.mediums_size(); i++) {
    result.push_back(
        connectionRequestMediumToMedium(connection_request_frame.mediums(i)));
  }
  return result;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
