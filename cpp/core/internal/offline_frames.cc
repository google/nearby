#include "core/internal/offline_frames.h"

#include "platform/port/down_cast.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

template <typename T>
T *downcastToRaw(Ptr<proto_ns::MessageLite> message) {
  return DOWN_CAST<T *>(message.operator->());
}

// This method takes ownership of the passed-in 'message'.
//
// This can be implemented more efficiently by taking in a reference to an
// OfflineFrame object created on the caller's stack, but we instead create it
// on the heap and return a Ptr to it for the sake of consistency.
ConstPtr<OfflineFrame> newOfflineFrame(V1Frame::FrameType frame_type,
                                       Ptr<proto_ns::MessageLite> message) {
  V1Frame *v1_frame = new V1Frame();
  v1_frame->set_type(frame_type);

  switch (frame_type) {
    case V1Frame::CONNECTION_REQUEST:
      v1_frame->set_allocated_connection_request(
          downcastToRaw<ConnectionRequestFrame>(message));
      break;
    case V1Frame::CONNECTION_RESPONSE:
      v1_frame->set_allocated_connection_response(
          downcastToRaw<ConnectionResponseFrame>(message));
      break;
    case V1Frame::PAYLOAD_TRANSFER:
      v1_frame->set_allocated_payload_transfer(
          downcastToRaw<PayloadTransferFrame>(message));
      break;
    case V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION:
      v1_frame->set_allocated_bandwidth_upgrade_negotiation(
          downcastToRaw<BandwidthUpgradeNegotiationFrame>(message));
      break;
    case V1Frame::KEEP_ALIVE:
      v1_frame->set_allocated_keep_alive(
          downcastToRaw<KeepAliveFrame>(message));
      break;
    default:
      break;
  }

  Ptr<OfflineFrame> offline_frame(new OfflineFrame());
  offline_frame->set_version(OfflineFrame::V1);
  offline_frame->set_allocated_v1(v1_frame);
  return ConstifyPtr(offline_frame);
}

// This method takes ownership of the passed-in 'offline_frame' and destroys it
// before returning.
ConstPtr<ByteArray> toBytes(ConstPtr<OfflineFrame> offline_frame) {
  ScopedPtr<ConstPtr<OfflineFrame> > scoped_offline_frame(offline_frame);

  size_t serialized_size = offline_frame->ByteSizeLong();
  Ptr<ByteArray> bytes{new ByteArray{serialized_size}};

  offline_frame->SerializeToArray(bytes->getData(), serialized_size);
  return ConstifyPtr(bytes);
}

}  // namespace

ExceptionOr<ConstPtr<OfflineFrame> > OfflineFrames::fromBytes(
    ConstPtr<ByteArray> offline_frame_bytes) {
  ScopedPtr<Ptr<OfflineFrame> > offline_frame(new OfflineFrame());

  if (!offline_frame->ParseFromArray(offline_frame_bytes->getData(),
                                     offline_frame_bytes->size())) {
    return ExceptionOr<ConstPtr<OfflineFrame> >(
        Exception::INVALID_PROTOCOL_BUFFER);
  }

  return ExceptionOr<ConstPtr<OfflineFrame> >(
      ConstifyPtr(offline_frame.release()));
}

V1Frame::FrameType OfflineFrames::getFrameType(
    ConstPtr<OfflineFrame> offline_frame) {
  if ((offline_frame->version() == OfflineFrame::V1) &&
      offline_frame->has_v1()) {
    return offline_frame->v1().type();
  }

  return V1Frame::UNKNOWN_FRAME_TYPE;
}

ConstPtr<ByteArray> OfflineFrames::forConnectionRequest(
    const std::string &endpoint_id, const std::string &endpoint_name,
    std::int32_t nonce,
    const std::vector<proto::connections::Medium> &mediums) {
  Ptr<ConnectionRequestFrame> connection_request(new ConnectionRequestFrame());
  connection_request->set_endpoint_id(endpoint_id);
  connection_request->set_endpoint_name(endpoint_name);
  connection_request->set_nonce(nonce);

  for (std::vector<proto::connections::Medium>::const_iterator it =
           mediums.begin();
       it != mediums.end(); it++) {
    connection_request->add_mediums(mediumToConnectionRequestMedium(*it));
  }

  return toBytes(
      newOfflineFrame(V1Frame::CONNECTION_REQUEST, connection_request));
}

ConstPtr<ByteArray> OfflineFrames::forConnectionResponse(std::int32_t status) {
  Ptr<ConnectionResponseFrame> connection_response(
      new ConnectionResponseFrame());
  connection_response->set_status(status);

  return toBytes(
      newOfflineFrame(V1Frame::CONNECTION_RESPONSE, connection_response));
}

ConstPtr<ByteArray> OfflineFrames::forDataPayloadTransferFrame(
    const PayloadTransferFrame::PayloadHeader &header,
    const PayloadTransferFrame::PayloadChunk &chunk) {
  Ptr<PayloadTransferFrame> payload_transfer(new PayloadTransferFrame());
  payload_transfer->set_packet_type(PayloadTransferFrame::DATA);
  *payload_transfer->mutable_payload_header() = header;
  *payload_transfer->mutable_payload_chunk() = chunk;

  return toBytes(newOfflineFrame(V1Frame::PAYLOAD_TRANSFER, payload_transfer));
}

ConstPtr<ByteArray> OfflineFrames::forControlPayloadTransferFrame(
    const PayloadTransferFrame::PayloadHeader &header,
    const PayloadTransferFrame::ControlMessage &control) {
  Ptr<PayloadTransferFrame> payload_transfer(new PayloadTransferFrame());
  payload_transfer->set_packet_type(PayloadTransferFrame::CONTROL);
  *payload_transfer->mutable_payload_header() = header;
  *payload_transfer->mutable_control_message() = control;

  return toBytes(newOfflineFrame(V1Frame::PAYLOAD_TRANSFER, payload_transfer));
}

ConstPtr<ByteArray> OfflineFrames::
    forWifiHotspotUpgradePathAvailableBandwidthUpgradeNegotiationEvent(
        const std::string &ssid, const std::string &password,
        std::int32_t port) {
  BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiHotspotCredentials
      *wifi_hotspot_credentials = new BandwidthUpgradeNegotiationFrame::
          UpgradePathInfo::WifiHotspotCredentials();
  wifi_hotspot_credentials->set_ssid(ssid);
  wifi_hotspot_credentials->set_password(password);
  wifi_hotspot_credentials->set_port(port);

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo *upgrade_path_info =
      new BandwidthUpgradeNegotiationFrame::UpgradePathInfo();
  upgrade_path_info->set_medium(
      BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WIFI_HOTSPOT);
  upgrade_path_info->set_allocated_wifi_hotspot_credentials(
      wifi_hotspot_credentials);

  Ptr<BandwidthUpgradeNegotiationFrame> bandwidth_upgrade_negotiation(
      new BandwidthUpgradeNegotiationFrame());
  bandwidth_upgrade_negotiation->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  bandwidth_upgrade_negotiation->set_allocated_upgrade_path_info(
      upgrade_path_info);

  return toBytes(newOfflineFrame(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION,
                                 bandwidth_upgrade_negotiation));
}

ConstPtr<ByteArray>
OfflineFrames::forLastWriteToPriorChannelBandwidthUpgradeNegotiationEvent() {
  Ptr<BandwidthUpgradeNegotiationFrame> bandwidth_upgrade_negotiation(
      new BandwidthUpgradeNegotiationFrame());
  bandwidth_upgrade_negotiation->set_event_type(
      BandwidthUpgradeNegotiationFrame::LAST_WRITE_TO_PRIOR_CHANNEL);

  return toBytes(newOfflineFrame(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION,
                                 bandwidth_upgrade_negotiation));
}

ConstPtr<ByteArray>
OfflineFrames::forSafeToClosePriorChannelBandwidthUpgradeNegotiationEvent() {
  Ptr<BandwidthUpgradeNegotiationFrame> bandwidth_upgrade_negotiation(
      new BandwidthUpgradeNegotiationFrame());
  bandwidth_upgrade_negotiation->set_event_type(
      BandwidthUpgradeNegotiationFrame::SAFE_TO_CLOSE_PRIOR_CHANNEL);

  return toBytes(newOfflineFrame(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION,
                                 bandwidth_upgrade_negotiation));
}

ConstPtr<ByteArray>
OfflineFrames::forClientIntroductionBandwidthUpgradeNegotiationEvent(
    const std::string &endpoint_id) {
  BandwidthUpgradeNegotiationFrame::ClientIntroduction *client_introduction =
      new BandwidthUpgradeNegotiationFrame::ClientIntroduction();
  client_introduction->set_endpoint_id(endpoint_id);

  Ptr<BandwidthUpgradeNegotiationFrame> bandwidth_upgrade_negotiation(
      new BandwidthUpgradeNegotiationFrame());
  bandwidth_upgrade_negotiation->set_event_type(
      BandwidthUpgradeNegotiationFrame::CLIENT_INTRODUCTION);
  bandwidth_upgrade_negotiation->set_allocated_client_introduction(
      client_introduction);

  return toBytes(newOfflineFrame(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION,
                                 bandwidth_upgrade_negotiation));
}

ConstPtr<ByteArray> OfflineFrames::forKeepAlive() {
  Ptr<KeepAliveFrame> keep_alive_frame(new KeepAliveFrame());
  return toBytes(newOfflineFrame(V1Frame::KEEP_ALIVE, keep_alive_frame));
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
