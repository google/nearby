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

#ifndef CORE_INTERNAL_BASE_PCP_HANDLER_H_
#define CORE_INTERNAL_BASE_PCP_HANDLER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "securegcm/ukey2_handshake.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "connections/implementation/bwu_manager.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/encryption_runner.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/pcp.h"
#include "connections/implementation/pcp_handler.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/status.h"
#include "connections/v3/listeners.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/connection_info.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/future.h"
#include "internal/platform/prng.h"
#include "internal/platform/scheduled_executor.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace connections {

// Represents the WebRtc state that mediums are connectable or not.
enum class WebRtcState {
  kUndefined = 0,
  kConnectable = 1,
  kUnconnectable = 2,
};

// Annotations for methods that need to run on PCP handler thread.
// Use only in BasePcpHandler and derived classes.
#define RUN_ON_PCP_HANDLER_THREAD() \
  ABSL_EXCLUSIVE_LOCKS_REQUIRED(GetPcpHandlerThread())

// A base implementation of the PcpHandler interface that takes care of all
// bookkeeping and handshake protocols that are common across all PcpHandler
// implementations -- thus, every concrete PcpHandler implementation must extend
// this class, so that they can focus exclusively on the medium-specific
// operations.
class BasePcpHandler : public PcpHandler,
                       public EndpointManager::FrameProcessor {
 public:
  using FrameProcessor = EndpointManager::FrameProcessor;

  BasePcpHandler(Mediums* mediums, EndpointManager* endpoint_manager,
                 EndpointChannelManager* channel_manager,
                 BwuManager* bwu_manager, Pcp pcp);
  ~BasePcpHandler() override;
  BasePcpHandler(BasePcpHandler&&) = delete;
  BasePcpHandler& operator=(BasePcpHandler&&) = delete;

  std::pair<Status, std::vector<ConnectionInfoVariant>>
  StartListeningForIncomingConnections(
      ClientProxy* client, absl::string_view service_id,
      v3::ConnectionListeningOptions options,
      v3::ConnectionListener connection_listener) override;

  // Starts advertising. Once successfully started, changes ClientProxy's state.
  // Notifies ConnectionListener (info.listener) in case of any event.
  // See
  // cpp/core/listeners.h
  Status StartAdvertising(ClientProxy* client, const std::string& service_id,
                          const AdvertisingOptions& advertising_options,
                          const ConnectionRequestInfo& info) override;

  // Stops Advertising is active, and changes CLientProxy state,
  // otherwise does nothing.
  void StopAdvertising(ClientProxy* client) override;

  // Starts discovery of endpoints that may be advertising.
  // Updates ClientProxy state once discovery started.
  // DiscoveryListener will get called in case of any event.
  Status StartDiscovery(ClientProxy* client, const std::string& service_id,
                        const DiscoveryOptions& discovery_options,
                        const DiscoveryListener& listener) override;

  // Stops Discovery if it is active, and changes CLientProxy state,
  // otherwise does nothing.
  void StopDiscovery(ClientProxy* client) override;

  void InjectEndpoint(ClientProxy* client, const std::string& service_id,
                      const OutOfBandConnectionMetadata& metadata) override;

  ConnectionInfo FillConnectionInfo(
      ClientProxy* client, const ConnectionRequestInfo& info,
      const ConnectionOptions& connection_options);

  // Requests a newly discovered remote endpoint it to form a connection.
  // Updates state on ClientProxy.
  Status RequestConnection(
      ClientProxy* client, const std::string& endpoint_id,
      const ConnectionRequestInfo& info,
      const ConnectionOptions& connection_options) override;

  // Called by either party to accept connection on their part.
  // Until both parties call it, connection will not reach a data phase.
  // Updates state in ClientProxy.
  Status AcceptConnection(ClientProxy* client, const std::string& endpoint_id,
                          PayloadListener payload_listener) override;

  // Called by either party to reject connection on their part.
  // If either party does call it, connection will terminate.
  // Updates state in ClientProxy.
  Status RejectConnection(ClientProxy* client,
                          const std::string& endpoint_id) override;

  // @EndpointManagerReaderThread
  void OnIncomingFrame(location::nearby::connections::OfflineFrame& frame,
                       const std::string& endpoint_id, ClientProxy* client,
                       location::nearby::proto::connections::Medium medium,
                       analytics::PacketMetaData& packet_meta_data) override;

  // Called when an endpoint disconnects while we're waiting for both sides to
  // approve/reject the connection.
  // @EndpointManagerThread
  void OnEndpointDisconnect(ClientProxy* client, const std::string& service_id,
                            const std::string& endpoint_id,
                            CountDownLatch barrier) override;

  Pcp GetPcp() const override { return pcp_; }
  Strategy GetStrategy() const override { return strategy_; }
  void DisconnectFromEndpointManager();

 protected:
  // The result of a call to startAdvertisingImpl() or startDiscoveryImpl().
  struct StartOperationResult {
    Status status;
    // If success, the mediums on which we are now advertising/discovering, for
    // analytics.
    std::vector<location::nearby::proto::connections::Medium> mediums;
  };

  // Represents an endpoint that we've discovered. Typically, the implementation
  // will know how to connect to this endpoint if asked. (eg. It holds on to a
  // BluetoothDevice)
  //
  // NOTE(DiscoveredEndpoint):
  // Specific protocol is expected to derive from it, as follows:
  // struct ProtocolEndpoint : public DiscoveredEndpoint {
  //   ProtocolContext context;
  // };
  // Protocol then allocates instance with std::make_shared<ProtocolEndpoint>(),
  // and passes this instance to OnEndpointFound() method.
  // When calling OnEndpointLost(), protocol does not need to pass the same
  // instance (but it can if implementation desires to do so).
  // BasePcpHandler will hold on to the shared_ptr<DiscoveredEndpoint>.
  struct DiscoveredEndpoint {
    DiscoveredEndpoint(std::string endpoint_id, ByteArray endpoint_info,
                       std::string service_id,
                       location::nearby::proto::connections::Medium medium,
                       WebRtcState web_rtc_state)
        : endpoint_id(std::move(endpoint_id)),
          endpoint_info(std::move(endpoint_info)),
          service_id(std::move(service_id)),
          medium(medium),
          web_rtc_state(web_rtc_state) {}
    virtual ~DiscoveredEndpoint() = default;

    std::string endpoint_id;
    ByteArray endpoint_info;
    std::string service_id;
    location::nearby::proto::connections::Medium medium;
    WebRtcState web_rtc_state;
  };

  struct BluetoothEndpoint : public DiscoveredEndpoint {
    BluetoothEndpoint(DiscoveredEndpoint endpoint, BluetoothDevice device)
        : DiscoveredEndpoint(std::move(endpoint)),
          bluetooth_device(std::move(device)) {}

    BluetoothDevice bluetooth_device;
  };

  struct BleEndpoint : public BasePcpHandler::DiscoveredEndpoint {
    BleEndpoint(DiscoveredEndpoint endpoint, BlePeripheral peripheral)
        : DiscoveredEndpoint(std::move(endpoint)),
          ble_peripheral(std::move(peripheral)) {}

    BlePeripheral ble_peripheral;
  };

  struct BleV2Endpoint : public BasePcpHandler::DiscoveredEndpoint {
    BleV2Endpoint(DiscoveredEndpoint endpoint, BleV2Peripheral peripheral)
        : DiscoveredEndpoint(std::move(endpoint)),
          ble_peripheral(std::move(peripheral)) {}

    BleV2Peripheral ble_peripheral;
  };

  struct WifiLanEndpoint : public DiscoveredEndpoint {
    WifiLanEndpoint(DiscoveredEndpoint endpoint,
                    const NsdServiceInfo& service_info)
        : DiscoveredEndpoint(std::move(endpoint)), service_info(service_info) {}

    NsdServiceInfo service_info;
  };

  struct WebRtcEndpoint : public DiscoveredEndpoint {
    WebRtcEndpoint(DiscoveredEndpoint endpoint, mediums::WebrtcPeerId peer_id)
        : DiscoveredEndpoint(std::move(endpoint)),
          peer_id(std::move(peer_id)) {}

    mediums::WebrtcPeerId peer_id;
  };

  struct ConnectImplResult {
    location::nearby::proto::connections::Medium medium =
        location::nearby::proto::connections::Medium::UNKNOWN_MEDIUM;
    Status status = {Status::kError};
    std::unique_ptr<EndpointChannel> endpoint_channel;
  };

  void RunOnPcpHandlerThread(const std::string& name, Runnable runnable);

  BluetoothDevice GetRemoteBluetoothDevice(
      const std::string& remote_bluetooth_mac_address);

  void OnEndpointFound(ClientProxy* client,
                       std::shared_ptr<DiscoveredEndpoint> endpoint)
      RUN_ON_PCP_HANDLER_THREAD();

  void OnEndpointLost(ClientProxy* client, const DiscoveredEndpoint& endpoint)
      RUN_ON_PCP_HANDLER_THREAD();

  Exception OnIncomingConnection(
      ClientProxy* client, const ByteArray& remote_endpoint_info,
      std::unique_ptr<EndpointChannel> endpoint_channel,
      location::nearby::proto::connections::Medium
          medium);  // throws Exception::IO

  virtual bool HasOutgoingConnections(ClientProxy* client) const;
  virtual bool HasIncomingConnections(ClientProxy* client) const;

  virtual bool CanSendOutgoingConnection(ClientProxy* client) const;
  virtual bool CanReceiveIncomingConnection(ClientProxy* client) const;

  virtual StartOperationResult StartAdvertisingImpl(
      ClientProxy* client, const std::string& service_id,
      const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info,
      const AdvertisingOptions& advertising_options)
      RUN_ON_PCP_HANDLER_THREAD() = 0;

  virtual Status StopAdvertisingImpl(ClientProxy* client)
      RUN_ON_PCP_HANDLER_THREAD() = 0;

  virtual StartOperationResult StartDiscoveryImpl(
      ClientProxy* client, const std::string& service_id,
      const DiscoveryOptions& discovery_options)
      RUN_ON_PCP_HANDLER_THREAD() = 0;

  virtual Status StopDiscoveryImpl(ClientProxy* client)
      RUN_ON_PCP_HANDLER_THREAD() = 0;

  virtual StartOperationResult StartListeningForIncomingConnectionsImpl(
      ClientProxy* client_proxy, absl::string_view service_id,
      absl::string_view local_endpoint_id,
      v3::ConnectionListeningOptions options) RUN_ON_PCP_HANDLER_THREAD() = 0;

  virtual Status InjectEndpointImpl(ClientProxy* client,
                                    const std::string& service_id,
                                    const OutOfBandConnectionMetadata& metadata)
      RUN_ON_PCP_HANDLER_THREAD() = 0;

  virtual ConnectImplResult ConnectImpl(ClientProxy* client,
                                        DiscoveredEndpoint* endpoint)
      RUN_ON_PCP_HANDLER_THREAD() = 0;

  virtual std::vector<location::nearby::proto::connections::Medium>
  GetConnectionMediumsByPriority() = 0;
  virtual location::nearby::proto::connections::Medium
  GetDefaultUpgradeMedium() = 0;

  // Returns the first discovered endpoint for the given endpoint_id.
  DiscoveredEndpoint* GetDiscoveredEndpoint(const std::string& endpoint_id);

  // Returns a vector of discovered endpoints, sorted in order of decreasing
  // preference.
  std::vector<BasePcpHandler::DiscoveredEndpoint*> GetDiscoveredEndpoints(
      const std::string& endpoint_id);

  // Returns a vector of discovered endpoints that share a given Medium.
  std::vector<BasePcpHandler::DiscoveredEndpoint*> GetDiscoveredEndpoints(
      const location::nearby::proto::connections::Medium medium);

  // Start alarms for endpoints lost by their mediums. Used when updating
  // discovery options.
  void StartEndpointLostByMediumAlarms(
      ClientProxy* client, location::nearby::proto::connections::Medium medium);

  void StopEndpointLostByMediumAlarm(
      absl::string_view endpoint_id,
      location::nearby::proto::connections::Medium medium);

  // Returns a vector of ConnectionInfos generated from a StartOperationResult.
  std::vector<ConnectionInfoVariant> GetConnectionInfoFromResult(
      absl::string_view service_id, StartOperationResult result);

  mediums::WebrtcPeerId CreatePeerIdFromAdvertisement(
      const string& service_id, const string& endpoint_id,
      const ByteArray& endpoint_info);

  SingleThreadExecutor* GetPcpHandlerThread()
      ABSL_LOCK_RETURNED(serial_executor_) {
    return &serial_executor_;
  }

  // Test only.
  absl::flat_hash_map<std::string, std::unique_ptr<CancelableAlarm>>&
  GetEndpointLostByMediumAlarms() {
    return endpoint_lost_by_medium_alarms_;
  }

  Mediums* mediums_;
  EndpointManager* endpoint_manager_;
  EndpointChannelManager* channel_manager_;
  AtomicBoolean stop_{false};

 private:
  struct PendingConnectionInfo {
    PendingConnectionInfo() = default;
    PendingConnectionInfo(PendingConnectionInfo&& other) = default;
    PendingConnectionInfo& operator=(PendingConnectionInfo&&) = default;
    ~PendingConnectionInfo();

    // Passes crypto context that we acquired in DH session for temporary
    // ownership here.
    void SetCryptoContext(std::unique_ptr<securegcm::UKey2Handshake> ukey2);

    // Pass Accept notification to client.
    void LocalEndpointAcceptedConnection(const std::string& endpoint_id,
                                         PayloadListener payload_listener);

    // Pass Reject notification to client.
    void LocalEndpointRejectedConnection(const std::string& endpoint_id);

    // Client state tracker to report events to. Never changes. Always valid.
    ClientProxy* client = nullptr;
    // Peer endpoint info, or empty, if not discovered yet. May change.
    ByteArray remote_endpoint_info;
    std::int32_t nonce = 0;
    bool is_incoming = false;
    absl::Time start_time{absl::InfinitePast()};
    // Client callbacks. Always valid.
    ConnectionListener listener;
    ConnectionOptions connection_options;

    // Only set for outgoing connections. If set, we must call
    // result->Set() when connection is established, or rejected.
    std::weak_ptr<Future<Status>> result;

    // Only (possibly) vector for incoming connections.
    std::vector<location::nearby::proto::connections::Medium> supported_mediums;

    // Keep track of a channel before we pass it to EndpointChannelManager.
    std::unique_ptr<EndpointChannel> channel;

    // Crypto context; initially empty; established first thing after channel
    // creation by running UKey2 session. While it is in progress, we keep track
    // of channel ourselves. Once it is done, we pass channel over to
    // EndpointChannelManager. We keep crypto context until connection is
    // accepted. Crypto context is passed over to channel_manager_ before
    // switching to connected state, where Payload may be exchanged.
    std::unique_ptr<securegcm::UKey2Handshake> ukey2;

    // Used in AnalyticsRecorder for devices connection tracking.
    std::string connection_token;
  };

  // @EncryptionRunnerThread
  // Called internally when DH session has negotiated a key successfully.
  void OnEncryptionSuccessImpl(const std::string& endpoint_id,
                               std::unique_ptr<securegcm::UKey2Handshake> ukey2,
                               const std::string& auth_token,
                               const ByteArray& raw_auth_token);

  // @EncryptionRunnerThread
  // Called internally when DH session was not able to negotiate a key.
  void OnEncryptionFailureImpl(const std::string& endpoint_id,
                               EndpointChannel* channel);

  EncryptionRunner::ResultListener GetResultListener();

  void OnEncryptionSuccessRunnable(
      const std::string& endpoint_id,
      std::unique_ptr<securegcm::UKey2Handshake> ukey2,
      const std::string& auth_token, const ByteArray& raw_auth_token);
  void OnEncryptionFailureRunnable(const std::string& endpoint_id,
                                   EndpointChannel* endpoint_channel);

  static Exception WriteConnectionRequestFrame(
      const ConnectionInfo& conection_info, EndpointChannel* endpoint_channel);
  static constexpr absl::Duration kConnectionRequestReadTimeout =
      absl::Seconds(2);
  static constexpr absl::Duration kRejectedConnectionCloseDelay =
      absl::Seconds(2);
  static constexpr int kConnectionTokenLength = 8;

  // Returns true if the new endpoint is preferred over the old endpoint.
  bool IsPreferred(const BasePcpHandler::DiscoveredEndpoint& new_endpoint,
                   const BasePcpHandler::DiscoveredEndpoint& old_endpoint);

  // Returns true, if connection party should respect the specified topology.
  bool ShouldEnforceTopologyConstraints(
      const AdvertisingOptions& local_advertising_options) const;

  // Returns true, if connection party should attempt to upgrade itself to
  // use a higher bandwidth medium, if it is available.
  bool AutoUpgradeBandwidth(
      const AdvertisingOptions& local_advertising_options) const;

  // Returns true if the incoming connection should be killed. This only
  // happens when an incoming connection arrives while we have an outgoing
  // connection to the same endpoint and we need to stop one connection.
  bool BreakTie(ClientProxy* client, const std::string& endpoint_id,
                std::int32_t incoming_nonce, EndpointChannel* channel);
  // We're not sure how far our outgoing connection has gotten. We may (or may
  // not) have called ClientProxy::OnConnectionInitiated. Therefore, we'll
  // call both preInit and preResult failures.
  void ProcessTieBreakLoss(ClientProxy* client, const std::string& endpoint_id,
                           PendingConnectionInfo* info);

  // Returns true if the bluetooth endpoint based on remote bluetooth mac
  // address is created and appended into discovered_endpoints_ with key
  // endpoint_id.
  bool AppendRemoteBluetoothMacAddressEndpoint(
      const std::string& endpoint_id,
      const std::string& remote_bluetooth_mac_address,
      const DiscoveryOptions& local_discovery_options);

  // Returns true if the webrtc endpoint is created and appended into
  // discovered_endpoints_ with key endpoint_id.
  bool AppendWebRTCEndpoint(const std::string& endpoint_id,
                            const DiscoveryOptions& local_discovery_options);

  void ProcessPreConnectionInitiationFailure(
      ClientProxy* client, Medium medium, const std::string& endpoint_id,
      EndpointChannel* channel, bool is_incoming, absl::Time start_time,
      Status status, Future<Status>* result);
  void ProcessPreConnectionResultFailure(ClientProxy* client,
                                         const std::string& endpoint_id);

  // Called when either side accepts/rejects the connection, but only takes
  // effect after both have accepted or one side has rejected.
  //
  // NOTE: We also take in a 'can_close_immediately' variable. This is because
  // any writes in transit are dropped when we close. To avoid having a reject
  // write being dropped (which causes the other side to report
  // onResult(DISCONNECTED) instead of onResult(REJECTED)), we delay our
  // close. If the other side behaves properly, we shouldn't even see the
  // delay (because they will also close the connection).
  void EvaluateConnectionResult(ClientProxy* client,
                                const std::string& endpoint_id,
                                bool can_close_immediately);

  ExceptionOr<location::nearby::connections::OfflineFrame>
  ReadConnectionRequestFrame(EndpointChannel* channel);

  // Returns an 8 characters length hashed string generated via a token byte
  // array.
  std::string GetHashedConnectionToken(const ByteArray& token_bytes);

  static void LogConnectionAttemptFailure(ClientProxy* client, Medium medium,
                                          const std::string& endpoint_id,
                                          bool is_incoming,
                                          absl::Time start_time,
                                          EndpointChannel* endpoint_channel);

  static void LogConnectionAttemptSuccess(
      const std::string& endpoint_id,
      const PendingConnectionInfo& connection_info);

  // Returns true if the client cancels the operation in progress through the
  // endpoint id. This is done by CancellationFlag.
  static bool Cancelled(ClientProxy* client, const std::string& endpoint_id);

  void WaitForLatch(const std::string& method_name, CountDownLatch* latch);
  Status WaitForResult(const std::string& method_name, std::int64_t client_id,
                       Future<Status>* future);
  bool MediumSupportedByClientOptions(
      const location::nearby::proto::connections::Medium& medium,
      const ConnectionOptions& connection_options) const;
  std::vector<location::nearby::proto::connections::Medium>
  GetSupportedConnectionMediumsByPriority(
      const ConnectionOptions& local_connection_option);
  std::string GetStringValueOfSupportedMediums(
      const ConnectionOptions& connection_options) const;
  std::string GetStringValueOfSupportedMediums(
      const AdvertisingOptions& advertising_options) const;
  std::string GetStringValueOfSupportedMediums(
      const DiscoveryOptions& discovery_options) const;
  void StripOutUnavailableMediums(AdvertisingOptions& advertising_options);
  void StripOutUnavailableMediums(DiscoveryOptions& discovery_options);

  // The endpoint id in high visibility mode is stable for 30 seconds, while in
  // low visibility mode it always rotates. We assume a client is trying to
  // rotate endpoint id when the advertising options is "low power" (3P) or
  // "disable Bluetooth classic" (1P).
  bool ShouldEnterHighVisibilityMode(
      const AdvertisingOptions& advertising_options);

  // Returns the intersection of supported mediums based on the mediums reported
  // by the remote client and the local client's advertising options.
  BooleanMediumSelector ComputeIntersectionOfSupportedMediums(
      const PendingConnectionInfo& connection_info);

  void OptionsAllowed(const BooleanMediumSelector& allowed,
                      std::ostringstream& result) const;

  ScheduledExecutor alarm_executor_;
  SingleThreadExecutor serial_executor_;

  // A map of endpoint id -> PendingConnectionInfo. Entries in this map imply
  // that there is an active connection to the endpoint and we're waiting for
  // both sides to accept before allowing payloads through. Once the fate of
  // the connection is decided (either accepted or rejected), it should be
  // removed from this map.
  absl::flat_hash_map<std::string, PendingConnectionInfo> pending_connections_;
  // A map of endpoint id -> DiscoveredEndpoint.
  absl::btree_multimap<std::string, std::shared_ptr<DiscoveredEndpoint>>
      discovered_endpoints_;
  // A map of endpoint id -> alarm. These alarms delay closing the
  // EndpointChannel to give the other side enough time to read the rejection
  // message. It's expected that the other side will close the connection
  // after reading the message (in which case, this alarm should be cancelled
  // as it's no longer needed), but this alarm is the fallback in case that
  // doesn't happen.
  absl::flat_hash_map<std::string, std::unique_ptr<CancelableAlarm>>
      pending_alarms_;

  // The active ClientProxy's connection lifecycle listener. Non-null while
  // advertising.
  ConnectionListener advertising_listener_;

  // Mapping from endpoint_id -> CancelableAlarm for triggering endpoint loss
  // while discovery options are updated.
  absl::flat_hash_map<std::string, std::unique_ptr<CancelableAlarm>>
      endpoint_lost_by_medium_alarms_;

  Pcp pcp_;
  Strategy strategy_{PcpToStrategy(pcp_)};
  EncryptionRunner encryption_runner_;
  BwuManager* bwu_manager_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_BASE_PCP_HANDLER_H_
