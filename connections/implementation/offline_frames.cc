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

#include "connections/implementation/offline_frames.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/internal_payload.h"
#include "connections/implementation/offline_frames_validator.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/medium_selector.h"
#include "connections/status.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace parser {
namespace {

using ExceptionOrOfflineFrame =
    ExceptionOr<::location::nearby::connections::OfflineFrame>;
using MessageLite = ::google::protobuf::MessageLite;
using ::location::nearby::connections::BandwidthUpgradeNegotiationFrame;
using ::location::nearby::connections::ConnectionRequestFrame;
using ::location::nearby::connections::ConnectionResponseFrame;
using ::location::nearby::connections::LocationHint;
using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::OsInfo;
using ::location::nearby::connections::PayloadTransferFrame;
using ::location::nearby::connections::V1Frame;
using ::location::nearby::connections::AutoReconnectFrame;

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
    Exception validation_exception = EnsureValidOfflineFrame(frame);
    if (validation_exception.Raised()) {
      return ExceptionOrOfflineFrame(validation_exception);
    }
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

ByteArray ForConnectionRequestConnections(
    const location::nearby::connections::ConnectionsDevice&
        proto_connections_device,
    const ConnectionInfo& conection_info) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::CONNECTION_REQUEST);
  auto* connection_request = v1_frame->mutable_connection_request();
  if (proto_connections_device.has_endpoint_id()) {
    connection_request->mutable_connections_device()->MergeFrom(
        proto_connections_device);
  }
  if (!conection_info.local_endpoint_id.empty()) {
    connection_request->set_endpoint_id(conection_info.local_endpoint_id);
  }
  if (!conection_info.local_endpoint_info.Empty()) {
    connection_request->set_endpoint_name(
        conection_info.local_endpoint_info.string_data());
    connection_request->set_endpoint_info(
        conection_info.local_endpoint_info.string_data());
  }
  connection_request->set_nonce(conection_info.nonce);
  auto* medium_metadata = connection_request->mutable_medium_metadata();
  medium_metadata->set_supports_5_ghz(conection_info.supports_5_ghz);
  if (!conection_info.bssid.empty())
    medium_metadata->set_bssid(conection_info.bssid);
  medium_metadata->set_ap_frequency(conection_info.ap_frequency);
  if (!conection_info.ip_address.empty())
    medium_metadata->set_ip_address(conection_info.ip_address);
  if (!conection_info.supported_mediums.empty()) {
    for (const auto& medium : conection_info.supported_mediums) {
      connection_request->add_mediums(MediumToConnectionRequestMedium(medium));
    }
  }
  if (conection_info.keep_alive_interval_millis > 0) {
    connection_request->set_keep_alive_interval_millis(
        conection_info.keep_alive_interval_millis);
  }
  if (conection_info.keep_alive_timeout_millis > 0) {
    connection_request->set_keep_alive_timeout_millis(
        conection_info.keep_alive_timeout_millis);
  }

  return ToBytes(std::move(frame));
}

ByteArray ForConnectionRequestPresence(
    const location::nearby::connections::PresenceDevice& proto_presence_device,
    const ConnectionInfo& connection_info) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::CONNECTION_REQUEST);
  auto* connection_request = v1_frame->mutable_connection_request();
  if (!connection_info.local_endpoint_id.empty()) {
    connection_request->set_endpoint_id(proto_presence_device.endpoint_id());
  }
  if (!connection_info.local_endpoint_info.Empty()) {
    connection_request->set_endpoint_name(
        connection_info.local_endpoint_info.string_data());
    connection_request->set_endpoint_info(
        connection_info.local_endpoint_info.string_data());
  }
  connection_request->mutable_presence_device()->MergeFrom(
      proto_presence_device);
  connection_request->set_nonce(connection_info.nonce);
  auto* medium_metadata = connection_request->mutable_medium_metadata();
  medium_metadata->set_supports_5_ghz(connection_info.supports_5_ghz);
  if (!connection_info.bssid.empty())
    medium_metadata->set_bssid(connection_info.bssid);
  medium_metadata->set_ap_frequency(connection_info.ap_frequency);
  if (!connection_info.ip_address.empty())
    medium_metadata->set_ip_address(connection_info.ip_address);
  if (!connection_info.supported_mediums.empty()) {
    for (const auto& medium : connection_info.supported_mediums) {
      connection_request->add_mediums(MediumToConnectionRequestMedium(medium));
    }
  }
  if (connection_info.keep_alive_interval_millis > 0) {
    connection_request->set_keep_alive_interval_millis(
        connection_info.keep_alive_interval_millis);
  }
  if (connection_info.keep_alive_timeout_millis > 0) {
    connection_request->set_keep_alive_timeout_millis(
        connection_info.keep_alive_timeout_millis);
  }

  return ToBytes(std::move(frame));
}

ByteArray ForConnectionResponse(std::int32_t status, const OsInfo& os_info,
                                std::int32_t multiplex_socket_bitmask) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::CONNECTION_RESPONSE);
  auto* sub_frame = v1_frame->mutable_connection_response();

  // For backward compatiblility, here still sets both status and response
  // parameters until the response feature is roll out in all supported
  // devices.
  sub_frame->set_status(status);
  sub_frame->set_response(status == Status::kSuccess
                              ? ConnectionResponseFrame::ACCEPT
                              : ConnectionResponseFrame::REJECT);
  *sub_frame->mutable_os_info() = os_info;
  sub_frame->set_multiplex_socket_bitmask(multiplex_socket_bitmask);
  sub_frame->set_safe_to_disconnect_version(
      NearbyFlags::GetInstance().GetInt64Flag(
          config_package_nearby::nearby_connections_feature::
              kSafeToDisconnectVersion));

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

ByteArray ForPayloadAckPayloadTransfer(std::int64_t payload_id) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::PAYLOAD_TRANSFER);
  auto* sub_frame = v1_frame->mutable_payload_transfer();
  sub_frame->set_packet_type(PayloadTransferFrame::PAYLOAD_ACK);

  PayloadTransferFrame::PayloadHeader header;
  header.set_id(payload_id);
  header.set_total_size(InternalPayload::kIndeterminateSize);
  *sub_frame->mutable_payload_header() = header;

  return ToBytes(std::move(frame));
}

ByteArray ForBwuWifiHotspotPathAvailable(const std::string& ssid,
                                         const std::string& password,
                                         std::int32_t port,
                                         std::int32_t frequency,
                                         const std::string& gateway,
                                         bool supports_disabling_encryption) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = sub_frame->mutable_upgrade_path_info();
  upgrade_path_info->set_medium(UpgradePathInfo::WIFI_HOTSPOT);
  upgrade_path_info->set_supports_client_introduction_ack(true);
  upgrade_path_info->set_supports_disabling_encryption(
      supports_disabling_encryption);
  auto* wifi_hotspot_credentials =
      upgrade_path_info->mutable_wifi_hotspot_credentials();
  wifi_hotspot_credentials->set_ssid(ssid);
  wifi_hotspot_credentials->set_password(password);
  wifi_hotspot_credentials->set_port(port);
  wifi_hotspot_credentials->set_frequency(frequency);
  wifi_hotspot_credentials->set_gateway(gateway);

  return ToBytes(std::move(frame));
}

ByteArray ForBwuWifiLanPathAvailable(const std::string& ip_address,
                                     std::int32_t port) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = sub_frame->mutable_upgrade_path_info();
  upgrade_path_info->set_medium(UpgradePathInfo::WIFI_LAN);
  upgrade_path_info->set_supports_client_introduction_ack(true);
  auto* wifi_lan_socket = upgrade_path_info->mutable_wifi_lan_socket();
  wifi_lan_socket->set_ip_address(ip_address);
  wifi_lan_socket->set_wifi_port(port);

  return ToBytes(std::move(frame));
}

ByteArray ForBwuWifiAwarePathAvailable(const std::string& service_id,
                                       const std::string& service_info,
                                       const std::string& password,
                                       bool supports_disabling_encryption) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = sub_frame->mutable_upgrade_path_info();
  upgrade_path_info->set_medium(UpgradePathInfo::WIFI_AWARE);
  upgrade_path_info->set_supports_client_introduction_ack(true);
  upgrade_path_info->set_supports_disabling_encryption(
      supports_disabling_encryption);
  auto* wifi_aware_credentials =
      upgrade_path_info->mutable_wifi_aware_credentials();
  wifi_aware_credentials->set_service_id(service_id);
  wifi_aware_credentials->set_service_info(service_info);
  if (!password.empty()) wifi_aware_credentials->set_password(password);

  return ToBytes(std::move(frame));
}

ByteArray ForBwuWifiDirectPathAvailable(const std::string& ssid,
                                        const std::string& password,
                                        std::int32_t port,
                                        std::int32_t frequency,
                                        bool supports_disabling_encryption,
                                        const std::string& gateway) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = sub_frame->mutable_upgrade_path_info();
  upgrade_path_info->set_medium(UpgradePathInfo::WIFI_DIRECT);
  upgrade_path_info->set_supports_client_introduction_ack(true);
  upgrade_path_info->set_supports_disabling_encryption(
      supports_disabling_encryption);
  auto* wifi_direct_credentials =
      upgrade_path_info->mutable_wifi_direct_credentials();
  wifi_direct_credentials->set_ssid(ssid);
  wifi_direct_credentials->set_password(password);
  wifi_direct_credentials->set_port(port);
  wifi_direct_credentials->set_frequency(frequency);
  wifi_direct_credentials->set_gateway(gateway);

  return ToBytes(std::move(frame));
}

ByteArray ForBwuBluetoothPathAvailable(const std::string& service_id,
                                       const std::string& mac_address) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = sub_frame->mutable_upgrade_path_info();
  upgrade_path_info->set_medium(UpgradePathInfo::BLUETOOTH);
  upgrade_path_info->set_supports_client_introduction_ack(true);
  auto* bluetooth_credentials =
      upgrade_path_info->mutable_bluetooth_credentials();
  bluetooth_credentials->set_mac_address(mac_address);
  bluetooth_credentials->set_service_name(service_id);

  return ToBytes(std::move(frame));
}

ByteArray ForBwuWebrtcPathAvailable(const std::string& peer_id,
                                    const LocationHint& location_hint) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = sub_frame->mutable_upgrade_path_info();
  upgrade_path_info->set_medium(UpgradePathInfo::WEB_RTC);
  upgrade_path_info->set_supports_client_introduction_ack(true);
  auto* webrtc_credentials = upgrade_path_info->mutable_web_rtc_credentials();
  webrtc_credentials->set_peer_id(peer_id);
  auto* local_location_hint = webrtc_credentials->mutable_location_hint();
  *local_location_hint = location_hint;

  return ToBytes(std::move(frame));
}

ByteArray ForBwuLastWrite() {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::LAST_WRITE_TO_PRIOR_CHANNEL);

  return ToBytes(std::move(frame));
}

ByteArray ForBwuSafeToClose() {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::SAFE_TO_CLOSE_PRIOR_CHANNEL);

  return ToBytes(std::move(frame));
}

ByteArray ForBwuIntroduction(const std::string& endpoint_id,
                             bool supports_disabling_encryption) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::CLIENT_INTRODUCTION);
  auto* client_introduction = sub_frame->mutable_client_introduction();
  client_introduction->set_endpoint_id(endpoint_id);
  client_introduction->set_supports_disabling_encryption(
      supports_disabling_encryption);

  return ToBytes(std::move(frame));
}

ByteArray ForBwuIntroductionAck() {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::CLIENT_INTRODUCTION_ACK);

  return ToBytes(std::move(frame));
}

ByteArray ForBwuFailure(const UpgradePathInfo& info) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(BandwidthUpgradeNegotiationFrame::UPGRADE_FAILURE);
  auto* upgrade_path_info = sub_frame->mutable_upgrade_path_info();
  *upgrade_path_info = info;

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

ByteArray ForDisconnection(bool request_safe_to_disconnect,
                           bool ack_safe_to_disconnect) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::DISCONNECTION);
  auto* disconnection = v1_frame->mutable_disconnection();
  disconnection->set_request_safe_to_disconnect(request_safe_to_disconnect);
  disconnection->set_ack_safe_to_disconnect(ack_safe_to_disconnect);

  return ToBytes(std::move(frame));
}

ByteArray ForAutoReconnectIntroduction(const std::string& endpoint_id) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::AUTO_RECONNECT);
  auto* auto_reconnect = v1_frame->mutable_auto_reconnect();
  auto_reconnect->set_endpoint_id(endpoint_id);
  auto_reconnect->set_event_type(AutoReconnectFrame::CLIENT_INTRODUCTION);

  return ToBytes(std::move(frame));
}

ByteArray ForAutoReconnectIntroductionAck(const std::string& endpoint_id) {
  OfflineFrame frame;

  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  v1_frame->set_type(V1Frame::AUTO_RECONNECT);
  auto* auto_reconnect = v1_frame->mutable_auto_reconnect();
  auto_reconnect->set_endpoint_id(endpoint_id);
  auto_reconnect->set_event_type(AutoReconnectFrame::CLIENT_INTRODUCTION_ACK);

  return ToBytes(std::move(frame));
}

UpgradePathInfo::Medium MediumToUpgradePathInfoMedium(Medium medium) {
  switch (medium) {
    case Medium::MDNS:
      return UpgradePathInfo::MDNS;
    case Medium::BLUETOOTH:
      return UpgradePathInfo::BLUETOOTH;
    case Medium::WIFI_HOTSPOT:
      return UpgradePathInfo::WIFI_HOTSPOT;
    case Medium::BLE:
      return UpgradePathInfo::BLE;
    case Medium::WIFI_LAN:
      return UpgradePathInfo::WIFI_LAN;
    case Medium::WIFI_AWARE:
      return UpgradePathInfo::WIFI_AWARE;
    case Medium::NFC:
      return UpgradePathInfo::NFC;
    case Medium::WIFI_DIRECT:
      return UpgradePathInfo::WIFI_DIRECT;
    case Medium::WEB_RTC:
      return UpgradePathInfo::WEB_RTC;
    default:
      return UpgradePathInfo::UNKNOWN_MEDIUM;
  }
}

Medium UpgradePathInfoMediumToMedium(UpgradePathInfo::Medium medium) {
  switch (medium) {
    case UpgradePathInfo::MDNS:
      return Medium::MDNS;
    case UpgradePathInfo::BLUETOOTH:
      return Medium::BLUETOOTH;
    case UpgradePathInfo::WIFI_HOTSPOT:
      return Medium::WIFI_HOTSPOT;
    case UpgradePathInfo::BLE:
      return Medium::BLE;
    case UpgradePathInfo::WIFI_LAN:
      return Medium::WIFI_LAN;
    case UpgradePathInfo::WIFI_AWARE:
      return Medium::WIFI_AWARE;
    case UpgradePathInfo::NFC:
      return Medium::NFC;
    case UpgradePathInfo::WIFI_DIRECT:
      return Medium::WIFI_DIRECT;
    case UpgradePathInfo::WEB_RTC:
      return Medium::WEB_RTC;
    default:
      return Medium::UNKNOWN_MEDIUM;
  }
}

ConnectionRequestFrame::Medium MediumToConnectionRequestMedium(Medium medium) {
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

Medium ConnectionRequestMediumToMedium(ConnectionRequestFrame::Medium medium) {
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

std::vector<Medium> ConnectionRequestMediumsToMediums(
    const ConnectionRequestFrame& frame) {
  std::vector<Medium> result;
  for (const auto& int_medium : frame.mediums()) {
    result.push_back(ConnectionRequestMediumToMedium(
        static_cast<ConnectionRequestFrame::Medium>(int_medium)));
  }
  return result;
}

}  // namespace parser
}  // namespace connections
}  // namespace nearby
