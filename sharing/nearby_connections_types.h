// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_TYPES_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_TYPES_H_

#include <stdint.h>

#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/base/files.h"
#include "internal/interop/authentication_status.h"
#include "sharing/common/compatible_u8_string.h"

namespace nearby {
namespace sharing {

struct Uuid {
  Uuid() = default;
  explicit Uuid(std::string uuid) { this->uuid = uuid; }

  std::string uuid;
};

// Generic result status of NearbyConnections API calls. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
// LINT.IfChange(status_enum)
enum class Status {
  // The operation was successful.
  kSuccess = 0,
  // The operation failed, without any more information.
  kError = 1,
  // The app called an API method out of order (i.e. another method is expected
  // to be called first).
  kOutOfOrderApiCall = 2,
  // The app already has active operations (advertising, discovering, or
  // connected to other devices) with another Strategy. Stop these operations on
  // the current Strategy before trying to advertise or discover with a new
  // Strategy.
  kAlreadyHaveActiveStrategy = 3,
  // The app is already advertising; call StopAdvertising() before trying to
  // advertise again.
  kAlreadyAdvertising = 4,
  // The app is already discovering; call StopDiscovery() before trying to
  // discover again.
  kAlreadyDiscovering = 5,
  // NC is already listening for incoming connections from remote endpoints.
  kAlreadyListening = 6,
  // An attempt to read from/write to a connected remote endpoint failed. If
  // this occurs repeatedly, consider invoking DisconnectFromEndpoint().
  kEndpointIOError = 7,
  // An attempt to interact with a remote endpoint failed because it's unknown
  // to us -- it's either an endpoint that was never discovered, or an endpoint
  // that never connected to us (both of which are indicative of bad input from
  // the client app).
  kEndpointUnknown = 8,
  // The remote endpoint rejected the connection request.
  kConnectionRejected = 9,
  // The app is already connected to the specified endpoint. Multiple
  // connections to a remote endpoint cannot be maintained simultaneously.
  kAlreadyConnectedToEndpoint = 10,
  // The remote endpoint is not connected; messages cannot be sent to it.
  kNotConnectedToEndpoint = 11,
  // There was an error trying to use the device's Bluetooth capabilities.
  kBluetoothError = 12,
  // There was an error trying to use the device's Bluetooth Low Energy
  // capabilities.
  kBleError = 13,
  // There was an error trying to use the device's Wi-Fi capabilities.
  kWifiLanError = 14,
  // An attempt to interact with an in-flight Payload failed because it's
  // unknown to us.
  kPayloadUnknown = 15,
  // The connection was reset
  kReset = 16,
  // The connection timed out
  kTimeout = 17,
  // No status is available
  kUnknown = 18,
  // Value of the next enum variant.
  kNextValue = 19,
};
// LINT.ThenChange(
//     ../connections/status.h:status_enum,
//     nearby_connections_manager.cc:status_enum,
//     nearby_connections_types_test.cc:status_enum
// )

// Information about a connection that is being initiated.
struct ConnectionInfo {
  // A short human-readable authentication token that has been given to both
  // devices.
  std::string authentication_token;
  // The raw (significantly longer) version of the authentication token of
  // authentication_token -- this is intended for headless authentication,
  // typically on devices with no output capabilities, where the authentication
  // is purely programmatic and does not have the luxury of human intervention.
  std::vector<uint8_t> raw_authentication_token;
  // Information that represents the remote device.
  std::vector<uint8_t> endpoint_info;
  // True if the connection request was initiated from a remote device. False if
  // this device was the one to try and initiate the connection.
  bool is_incoming_connection;
  // Connection status used for analytics
  Status connection_layer_status = Status::kUnknown;
  // Result of authenticating the device with the DeviceProvider, which is only
  // used during `RequestConnectionV3()`. Based on the result, clients should
  // do the following:
  // - `AuthenticationStatus::kFailure`: disconnect from the remote device
  // - `AuthenticationStatus::kUnknown`: prompt the user to confirm the PIN on
  //   both sides (retaining existing behavior)
  // - `AuthenticationStatus::kSuccess`: connect to the device with no further
  //   prompting
  AuthenticationStatus authentication_status = AuthenticationStatus::kUnknown;
};

// Information about an endpoint when it's discovered.
struct DiscoveredEndpointInfo {
  DiscoveredEndpointInfo() = default;
  DiscoveredEndpointInfo(std::vector<uint8_t> endpoint_info,
                         std::string service_id) {
    this->endpoint_info = std::move(endpoint_info);
    this->service_id = std::move(service_id);
  }

  // Information advertised by the remote endpoint.
  std::vector<uint8_t> endpoint_info;
  // The ID of the service advertised by the remote endpoint.
  std::string service_id;
};

// The Strategy to be used when discovering or advertising to Nearby devices.
// The Strategy defines the connectivity requirements for the device, and the
// topology constraints of the connection.
enum class Strategy {
  // Peer-to-peer strategy that supports an M-to-N, or cluster-shaped,
  // connection topology. In other words, this enables connecting amorphous
  // clusters of devices within radio range (~100m), where each device can both
  // initiate outgoing connections to M other devices and accept incoming
  // connections from N other devices.
  kP2pCluster,
  // Peer-to-peer strategy that supports a 1-to-N, or star-shaped, connection
  // topology. In other words, this enables connecting devices within radio
  // range (~100m) in a star shape, where each device can, at any given time,
  // play the role of either a hub (where it can accept incoming connections
  // from N other devices), or a spoke (where it can initiate an outgoing
  // connection to a single hub), but not both.
  kP2pStar,
  // Peer-to-peer strategy that supports a 1-to-1 connection topology. In other
  // words, this enables connecting to a single device within radio range
  // (~100m). This strategy will give the absolute highest bandwidth, but will
  // not allow multiple connections at a time.
  kP2pPointToPoint,
};

// A selection of on/off toggles to define a set of allowed mediums.
struct MediumSelection {
  MediumSelection() = default;
  MediumSelection(bool bluetooth, bool ble, bool web_rtc, bool wifi_lan,
                  bool wifi_hotspot) {
    this->bluetooth = bluetooth;
    this->ble = ble;
    this->web_rtc = web_rtc;
    this->wifi_lan = wifi_lan;
    this->wifi_hotspot = wifi_hotspot;
  }

  // Whether Bluetooth should be allowed.
  bool bluetooth = true;
  // Whether BLE should be allowed.
  bool ble = true;
  // Whether WebRTC should be allowed.
  bool web_rtc = true;
  // Whether Wi-Fi LAN should be allowed.
  bool wifi_lan = true;
  // Whether Wi-Fi Hotspot should be allowed
  bool wifi_hotspot = true;
};

// Options for a call to NearbyConnections::StartAdvertising().
struct AdvertisingOptions {
  AdvertisingOptions() = default;
  AdvertisingOptions(Strategy strategy, MediumSelection allowed_mediums,
                     bool auto_upgrade_bandwidth,
                     bool enforce_topology_constraints,
                     bool enable_bluetooth_listening,
                     bool enable_webrtc_listening,
                     bool use_stable_endpoint_id,
                     Uuid fast_advertisement_service_uuid) {
    this->strategy = strategy;
    this->allowed_mediums = allowed_mediums;
    this->auto_upgrade_bandwidth = auto_upgrade_bandwidth;
    this->enforce_topology_constraints = enforce_topology_constraints;
    this->enable_bluetooth_listening = enable_bluetooth_listening;
    this->enable_webrtc_listening = enable_webrtc_listening;
    this->use_stable_endpoint_id = use_stable_endpoint_id;
    this->fast_advertisement_service_uuid = fast_advertisement_service_uuid;
  }

  // The strategy to use for advertising. Must match the strategy used in
  // DiscoveryOptions for remote devices to see this advertisement.
  Strategy strategy;
  // Describes which mediums are allowed to be used for advertising. Note that
  // allowing an otherwise unsupported medium is ok. Only the intersection of
  // allowed and supported mediums will be used to advertise.
  MediumSelection allowed_mediums;
  // By default, this option is true. If false, we will not attempt to upgrade
  // the bandwidth until a call to InitiateBandwidthUpgrade() is made.
  bool auto_upgrade_bandwidth = true;
  // By default, this option is true. If false, restrictions on topology will be
  // ignored. This allows you treat all strategies as kP2pCluster (N to M),
  // although bandwidth will be severely throttled if you don't maintain the
  // original topology. When used in conjunction with auto_upgrade_bandwidth,
  // you can initially connect as a kP2pCluster and then trim connections until
  // you match kP2pStar or kP2pPointToPoint before upgrading the bandwidth.
  bool enforce_topology_constraints = true;
  // By default, this option is false. If true, this allows listening on
  // incoming Bluetooth Classic connections while BLE advertising.
  bool enable_bluetooth_listening = false;
  // By default, this option is false. If true, this allows listening on
  // incoming WebRTC connections while advertising.
  bool enable_webrtc_listening = false;
  // Indicates whether the endpoint id should be stable. When visibility is
  // everyone mode, we should set this to true to avoid duplicated endpoint ids.
  bool use_stable_endpoint_id = false;
  // Optional. If set, BLE advertisements will be in their "fast advertisement"
  // form, use this UUID, and non-connectable; if empty, BLE advertisements
  // will otherwise be normal and connectable.
  Uuid fast_advertisement_service_uuid;
};

// Options for a call to NearbyConnections::StartDiscovery().
struct DiscoveryOptions {
  DiscoveryOptions() = default;
  DiscoveryOptions(Strategy strategy, MediumSelection allowed_mediums,
                   std::optional<Uuid> fast_advertisement_service_uuid,
                   bool is_out_of_band_connection) {
    this->strategy = strategy;
    this->allowed_mediums = allowed_mediums;
    this->fast_advertisement_service_uuid = fast_advertisement_service_uuid,
    this->is_out_of_band_connection = is_out_of_band_connection;
  }
  // The strategy to use for discovering. Must match the strategy used in
  // AdvertisingOptions in order to see advertisements.
  Strategy strategy;
  // Describes which mediums are allowed to be used for scanning/discovery. Note
  // that allowing an otherwise unsupported medium is ok. Only the intersection
  // of allowed and supported mediums will be used to scan.
  MediumSelection allowed_mediums;
  // The fast advertisement service id to scan for in BLE.
  std::optional<Uuid> fast_advertisement_service_uuid;
  // Whether this connection request skips over the normal discovery flow to
  // inject discovery information synced outside the Nearby Connections library.
  // Intended to be used in conjunction with InjectEndpoint().
  bool is_out_of_band_connection = false;
};

// Options for a call to NearbyConnections::RequestConnection().
struct ConnectionOptions {
  ConnectionOptions() = default;
  ConnectionOptions(
      MediumSelection allowed_mediums,
      std::optional<std::vector<uint8_t>> remote_bluetooth_mac_address,
      std::optional<absl::Duration> keep_alive_interval,
      std::optional<absl::Duration> keep_alive_timeout,
      bool non_disruptive_hotspot_mode) {
    this->allowed_mediums = allowed_mediums;
    this->remote_bluetooth_mac_address = remote_bluetooth_mac_address;
    this->keep_alive_interval = keep_alive_interval;
    this->keep_alive_timeout = keep_alive_timeout;
    this->non_disruptive_hotspot_mode = non_disruptive_hotspot_mode;
  }

  // Describes which mediums are allowed to be used for connection. Note that
  // allowing an otherwise unsupported medium is ok. Only the intersection of
  // allowed and supported mediums will be used to connect.
  MediumSelection allowed_mediums;
  // Bluetooth MAC address of remote device in byte format.
  std::optional<std::vector<uint8_t>> remote_bluetooth_mac_address;
  // How often to send a keep alive message on the channel. An unspecified or
  // negative value will result in the Nearby Connections default of 5 seconds
  // being used.
  std::optional<absl::Duration> keep_alive_interval;
  // The connection will time out if no message is received on the channel
  // for this length of time. An unspecified or negative value will result in
  // the Nearby Connections default of 30 seconds being used.
  std::optional<absl::Duration> keep_alive_timeout;
  // If true, only use WiFi Hotspot for connection when Wifi LAN is not
  // connected.
  bool non_disruptive_hotspot_mode = false;
};

// The status of the payload transfer at the time of this update.
enum PayloadStatus {
  // The payload transfer has completed successfully.
  kSuccess,
  // The payload transfer failed.
  kFailure,
  // The payload transfer is still in progress.
  kInProgress,
  // The payload transfer has been canceled.
  kCanceled,
};

// Describes the status for an active Payload transfer, either incoming or
// outgoing. Delivered to PayloadListener::OnPayloadTransferUpdate.
struct PayloadTransferUpdate {
  PayloadTransferUpdate() = default;
  PayloadTransferUpdate(int64_t payload_id, PayloadStatus status,
                        uint64_t total_bytes, uint64_t bytes_transferred) {
    this->payload_id = payload_id;
    this->status = status;
    this->total_bytes = total_bytes;
    this->bytes_transferred = bytes_transferred;
  }

  // The ID for the payload related to this update. Clients should match this
  // with Payload::id.
  int64_t payload_id;
  // The status of this payload transfer. Always starts with kInProgress and
  // ends with one of kSuccess, kFailure or kCanceled.
  PayloadStatus status;
  // The total expected bytes of this transfer.
  uint64_t total_bytes;
  // The number of bytes transferred so far.
  uint64_t bytes_transferred;
};

// Bandwidth quality of a connection.
enum class BandwidthQuality {
  // Unknown connection quality.
  kUnknown,
  // Low quality, e.g. connected via NFC or BLE.
  kLow,
  // Medium quality, e.g. connected via Bluetooth Classic.
  kMedium,
  // High quality, e.g. connected via WebRTC or Wi-Fi LAN.
  kHigh,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class Medium {
  kUnknown = 0,
  kMdns = 1,
  kBluetooth = 2,
  kWifiHotspot = 3,
  kBle = 4,
  kWifiLan = 5,
  kWifiAware = 6,
  kNfc = 7,
  kWifiDirect = 8,
  kWebRtc = 9,
  kBleL2Cap = 10,
};

// Log severity levels. This is passed as a member of
// NearbyConnectionsDependencies to set the minimum log level in the Nearby
// Connections library. Entries should be kept in sync with the values in
// nearby::sharing::api::LogMessage::Severity.
enum class LogSeverity {
  kVerbose = -1,
  kInfo = 0,
  kWarning = 1,
  kError = 2,
  kFatal = 3,
};

enum class DistanceInfo {
  kUnknown = 1,
  kVeryClose = 2,
  kClose = 3,
  kFar = 4,
};

struct InputFile {
  InputFile() = default;
  explicit InputFile(std::string path) {
    this->path = std::filesystem::u8path(path);
  }

  std::filesystem::path path;
};

// A simple payload containing raw bytes.
struct BytesPayload {
  // The bytes of this payload.
  std::vector<uint8_t> bytes;
};

// A file payload representing a file.
struct FilePayload {
  // The file to which this payload points to. When sending this payload, the
  // NearbyConnections library reads from this file. When receiving a file
  // payload it writes to this file.
  InputFile file;
  int64_t size;
  std::string parent_folder;
};

// Union of all supported payload types.
struct PayloadContent {
  // A Payload consisting of a single byte array.
  BytesPayload bytes_payload;
  // A Payload representing a file on the device.
  FilePayload file_payload;
  enum class Type { kUnknown = 0, kBytes = 1, kStream = 2, kFile = 3 };
  Type type;
  bool is_bytes() const { return type == Type::kBytes; }
  bool is_file() const { return type == Type::kFile; }
  bool is_stream() const { return type == Type::kStream; }
};

// A Payload sent between devices. Payloads sent with a particular content type
// will be received as that same type on the other device, e.g. the content for
// a Payload of type BytesPayload must be received by reading from the bytes
// field returned by Payload::content::bytes.
struct Payload {
  // A unique identifier for this payload. Generated by the sender of the
  // payload and used to keep track of the transfer progress.
  int64_t id;
  // The content of this payload which is one of multiple types, see
  // PayloadContent for all possible types.
  PayloadContent content;

  Payload() = default;
  explicit Payload(std::vector<uint8_t> bytes)
      : Payload(GenerateId(), std::move(bytes)) {}

  explicit Payload(InputFile file,
                   absl::string_view parent_folder = absl::string_view()) {
    id = std::hash<std::string>()(GetCompatibleU8String(file.path.u8string()));

    content.type = PayloadContent::Type::kFile;
    std::optional<uintmax_t> size = GetFileSize(file.path);
    if (size.has_value()) {
      content.file_payload.size = *size;
    }

    content.file_payload.file = std::move(file);
    content.file_payload.parent_folder = std::string(parent_folder);
  }

  Payload(int64_t id, std::vector<uint8_t> bytes) : id(id) {
    content.type = PayloadContent::Type::kBytes;
    content.bytes_payload.bytes = std::move(bytes);
  }

  Payload(int64_t id, InputFile file,
          absl::string_view parent_folder = absl::string_view())
      : id(id) {
    content.type = PayloadContent::Type::kFile;
    std::optional<uintmax_t> size = GetFileSize(file.path);
    if (size.has_value()) {
      content.file_payload.size = *size;
    }

    content.file_payload.file = std::move(file);
    content.file_payload.parent_folder = std::string(parent_folder);
  }

  Payload(char* bytes, int size)
      : Payload(GenerateId(), std::vector<uint8_t>(bytes, bytes + size)) {}

  int64_t GenerateId() {
    absl::BitGen bitgen;
    return absl::Uniform<int64_t>(absl::IntervalOpenClosed, bitgen, 0,
                                  std::numeric_limits<int64_t>::max());
  }
};

// This is a bitmask field that determines the transport type to upgrade to.
enum class TransportType {
  kAny = 0,
  // Allows use of WiFi Hotspot for connection when Wifi LAN is not connected.
  kNonDisruptive = 1,
  // Allows use of Wifi Hotspot for connection.
  kHighQuality = 2,
  // kNonDisruptive | kHighQuality
  kHighQualityNonDisruptive = 3,
};

// Returns true if all flags in `mask` are enabled in `transport_type`.
inline bool IsTransportTypeFlagsSet(TransportType transport_type,
                                    TransportType mask) {
  return (static_cast<int>(transport_type) &
          static_cast<int>(mask)) == static_cast<int>(mask);
}

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_TYPES_H_
