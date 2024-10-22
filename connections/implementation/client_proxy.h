// Copyright 2021 Google LLC
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

#ifndef CORE_INTERNAL_CLIENT_PROXY_H_
#define CORE_INTERNAL_CLIENT_PROXY_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "connections/v3/connection_listening_options.h"
#include "connections/v3/connections_device_provider.h"
#include "connections/v3/listeners.h"
#include "internal/analytics/event_logger.h"
#include "internal/interop/device.h"
#include "internal/interop/device_provider.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/error_code_recorder.h"
#include "internal/platform/mutex.h"
// Prefer using absl:: versions of a set and a map; they tend to be more
// efficient: implementation is using open-addressing hash tables.
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "internal/platform/os_name.h"
#include "internal/platform/scheduled_executor.h"

namespace nearby {
namespace connections {

// ClientProxy is tracking state of client's connection, and serves as
// a proxy for notifications sent to this client.
class ClientProxy final {
 public:
  static constexpr int kEndpointIdLength = 4;
  static constexpr absl::Duration
      kHighPowerAdvertisementEndpointIdCacheTimeout = absl::Seconds(30);

  explicit ClientProxy(
      ::nearby::analytics::EventLogger* event_logger = nullptr);
  ~ClientProxy();
  ClientProxy(ClientProxy&&) = default;
  ClientProxy& operator=(ClientProxy&&) = default;

  std::int64_t GetClientId() const;

  std::string GetLocalEndpointId();
  std::string GetLocalEndpointInfo() { return local_endpoint_info_; }

  analytics::AnalyticsRecorder& GetAnalyticsRecorder() const {
    return *analytics_recorder_;
  }

  std::string GetConnectionToken(const std::string& endpoint_id);
  std::optional<std::string> GetBluetoothMacAddress(
      const std::string& endpoint_id);
  void SetBluetoothMacAddress(const std::string& endpoint_id,
                              const std::string& bluetooth_mac_address);
  const NearbyDevice* GetLocalDevice();
  NearbyDeviceProvider* GetLocalDeviceProvider() {
    if (external_device_provider_ != nullptr) {
      return external_device_provider_;
    }
    return connections_device_provider_.get();
  }

  // Clears all the runtime state of this client.
  void Reset();

  // Marks this client as advertising with the given callbacks.
  void StartedAdvertising(
      const std::string& service_id, Strategy strategy,
      const ConnectionListener& connection_lifecycle_listener,
      absl::Span<location::nearby::proto::connections::Medium> mediums,
      const AdvertisingOptions& advertising_options = AdvertisingOptions{});
  // Marks this client as not advertising.
  void StoppedAdvertising();
  bool IsAdvertising() const;
  std::string GetAdvertisingServiceId() const;

  // Marks this client as listening for incoming connections.
  void StartedListeningForIncomingConnections(
      absl::string_view service_id, Strategy strategy,
      v3::ConnectionListener listener,
      const v3::ConnectionListeningOptions& options);
  void StoppedListeningForIncomingConnections();
  bool IsListeningForIncomingConnections() const;
  std::string GetListeningForIncomingConnectionsServiceId() const;

  ConnectionListener GetAdvertisingOrIncomingConnectionListener();

  // Marks this client as discovering with the given callback.
  void StartedDiscovery(
      const std::string& service_id, Strategy strategy,
      DiscoveryListener discovery_listener,
      absl::Span<location::nearby::proto::connections::Medium> mediums,
      const DiscoveryOptions& discovery_options = DiscoveryOptions{});
  // Marks this client as not discovering at all.
  void StoppedDiscovery();
  bool IsDiscoveringServiceId(const std::string& service_id) const;
  bool IsDiscovering() const;
  std::string GetDiscoveryServiceId() const;

  void UpdateLocalEndpointInfo(absl::string_view endpoint_info) {
    MutexLock lock(&mutex_);
    local_endpoint_info_ = std::string(endpoint_info);
  }

  void UpdateAdvertisingOptions(const AdvertisingOptions& advertising_options) {
    MutexLock lock(&mutex_);
    advertising_options_ = advertising_options;
  }

  void UpdateDiscoveryOptions(const DiscoveryOptions& discovery_options) {
    MutexLock lock(&mutex_);
    discovery_options_ = discovery_options;
  }

  // Proxies to the client's DiscoveryListener::OnEndpointFound() callback.
  void OnEndpointFound(const std::string& service_id,
                       const std::string& endpoint_id,
                       const ByteArray& endpoint_info,
                       location::nearby::proto::connections::Medium medium);
  // Proxies to the client's DiscoveryListener::OnEndpointLost() callback.
  void OnEndpointLost(const std::string& service_id,
                      const std::string& endpoint_id);

  // Triggered when client request connection to remote device.
  void OnRequestConnection(const Strategy& strategy,
                           const std::string& endpoint_id,
                           const ConnectionOptions& connection_options);

  // Proxies to the client's ConnectionListener::OnInitiated() callback.
  void OnConnectionInitiated(const std::string& endpoint_id,
                             const ConnectionResponseInfo& info,
                             const ConnectionOptions& connection_options,
                             const ConnectionListener& listener,
                             const std::string& connection_token);

  // Proxies to the client's ConnectionListener::OnAccepted() callback.
  void OnConnectionAccepted(const std::string& endpoint_id);
  // Proxies to the client's ConnectionListener::OnRejected() callback.
  void OnConnectionRejected(const std::string& endpoint_id,
                            const Status& status);

  void OnBandwidthChanged(const std::string& endpoint_id, Medium new_medium);

  // Removes the endpoint from this client's list of connected endpoints. If
  // notify is true, also calls the client's
  // ConnectionListener.disconnected_cb() callback.
  void OnDisconnected(const std::string& endpoint_id, bool notify);

  // Returns all mediums eligible for upgrade.
  BooleanMediumSelector GetUpgradeMediums(const std::string& endpoint_id) const;
  // Returns if this endpoint support 5G for WIFI.
  bool Is5GHzSupported(const std::string& endpoint_id) const;
  // Returns BSSID for this endpoint.
  std::string GetBssid(const std::string& endpoint_id) const;
  // Returns WIFI Frequency for this endpoint.
  std::int32_t GetApFrequency(const std::string& endpoint_id) const;
  // Returns IP Address in 4 bytes format for this endpoint.
  std::string GetIPAddress(const std::string& endpoint_id) const;
  // Returns true if it's safe to send payloads to this endpoint.
  bool IsConnectedToEndpoint(const std::string& endpoint_id) const;
  // Returns all endpoints that can safely be sent payloads.
  std::vector<std::string> GetConnectedEndpoints() const;
  // Returns all endpoints that are still awaiting acceptance.
  std::vector<std::string> GetPendingConnectedEndpoints() const;
  // Returns true if there is at least one connected connection or one pending
  // connection.
  bool HasOngoingConnection() const;
  // Returns the number of endpoints that are connected and outgoing.
  std::int32_t GetNumOutgoingConnections() const;
  // Returns the number of endpoints that are connected and incoming.
  std::int32_t GetNumIncomingConnections() const;
  // Returns true if endpoint is incoming connection.
  bool IsIncomingConnection(const std::string& endpoint_id) const;
  // Returns true if endpoint is outgoing connection.
  bool IsOutgoingConnection(const std::string& endpoint_id) const;
  // If true, then we're in the process of approving (or rejecting) a
  // connection. No payloads should be sent until isConnectedToEndpoint()
  // returns true.
  bool HasPendingConnectionToEndpoint(const std::string& endpoint_id) const;
  // Returns true if the local endpoint has already marked itself as
  // accepted/rejected.
  bool HasLocalEndpointResponded(const std::string& endpoint_id) const;
  // Returns true if the remote endpoint has already marked themselves as
  // accepted/rejected.
  bool HasRemoteEndpointResponded(const std::string& endpoint_id) const;
  // Marks the local endpoint as having accepted the connection.
  void LocalEndpointAcceptedConnection(const std::string& endpoint_id,
                                       PayloadListener listener);
  // Marks the local endpoint as having rejected the connection.
  void LocalEndpointRejectedConnection(const std::string& endpoint_id);
  // Marks the remote endpoint as having accepted the connection.
  void RemoteEndpointAcceptedConnection(const std::string& endpoint_id);
  // Marks the remote endpoint as having rejected the connection.
  void RemoteEndpointRejectedConnection(const std::string& endpoint_id);
  // Returns true if both the local endpoint and the remote endpoint have
  // accepted the connection.
  bool IsConnectionAccepted(const std::string& endpoint_id) const;
  // Returns true if either the local endpoint or the remote endpoint has
  // rejected the connection.
  bool IsConnectionRejected(const std::string& endpoint_id) const;
  // Returns true if the client should enforce topology constraints.
  bool ShouldEnforceTopologyConstraints() const;

  // Returns true, if connection party should attempt to upgrade itself to
  // use a higher bandwidth medium, if it is available.
  bool AutoUpgradeBandwidth() const;

  // Proxies to the client's PayloadListener::OnPayload() callback.
  void OnPayload(const std::string& endpoint_id, Payload payload);
  // Proxies to the client's PayloadListener::OnPayloadProgress() callback.
  void OnPayloadProgress(const std::string& endpoint_id,
                         const PayloadProgressInfo& info);
  bool LocalConnectionIsAccepted(std::string endpoint_id) const;
  bool RemoteConnectionIsAccepted(std::string endpoint_id) const;

  // Adds a CancellationFlag for endpoint id.
  void AddCancellationFlag(const std::string& endpoint_id);
  // Returns the CancellationFlag for endpoint id,
  CancellationFlag* GetCancellationFlag(const std::string& endpoint_id);
  // Sets the CancellationFlag to true for endpoint id.
  void CancelEndpoint(const std::string& endpoint_id);
  // Cancels all CancellationFlags.
  void CancelAllEndpoints();
  AdvertisingOptions GetAdvertisingOptions() const;
  DiscoveryOptions GetDiscoveryOptions() const;
  v3::ConnectionListeningOptions GetListeningOptions() const;

  // The endpoint id will be stable for 30 seconds after high visibility mode
  // (high power and Bluetooth Classic) advertisement stops.
  // If client re-enters high visibility mode within 30 seconds, he is going to
  // have the same endpoint id.
  void EnterHighVisibilityMode();
  // Cleans up any modifications in high visibility mode. The endpoint id always
  // rotates.
  void ExitHighVisibilityMode();

  // Enters stable endpoint ID mode.
  void EnterStableEndpointIdMode();
  // Cleans up any modifications in stable endpoint ID mode. The endpoint id
  // always rotates.
  void ExitStableEndpointIdMode();

  std::string Dump();

  const location::nearby::connections::OsInfo& GetLocalOsInfo() const;
  std::optional<location::nearby::connections::OsInfo> GetRemoteOsInfo(
      absl::string_view endpoint_id) const;
  void SetRemoteOsInfo(
      absl::string_view endpoint_id,
      const location::nearby::connections::OsInfo& remote_os_info);

  void RegisterDeviceProvider(NearbyDeviceProvider* provider) {
    external_device_provider_ = provider;
  }

  void RegisterConnectionsDeviceProvider(
      std::unique_ptr<v3::ConnectionsDeviceProvider> provider) {
    connections_device_provider_ = std::move(provider);
  }

  const bool& IsSupportSafeToDisconnect() const {
    return supports_safe_to_disconnect_;
  }

  bool IsSupportAutoReconnect() const { return support_auto_reconnect_; }

  const std::int32_t& GetLocalSafeToDisconnectVersion() const {
    return local_safe_to_disconnect_version_;
  }
  std::optional<std::int32_t> GetRemoteSafeToDisconnectVersion(
      absl::string_view endpoint_id) const;
  void SetRemoteSafeToDisconnectVersion(
      absl::string_view endpoint_id,
      const std::int32_t& safe_to_disconnect_version);
  bool IsSafeToDisconnectEnabled(absl::string_view endpoint_id);
  bool IsAutoReconnectEnabled(absl::string_view endpoint_id);
  bool IsPayloadReceivedAckEnabled(absl::string_view endpoint_id);

  // Returns the multiplex socket supports status for local device.
  std::int32_t GetLocalMultiplexSocketBitmask() const;
  // Sets the multiplex socket supports status for remote device.
  void SetRemoteMultiplexSocketBitmask(absl::string_view endpoint_id,
                                       int remote_multiplex_socket_bitmask);
  // Returns true if the multiplex socket is supported for the given medium.
  bool IsLocalMultiplexSocketSupported(Medium medium);

  // Gets the multiplex socket supports status for remote device.
  std::optional<std::int32_t> GetRemoteMultiplexSocketBitmask(
      absl::string_view endpoint_id) const;
  // Returns true if the multiplex socket is supported for the given medium.
  bool IsMultiplexSocketSupported(absl::string_view endpoint_id, Medium medium);

  // Gets the WebRTC non cellular network status.
  bool GetWebRtcNonCellular();

  // Sets the WebRTC non cellular network status.
  void SetWebRtcNonCellular(bool webrtc_non_cellular);

  /** Bitmask for bt multiplex connection support. */
  // Note. Deprecates the first and second bit of BT_MULTIPLEX_ENABLED and
  // WIFI_LAN_MULTIPLEX_ENABLED and shift them to the third and the forth bit.
  // The reason is we need to escape the (0, 1) bit which has been set in some
  // devices without salt enabled. If accompany with the devices with salted
  // enabled, the frames passed cannot be decrypted and the connection shall be
  // failed. Please refer to b/295925531#comment#14 for the details.
  enum MultiplexSocketBitmask : uint32_t {
    kBtMultiplexEnabled = 1 << 2,
    kWifiLanMultiplexEnabled = 1 << 3,
  };

 private:
  struct Connection {
    // Status: may be either:
    // Connection::PENDING, or combination of
    // Connection::LOCAL_ENDPOINT_ACCEPTED:
    // Connection::LOCAL_ENDPOINT_REJECTED and
    // Connection::REMOTE_ENDPOINT_ACCEPTED:
    // Connection::REMOTE_ENDPOINT_REJECTED, or
    // Connection::CONNECTED.
    // Only when this is set to CONNECTED should you allow payload transfers.
    //
    // We want this enum to be implicitly convertible to int, because
    // we perform bit operations on it.
    enum Status : uint8_t {
      kPending = 0,
      kLocalEndpointAccepted = 1 << 0,
      kLocalEndpointRejected = 1 << 1,
      kRemoteEndpointAccepted = 1 << 2,
      kRemoteEndpointRejected = 1 << 3,
      kConnected = 1 << 4,
    };
    bool is_incoming{false};
    Status status{kPending};
    ConnectionListener connection_listener;
    ConnectionOptions connection_options;
    DiscoveryOptions discovery_options;
    AdvertisingOptions advertising_options;
    std::string connection_token;
    std::optional<location::nearby::connections::OsInfo> os_info;
    std::int32_t safe_to_disconnect_version;
    std::int32_t remote_multiplex_socket_bitmask;
  };
  using ConnectionPair = std::pair<Connection, PayloadListener>;

  struct AdvertisingInfo {
    std::string service_id;
    ConnectionListener listener;
    void Clear() { service_id.clear(); }
    bool IsEmpty() const { return service_id.empty(); }
  };

  struct DiscoveryInfo {
    std::string service_id;
    DiscoveryListener listener;
    void Clear() { service_id.clear(); }
    bool IsEmpty() const { return service_id.empty(); }
  };

  struct ListeningInfo {
    std::string service_id;
    v3::ConnectionListener listener;
    void Clear() { service_id.clear(); }
    bool IsEmpty() const { return service_id.empty(); }
  };

  // `RemoveAllEndpoints` is expected to only be called during destruction of
  // ClientProxy via `ClientProxy::Reset`, which makes destroying
  // CancellationFlags safe here since we are destroying ClientProxy. Do not
  // use CancellationFlags after `RemoveAllEndpoints` is called, since all
  // flags now are referencing garbage memory.
  void RemoveAllEndpoints();

  void OnSessionComplete();
  bool ConnectionStatusesContains(const std::string& endpoint_id,
                                  Connection::Status status_to_match) const;
  void AppendConnectionStatus(const std::string& endpoint_id,
                              Connection::Status status_to_append);

  const ConnectionPair* LookupConnection(absl::string_view endpoint_id) const;
  ConnectionPair* LookupConnection(absl::string_view endpoint_id);
  bool ConnectionStatusMatches(const std::string& endpoint_id,
                               Connection::Status status) const;
  std::vector<std::string> GetMatchingEndpoints(
      absl::AnyInvocable<bool(const Connection&)> pred) const;
  std::string GenerateLocalEndpointId();

  void ScheduleClearCachedEndpointIdAlarm();
  void CancelClearCachedEndpointIdAlarm();

  location::nearby::connections::OsInfo::OsType OSNameToOsInfoType(
      api::OSName osName);

  std::string ToString(PayloadProgressInfo::Status status) const;

  mutable RecursiveMutex mutex_;
  std::int64_t client_id_;
  std::string local_endpoint_id_;
  std::string local_endpoint_info_;
  // If currently is advertising in high visibility mode is true: high power and
  // Bluetooth Classic enabled. When high_visibility_mode_ is true, the endpoint
  // id is stable for 30s. When high_visibility_mode_ is false, the endpoint id
  // always rotates.
  bool high_vis_mode_ = false;

  // If advertising is in stable endpoint ID mode, the endpoint ID is stable
  // for 30s after advertising or disconnection. When stable_endpoint_id_mode_
  // is false, the endpoint id always rotates.
  bool stable_endpoint_id_mode_ = false;

  // Caches the endpoint id for stable endpoint ID mode.
  std::string cached_endpoint_id_;

  ScheduledExecutor single_thread_executor_;
  std::unique_ptr<CancelableAlarm> cached_endpoint_id_alarm_;

  // If not empty, we are currently advertising and accepting connection
  // requests for the given service_id.
  AdvertisingInfo advertising_info_;

  // If not empty, we are currently discovering for the given service_id.
  DiscoveryInfo discovery_info_;

  // If not empty, we are currently listening for the given service_id.
  ListeningInfo listening_info_;

  // The active ClientProxy's advertising constraints. Empty()
  // returns true if the client hasn't started advertising false otherwise.
  // Note: this is not cleared when the client stops advertising because it
  // might still be useful downstream of advertising (eg: establishing
  // connections, performing bandwidth upgrades, etc.)
  AdvertisingOptions advertising_options_;

  // The active ClientProxy's discovery constraints. Null if the client
  // hasn't started discovering. Note: this is not cleared when the client
  // stops discovering because it might still be useful downstream of
  // discovery (eg: connection speed, etc.)
  DiscoveryOptions discovery_options_;

  // The active ClientProxy's listening constraints.
  v3::ConnectionListeningOptions listening_options_;

  // Maps endpoint_id to endpoint connection state.
  absl::flat_hash_map<std::string, ConnectionPair> connections_;

  // Maps endpoint_id to Bluetooth Mac Addresses.
  absl::flat_hash_map<std::string, std::string> bluetooth_mac_addresses_;

  // A cache of endpoint ids that we've already notified the discoverer of. We
  // check this cache before calling onEndpointFound() so that we don't notify
  // the client multiple times for the same endpoint. This would otherwise
  // happen because some mediums (like Bluetooth) repeatedly give us the same
  // endpoints after each scan.
  absl::flat_hash_set<std::string> discovered_endpoint_ids_;

  // Maps endpoint_id to CancellationFlag. CancellationFlags are passed around
  // as raw pointers to other classes in Nearby Connections, so it is important
  // that objects in this map are not cleared, even if they are cancelled.
  absl::flat_hash_map<std::string, std::unique_ptr<CancellationFlag>>
      cancellation_flags_;
  // A default cancellation flag with isCancelled set be true.
  std::unique_ptr<CancellationFlag> default_cancellation_flag_ =
      std::make_unique<CancellationFlag>(true);

  // An analytics logger with |EventLogger| provided by client, which is default
  // nullptr as no-op.
  std::unique_ptr<analytics::AnalyticsRecorder> analytics_recorder_;
  std::unique_ptr<ErrorCodeRecorder> error_code_recorder_;
  // Local device OS information.
  location::nearby::connections::OsInfo local_os_info_;
  // For device providers not owned by Nearby connections (e.g. Nearby
  // Presence's DeviceProvider.)
  NearbyDeviceProvider* external_device_provider_ = nullptr;
  // For Nearby Connections' own device provider.
  std::unique_ptr<v3::ConnectionsDeviceProvider> connections_device_provider_;
  bool supports_safe_to_disconnect_;
  bool support_auto_reconnect_;
  std::int32_t local_safe_to_disconnect_version_;
  // Allowed to use WebRTC over non-cellular networks.
  bool webrtc_non_cellular_ = false;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_CLIENT_PROXY_H_
