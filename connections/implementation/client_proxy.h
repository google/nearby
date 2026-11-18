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
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/analytics/operation_result_with_medium.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "connections/v3/connection_listening_options.h"
#include "connections/v3/connections_device_provider.h"
#include "connections/v3/listeners.h"
#include "internal/interop/device.h"
#include "internal/interop/device_provider.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/error_code_recorder.h"
#include "internal/platform/implementation/app_lifecycle_monitor.h"
#include "internal/platform/implementation/preferences_manager.h"
#include "internal/platform/scheduled_executor.h"

namespace nearby::connections {

// ClientProxy is tracking state of client's connection, and serves as
// a proxy for notifications sent to this client.
class ClientProxy final {
 public:
  static constexpr int kEndpointIdLength = 4;
  static constexpr absl::Duration
      kHighPowerAdvertisementEndpointIdCacheTimeout = absl::Seconds(30);

  explicit ClientProxy(std::unique_ptr<nearby::analytics::AnalyticsRecorder>
                           analytics_recorder = nullptr);
  ~ClientProxy();
  ClientProxy(const ClientProxy&) = delete;
  ClientProxy& operator=(const ClientProxy&) = delete;

  int64_t GetClientId() const { return client_id_; }

  std::string GetLocalEndpointId() ABSL_LOCKS_EXCLUDED(mutex_);
  std::string GetLocalEndpointInfo() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(mutex_);
    return local_endpoint_info_;
  }
  std::string GetLocalDeviceName() const { return local_device_name_; }

  // Override the base for received file attachments from a specific endpoint.
  // Returns true if the endpoint is found and the path is overridden.
  bool OverrideSavePath(absl::string_view endpoint_id, absl::string_view path)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Get the save path for a specific endpoint.  Returns empty string if
  // not set.
  std::string GetSavePath(absl::string_view endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);

  analytics::AnalyticsRecorder& GetAnalyticsRecorder() const {
    return *analytics_recorder_;
  }

  std::string GetConnectionToken(const std::string& endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  const NearbyDevice* GetLocalDevice() ABSL_LOCKS_EXCLUDED(mutex_);
  NearbyDeviceProvider* GetLocalDeviceProvider() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(mutex_);
    return GetLocalDeviceProviderLocked();
  }

  // Clears all the runtime state of this client.
  void Reset() ABSL_LOCKS_EXCLUDED(mutex_);

  // Marks this client as advertising with the given callbacks.
  void StartedAdvertising(
      const std::string& service_id, Strategy strategy,
      const ConnectionListener& connection_lifecycle_listener,
      absl::Span<location::nearby::proto::connections::Medium> mediums,
      const std::vector<analytics::OperationResultWithMedium>&
          operation_result_with_medium,
      const AdvertisingOptions& advertising_options = AdvertisingOptions{},
      std::optional<bool> is_extended_advertisement_supported = std::nullopt,
      std::optional<int> connected_ap_frequency = std::nullopt,
      std::optional<bool> is_nfc_available = std::nullopt)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Marks this client as not advertising.
  void StoppedAdvertising() ABSL_LOCKS_EXCLUDED(mutex_);
  bool IsAdvertising() const ABSL_LOCKS_EXCLUDED(mutex_);
  std::string GetAdvertisingServiceId() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Marks this client as listening for incoming connections.
  void StartedListeningForIncomingConnections(
      absl::string_view service_id, Strategy strategy,
      v3::ConnectionListener listener,
      const v3::ConnectionListeningOptions& options)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void StoppedListeningForIncomingConnections() ABSL_LOCKS_EXCLUDED(mutex_);
  bool IsListeningForIncomingConnections() const ABSL_LOCKS_EXCLUDED(mutex_);
  std::string GetListeningForIncomingConnectionsServiceId() const
      ABSL_LOCKS_EXCLUDED(mutex_);

  ConnectionListener GetAdvertisingOrIncomingConnectionListener()
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Marks this client as discovering with the given callback.
  void StartedDiscovery(
      const std::string& service_id, Strategy strategy,
      DiscoveryListener discovery_listener,
      absl::Span<location::nearby::proto::connections::Medium> mediums,
      const std::vector<analytics::OperationResultWithMedium>&
          operation_result_with_medium,
      const DiscoveryOptions& discovery_options = DiscoveryOptions{},
      std::optional<bool> is_extended_advertisement_supported = std::nullopt,
      std::optional<int> connected_ap_frequency = std::nullopt,
      std::optional<bool> is_nfc_available = std::nullopt)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Marks this client as not discovering at all.
  void StoppedDiscovery() ABSL_LOCKS_EXCLUDED(mutex_);
  bool IsDiscoveringServiceId(const std::string& service_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool IsDiscovering() const ABSL_LOCKS_EXCLUDED(mutex_);
  std::string GetDiscoveryServiceId() const ABSL_LOCKS_EXCLUDED(mutex_);

  void UpdateLocalEndpointInfo(absl::string_view endpoint_info)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(mutex_);
    local_endpoint_info_ = std::string(endpoint_info);
  }
  void UpdateAdvertisingOptions(const AdvertisingOptions& advertising_options)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void UpdateDiscoveryOptions(const DiscoveryOptions& discovery_options)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(mutex_);
    discovery_options_ = discovery_options;
  }

  // Proxies to the client's DiscoveryListener::OnEndpointFound() callback.
  void OnEndpointFound(const std::string& service_id,
                       const std::string& endpoint_id,
                       const ByteArray& endpoint_info,
                       location::nearby::proto::connections::Medium medium)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Proxies to the client's DiscoveryListener::OnEndpointLost() callback.
  void OnEndpointLost(const std::string& service_id,
                      const std::string& endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Triggered when client request connection to remote device.
  void OnRequestConnection(const Strategy& strategy,
                           const std::string& endpoint_id,
                           const ConnectionOptions& connection_options);

  // Proxies to the client's ConnectionListener::OnInitiated() callback.
  void OnConnectionInitiated(const std::string& endpoint_id,
                             const ConnectionResponseInfo& info,
                             const ConnectionOptions& connection_options,
                             const ConnectionListener& listener,
                             const std::string& connection_token)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Proxies to the client's ConnectionListener::OnAccepted() callback.
  void OnConnectionAccepted(const std::string& endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Proxies to the client's ConnectionListener::OnRejected() callback.
  void OnConnectionRejected(const std::string& endpoint_id,
                            const Status& status) ABSL_LOCKS_EXCLUDED(mutex_);

  void OnBandwidthChanged(const std::string& endpoint_id, Medium new_medium)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Removes the endpoint from this client's list of connected endpoints. If
  // notify is true, also calls the client's
  // ConnectionListener.disconnected_cb() callback.
  void OnDisconnected(const std::string& endpoint_id, bool notify)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the medium we're currently connected to the endpoint over, or
  // UNKNOWN if we don't know or don't have a connection.
  Medium GetConnectedMedium(const std::string& endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns all mediums eligible for upgrade.
  BooleanMediumSelector GetUpgradeMediums(const std::string& endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns if this endpoint support 5G for WIFI.
  bool Is5GHzSupported(const std::string& endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns BSSID for this endpoint.
  std::string GetBssid(const std::string& endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns WIFI Frequency for this endpoint.
  std::int32_t GetApFrequency(const std::string& endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns true if it's safe to send payloads to this endpoint.
  bool IsConnectedToEndpoint(const std::string& endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns all endpoints that can safely be sent payloads.
  std::vector<std::string> GetConnectedEndpoints() const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns all endpoints that are still awaiting acceptance.
  std::vector<std::string> GetPendingConnectedEndpoints() const
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::string GetLastLocalEndpointId() const ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns the number of endpoints that are connected and outgoing.
  std::int32_t GetNumOutgoingConnections() const ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns the number of endpoints that are connected and incoming.
  std::int32_t GetNumIncomingConnections() const ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns true if endpoint is incoming connection.
  bool IsIncomingConnection(const std::string& endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // If true, then we're in the process of approving (or rejecting) a
  // connection. No payloads should be sent until isConnectedToEndpoint()
  // returns true.
  bool HasPendingConnectionToEndpoint(const std::string& endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns true if the local endpoint has already marked itself as
  // accepted/rejected.
  bool HasLocalEndpointResponded(const std::string& endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns true if the remote endpoint has already marked themselves as
  // accepted/rejected.
  bool HasRemoteEndpointResponded(const std::string& endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Marks the local endpoint as having accepted the connection.
  void LocalEndpointAcceptedConnection(const std::string& endpoint_id,
                                       PayloadListener listener)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Marks the local endpoint as having rejected the connection.
  void LocalEndpointRejectedConnection(const std::string& endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Marks the remote endpoint as having accepted the connection.
  void RemoteEndpointAcceptedConnection(const std::string& endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Marks the remote endpoint as having rejected the connection.
  void RemoteEndpointRejectedConnection(const std::string& endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns true if both the local endpoint and the remote endpoint have
  // accepted the connection.
  bool IsConnectionAccepted(const std::string& endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns true if either the local endpoint or the remote endpoint has
  // rejected the connection.
  bool IsConnectionRejected(const std::string& endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns true if the client should enforce topology constraints.
  bool ShouldEnforceTopologyConstraints() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true, if connection party should attempt to upgrade itself to
  // use a higher bandwidth medium, if it is available.
  bool AutoUpgradeBandwidth() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Proxies to the client's PayloadListener::OnPayload() callback.
  void OnPayload(const std::string& endpoint_id, Payload payload)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Proxies to the client's PayloadListener::OnPayloadProgress() callback.
  void OnPayloadProgress(const std::string& endpoint_id,
                         const PayloadProgressInfo& info)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Adds a CancellationFlag for endpoint id.
  void AddCancellationFlag(const std::string& endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Returns the CancellationFlag for endpoint id,
  std::shared_ptr<CancellationFlag> GetCancellationFlag(
      const std::string& endpoint_id) ABSL_LOCKS_EXCLUDED(mutex_);
  // Sets the CancellationFlag to true for endpoint id.
  void CancelEndpoint(const std::string& endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Cancels all CancellationFlags.
  void CancelAllEndpoints() ABSL_LOCKS_EXCLUDED(mutex_);
  AdvertisingOptions GetAdvertisingOptions() const ABSL_LOCKS_EXCLUDED(mutex_);
  DiscoveryOptions GetDiscoveryOptions() const ABSL_LOCKS_EXCLUDED(mutex_);
  v3::ConnectionListeningOptions GetListeningOptions() const
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Enters stable endpoint ID mode.
  void EnterStableEndpointIdMode() ABSL_LOCKS_EXCLUDED(mutex_);
  // Cleans up any modifications in stable endpoint ID mode. The endpoint id
  // always rotates.
  void ExitStableEndpointIdMode() ABSL_LOCKS_EXCLUDED(mutex_);

  std::string Dump() ABSL_LOCKS_EXCLUDED(mutex_);

  location::nearby::connections::OsInfo GetLocalOsInfo() const
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::optional<location::nearby::connections::OsInfo> GetRemoteOsInfo(
      absl::string_view endpoint_id) const ABSL_LOCKS_EXCLUDED(mutex_);
  void SetLocalOsType(
      const location::nearby::connections::OsInfo::OsType& os_type)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void SetRemoteOsInfo(
      absl::string_view endpoint_id,
      const location::nearby::connections::OsInfo& remote_os_info)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void SetRemoteDeviceName(absl::string_view endpoint_id,
                           absl::string_view device_name)
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::string GetRemoteDeviceName(absl::string_view endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);

  void RegisterDeviceProvider(NearbyDeviceProvider* provider)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(mutex_);
    external_device_provider_ = provider;
  }

  // Registers a handler for events that require a decision from the client.
  // Replaces any previously registered handler. The handler must not call
  // `RegisterEventHandler()` or `UnregisterEventHandler()` synchronously from
  // within the callback, as doing so will deadlock.
  void RegisterEventHandler(ClientEventHandler handler)
      ABSL_LOCKS_EXCLUDED(event_handler_mutex_);

  // Unregisters the currently registered event handler and releases any
  // resources associated with it.
  void UnregisterEventHandler() ABSL_LOCKS_EXCLUDED(event_handler_mutex_);

  // Dispatches `event` to the registered handler and returns its decision.
  // Returns true if no handler is registered. Concurrent calls are serialized
  // by `event_handler_mutex_` (without holding `mutex_`) and block the calling
  // thread.
  bool OnEvent(const ClientEvent& event)
      ABSL_LOCKS_EXCLUDED(event_handler_mutex_);

  void SetRemoteSafeToDisconnectVersion(
      absl::string_view endpoint_id,
      const std::int32_t& safe_to_disconnect_version)
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool IsSafeToDisconnectEnabled(absl::string_view endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Gets the WebRTC non cellular network status.
  bool GetWebRtcNonCellular() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Sets the WebRTC non cellular network status.
  void SetWebRtcNonCellular(bool webrtc_non_cellular)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if DCT advertising/scanning is enabled.
  bool IsDctEnabled() const { return is_dct_enabled_; }

  // Gets the DCT dedup value. This is used to dedup the same device name when
  // scanning for multiple devices.
  // It is 7 bits derived from the local endpoint ID.
  uint8_t GetDctDedup() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Updates the DCT device name before advertising.
  void UpdateDctDeviceName(absl::string_view device_name)
      ABSL_LOCKS_EXCLUDED(mutex_);

  std::optional<location::nearby::connections::MediumRole> GetMediumRole(
      absl::string_view endpoint_id) const ABSL_LOCKS_EXCLUDED(mutex_);

  struct MediumsAvailability {
    bool is_wifi_direct_go_available = false;
    bool is_wifi_direct_gc_available = false;
    bool is_wifi_hotspot_ap_available = false;
    bool is_wifi_hotspot_client_available = false;
    bool is_wifi_aware_available = false;
  };

  location::nearby::connections::MediumRole GetLocalMediumRole(
      const MediumsAvailability& mediums_availability) const
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsUsingP2pMedium() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Forces client to regenerate a new local endpoint id.
  void ClearCachedLocalEndpointId() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  friend class ClientProxyTestPeer;

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
    Medium connected_medium{Medium::UNKNOWN_MEDIUM};
    Status status{kPending};
    ConnectionListener connection_listener;
    ConnectionOptions connection_options;
    std::string connection_token;
    std::optional<location::nearby::connections::OsInfo> os_info;
    std::optional<std::int32_t> safe_to_disconnect_version;
    std::string save_path;
    std::string device_name;
    std::shared_ptr<const PayloadListener> payload_listener;
  };

  struct AdvertisingInfo {
    std::string service_id;
    ConnectionListener listener;
    void Clear() {
      service_id.clear();
      listener = ConnectionListener{};
    }
    bool IsEmpty() const { return service_id.empty(); }
  };

  struct DiscoveryInfo {
    std::string service_id;
    std::shared_ptr<const DiscoveryListener> listener;
    void Clear() {
      service_id.clear();
      listener.reset();
    }
    bool IsEmpty() const { return service_id.empty(); }
  };

  struct ListeningInfo {
    std::string service_id;
    std::shared_ptr<const v3::ConnectionListener> listener;
    void Clear() {
      service_id.clear();
      listener.reset();
    }
    bool IsEmpty() const { return service_id.empty(); }
  };

  std::string GetLocalEndpointIdLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  NearbyDeviceProvider* GetLocalDeviceProviderLocked() const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    if (external_device_provider_ != nullptr) {
      return external_device_provider_;
    }
    return connections_device_provider_.get();
  }

  bool IsAdvertisingLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return !advertising_info_.IsEmpty();
  }
  bool IsListeningForIncomingConnectionsLocked() const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return !listening_info_.IsEmpty();
  }
  bool IsDiscoveringLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return !discovery_info_.IsEmpty();
  }
  bool IsDiscoveringServiceIdLocked(const std::string& service_id) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return IsDiscoveringLocked() && service_id == discovery_info_.service_id;
  }

  void StoppedAdvertisingLocked(
      std::vector<std::unique_ptr<CancelableAlarm>>& alarms_to_cancel,
      ConnectionListener& removed_advertising_listener)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void StoppedDiscoveryLocked(
      std::shared_ptr<const DiscoveryListener>& removed_discovery_listener)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void StoppedListeningForIncomingConnectionsLocked(
      std::shared_ptr<const v3::ConnectionListener>& removed_listening_listener)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // `RemoveAllEndpointsLocked` is expected to only be called during reset or
  // destruction of ClientProxy via `ClientProxy::Reset`, which makes clearing
  // CancellationFlags safe here.
  void RemoveAllEndpointsLocked(
      absl::flat_hash_map<std::string, Connection>& removed_connections,
      absl::flat_hash_map<std::string, std::shared_ptr<CancellationFlag>>&
          removed_flags) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void OnSessionCompleteLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool ConnectionStatusesContains(const std::string& endpoint_id,
                                  Connection::Status status_to_match) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void AppendConnectionStatus(const std::string& endpoint_id,
                              Connection::Status status_to_append)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  const Connection* LookupConnection(absl::string_view endpoint_id) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  Connection* LookupConnection(absl::string_view endpoint_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool IsConnectedToEndpointLocked(const std::string& endpoint_id) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool HasPendingConnectionToEndpointLocked(
      const std::string& endpoint_id) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool HasLocalEndpointRespondedLocked(const std::string& endpoint_id) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool HasRemoteEndpointRespondedLocked(const std::string& endpoint_id) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  std::vector<std::string> GetMatchingEndpoints(
      absl::AnyInvocable<bool(const Connection&)> pred) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  std::string GenerateLocalEndpointId() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Resets the local endpoint ID and sets the last local endpoint ID.
  void ResetLocalEndpointId() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  // Returns true if there is at least one connected connection or one pending
  // connection.
  bool HasOngoingConnection() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return !connections_.empty();
  }
  bool HasWifiDirectConnection() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool HasWifiHotspotConnection() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool HasWifiAwareConnection() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool IsUsingP2pMediumLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return HasWifiDirectConnection() || HasWifiHotspotConnection() ||
           HasWifiAwareConnection();
  }

  bool IsSupportSafeToDisconnect() const {
    return supports_safe_to_disconnect_;
  }
  std::optional<std::int32_t> GetRemoteSafeToDisconnectVersion(
      absl::string_view endpoint_id) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void AddCancellationFlagLocked(const std::string& endpoint_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  std::shared_ptr<CancellationFlag> GetCancellationFlagToCancelLocked(
      const std::string& endpoint_id) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void ExitStableEndpointIdModeLocked(
      std::vector<std::unique_ptr<CancelableAlarm>>& alarms_to_cancel)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  std::unique_ptr<CancelableAlarm> ScheduleClearCachedEndpointIdAlarmLocked()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  std::unique_ptr<CancelableAlarm> CancelClearCachedEndpointIdAlarmLocked()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void ClearCachedLocalEndpointIdForAlarm(uint64_t generation)
      ABSL_LOCKS_EXCLUDED(mutex_);

  std::optional<std::string> GetEndpointIdForDct() const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void InitializePreferencesManager() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void LoadClientInfoFromPreferences() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void SaveClientInfoToPreferences() ABSL_LOCKS_EXCLUDED(mutex_);

  // Immutable client configuration and services.
  const bool is_dct_enabled_;
  const int64_t client_id_;
  const std::string local_device_name_;
  const bool supports_safe_to_disconnect_;
  // An analytics logger with |EventLogger| provided by client, which is default
  // NoOpAnalyticsRecorder when constructed with nullptr.
  const std::unique_ptr<analytics::AnalyticsRecorder> analytics_recorder_;
  const std::unique_ptr<ErrorCodeRecorder> error_code_recorder_;
  // A default cancellation flag with isCancelled set to true.
  const std::shared_ptr<CancellationFlag> default_cancellation_flag_ =
      std::make_shared<CancellationFlag>(true);

  mutable absl::Mutex mutex_;
  // The device name used for DCT advertising.
  std::string dct_device_name_ ABSL_GUARDED_BY(mutex_);
  // The dedup value used for DCT advertising.
  uint8_t dct_dedup_ ABSL_GUARDED_BY(mutex_) = 0;
  // The endpoint ID used for DCT advertising.
  std::string dct_endpoint_id_ ABSL_GUARDED_BY(mutex_);

  std::string local_endpoint_id_ ABSL_GUARDED_BY(mutex_);
  std::string local_endpoint_info_ ABSL_GUARDED_BY(mutex_);
  std::string last_local_endpoint_id_ ABSL_GUARDED_BY(mutex_);

  // If advertising is in stable endpoint ID mode, the endpoint ID is stable
  // for 30s after advertising or disconnection. When stable_endpoint_id_mode_
  // is false, the endpoint id always rotates.
  bool stable_endpoint_id_mode_ ABSL_GUARDED_BY(mutex_) = false;

  // Caches the endpoint id for stable endpoint ID mode.
  std::string cached_endpoint_id_ ABSL_GUARDED_BY(mutex_);

  ScheduledExecutor single_thread_executor_;
  uint64_t cached_endpoint_id_alarm_generation_ ABSL_GUARDED_BY(mutex_) = 0;
  std::unique_ptr<CancelableAlarm> cached_endpoint_id_alarm_
      ABSL_GUARDED_BY(mutex_);

  // If not empty, we are currently advertising and accepting connection
  // requests for the given service_id.
  AdvertisingInfo advertising_info_ ABSL_GUARDED_BY(mutex_);

  // If not empty, we are currently discovering for the given service_id.
  DiscoveryInfo discovery_info_ ABSL_GUARDED_BY(mutex_);

  // If not empty, we are currently listening for the given service_id.
  ListeningInfo listening_info_ ABSL_GUARDED_BY(mutex_);

  // The active ClientProxy's advertising constraints. Empty()
  // returns true if the client hasn't started advertising false otherwise.
  // Note: this is not cleared when the client stops advertising because it
  // might still be useful downstream of advertising (eg: establishing
  // connections, performing bandwidth upgrades, etc.)
  AdvertisingOptions advertising_options_ ABSL_GUARDED_BY(mutex_);

  // The active ClientProxy's discovery constraints. Null if the client
  // hasn't started discovering. Note: this is not cleared when the client
  // stops discovering because it might still be useful downstream of
  // discovery (eg: connection speed, etc.)
  DiscoveryOptions discovery_options_ ABSL_GUARDED_BY(mutex_);

  // The active ClientProxy's listening constraints.
  v3::ConnectionListeningOptions listening_options_ ABSL_GUARDED_BY(mutex_);

  // Maps endpoint_id to endpoint connection state.
  absl::flat_hash_map<std::string, Connection> connections_
      ABSL_GUARDED_BY(mutex_);

  // A cache of endpoint ids that we've already notified the discoverer of. We
  // check this cache before calling onEndpointFound() so that we don't notify
  // the client multiple times for the same endpoint. This would otherwise
  // happen because some mediums (like Bluetooth) repeatedly give us the same
  // endpoints after each scan.
  absl::flat_hash_set<std::string> discovered_endpoint_ids_
      ABSL_GUARDED_BY(mutex_);

  // Maps endpoint_id to CancellationFlag. CancellationFlags are passed around
  // as raw pointers to other classes in Nearby Connections, so it is important
  // that objects in this map are not cleared, even if they are cancelled.
  absl::flat_hash_map<std::string, std::shared_ptr<CancellationFlag>>
      cancellation_flags_ ABSL_GUARDED_BY(mutex_);

  // A preferences manager for storing client-specific preferences.
  std::unique_ptr<nearby::api::PreferencesManager> preferences_manager_
      ABSL_GUARDED_BY(mutex_);

  // Local device OS information.
  location::nearby::connections::OsInfo local_os_info_ ABSL_GUARDED_BY(mutex_);
  // For device providers not owned by Nearby connections (e.g. Nearby
  // Presence's DeviceProvider.)
  NearbyDeviceProvider* external_device_provider_ ABSL_GUARDED_BY(mutex_) =
      nullptr;
  // For Nearby Connections' own device provider.
  std::unique_ptr<v3::ConnectionsDeviceProvider> connections_device_provider_
      ABSL_GUARDED_BY(mutex_);
  // Allowed to use WebRTC over non-cellular networks.
  bool webrtc_non_cellular_ ABSL_GUARDED_BY(mutex_) = false;

  // Mutex serializing registration and invocation of `event_handler_`. Kept
  // separate from `mutex_` so `OnEvent` does not block other `ClientProxy`
  // operations or deadlock if the handler queries `ClientProxy`.
  mutable absl::Mutex event_handler_mutex_;
  // Handler for events that require a decision from the client.
  ClientEventHandler event_handler_ ABSL_GUARDED_BY(event_handler_mutex_);

  // Declared last so background lifecycle callbacks stop before other members
  // are destroyed.
  std::unique_ptr<api::AppLifecycleMonitor> app_lifecycle_monitor_;
};

}  // namespace nearby::connections

#endif  // CORE_INTERNAL_CLIENT_PROXY_H_
