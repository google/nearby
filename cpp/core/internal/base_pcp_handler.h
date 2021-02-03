#ifndef CORE_INTERNAL_BASE_PCP_HANDLER_H_
#define CORE_INTERNAL_BASE_PCP_HANDLER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/internal/bwu_manager.h"
#include "core/internal/client_proxy.h"
#include "core/internal/encryption_runner.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/mediums/mediums.h"
#include "core/internal/mediums/webrtc.h"
#include "core/internal/pcp.h"
#include "core/internal/pcp_handler.h"
#include "core/listeners.h"
#include "core/options.h"
#include "core/status.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/base/byte_array.h"
#include "platform/base/prng.h"
#include "platform/public/atomic_boolean.h"
#include "platform/public/atomic_reference.h"
#include "platform/public/cancelable_alarm.h"
#include "platform/public/count_down_latch.h"
#include "platform/public/future.h"
#include "platform/public/scheduled_executor.h"
#include "platform/public/single_thread_executor.h"
#include "platform/public/system_clock.h"
#include "proto/connections_enums.pb.h"
#include "securegcm/d2d_connection_context_v1.h"
#include "securegcm/ukey2_handshake.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace connections {

// Represents the WebRtc state that mediums are connectable or not.
enum class WebRtcState {
  kUndefined = 0,
  kConnectable = 1,
  kUnconnectable = 2,
};

// A base implementation of the PcpHandler interface that takes care of all
// bookkeeping and handshake protocols that are common across all PcpHandler
// implementations -- thus, every concrete PcpHandler implementation must extend
// this class, so that they can focus exclusively on the medium-specific
// operations.
class BasePcpHandler : public PcpHandler,
                       public EndpointManager::FrameProcessor {
 public:
  using FrameProcessor = EndpointManager::FrameProcessor;

  // TODO(apolyudov): Add SecureRandom.
  BasePcpHandler(Mediums* mediums, EndpointManager* endpoint_manager,
                 EndpointChannelManager* channel_manager,
                 BwuManager* bwu_manager, Pcp pcp);
  ~BasePcpHandler() override;
  BasePcpHandler(BasePcpHandler&&) = delete;
  BasePcpHandler& operator=(BasePcpHandler&&) = delete;

  // Starts advertising. Once successfully started, changes ClientProxy's state.
  // Notifies ConnectionListener (info.listener) in case of any event.
  // See
  // https://source.corp.google.com/piper///depot/google3/core/listeners.h;l=78
  Status StartAdvertising(ClientProxy* client, const std::string& service_id,
                          const ConnectionOptions& options,
                          const ConnectionRequestInfo& info) override;

  // Stops Advertising is active, and changes CLientProxy state,
  // otherwise does nothing.
  void StopAdvertising(ClientProxy* client) override;

  // Starts discovery of endpoints that may be advertising.
  // Updates ClientProxy state once discovery started.
  // DiscoveryListener will get called in case of any event.
  Status StartDiscovery(ClientProxy* client, const std::string& service_id,
                        const ConnectionOptions& options,
                        const DiscoveryListener& listener) override;

  // Stops Discovery if it is active, and changes CLientProxy state,
  // otherwise does nothing.
  void StopDiscovery(ClientProxy* client) override;

  void InjectEndpoint(ClientProxy* client, const std::string& service_id,
                      const OutOfBandConnectionMetadata& metadata) override;

  // Requests a newly discovered remote endpoint it to form a connection.
  // Updates state on ClientProxy.
  Status RequestConnection(ClientProxy* client, const std::string& endpoint_id,
                           const ConnectionRequestInfo& info,
                           const ConnectionOptions& options) override;

  // Called by either party to accept connection on their part.
  // Until both parties call it, connection will not reach a data phase.
  // Updates state in ClientProxy.
  Status AcceptConnection(ClientProxy* client, const std::string& endpoint_id,
                          const PayloadListener& payload_listener) override;

  // Called by either party to reject connection on their part.
  // If either party does call it, connection will terminate.
  // Updates state in ClientProxy.
  Status RejectConnection(ClientProxy* client,
                          const std::string& endpoint_id) override;

  // @EndpointManagerReaderThread
  void OnIncomingFrame(OfflineFrame& frame, const std::string& endpoint_id,
                       ClientProxy* client,
                       proto::connections::Medium medium) override;

  // Called when an endpoint disconnects while we're waiting for both sides to
  // approve/reject the connection.
  // @EndpointManagerThread
  void OnEndpointDisconnect(ClientProxy* client, const std::string& endpoint_id,
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
    std::vector<proto::connections::Medium> mediums;
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
                       proto::connections::Medium medium,
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
    proto::connections::Medium medium;
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

  struct WifiLanEndpoint : public DiscoveredEndpoint {
    WifiLanEndpoint(DiscoveredEndpoint endpoint, WifiLanService service)
        : DiscoveredEndpoint(std::move(endpoint)),
          wifi_lan_service(std::move(service)) {}

    WifiLanService wifi_lan_service;
  };

  struct WebRtcEndpoint : public DiscoveredEndpoint {
    WebRtcEndpoint(DiscoveredEndpoint endpoint, mediums::PeerId peer_id)
        : DiscoveredEndpoint(std::move(endpoint)),
          peer_id(std::move(peer_id)) {}

    mediums::PeerId peer_id;
  };

  struct ConnectImplResult {
    proto::connections::Medium medium =
        proto::connections::Medium::UNKNOWN_MEDIUM;
    Status status = {Status::kError};
    std::unique_ptr<EndpointChannel> endpoint_channel;
  };

  void RunOnPcpHandlerThread(Runnable runnable);

  BluetoothDevice GetRemoteBluetoothDevice(
      const std::string& remote_bluetooth_mac_address);

  ConnectionOptions GetConnectionOptions() const;
  ConnectionOptions GetDiscoveryOptions() const;

  // @PcpHandlerThread
  void OnEndpointFound(ClientProxy* client,
                       std::shared_ptr<DiscoveredEndpoint> endpoint);

  // @PcpHandlerThread
  void OnEndpointLost(ClientProxy* client, const DiscoveredEndpoint& endpoint);

  Exception OnIncomingConnection(
      ClientProxy* client, const ByteArray& remote_endpoint_info,
      std::unique_ptr<EndpointChannel> endpoint_channel,
      proto::connections::Medium medium);  // throws Exception::IO

  virtual bool HasOutgoingConnections(ClientProxy* client) const;
  virtual bool HasIncomingConnections(ClientProxy* client) const;

  virtual bool CanSendOutgoingConnection(ClientProxy* client) const;
  virtual bool CanReceiveIncomingConnection(ClientProxy* client) const;

  // @PcpHandlerThread
  virtual StartOperationResult StartAdvertisingImpl(
      ClientProxy* client, const std::string& service_id,
      const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info,
      const ConnectionOptions& options) = 0;
  // @PcpHandlerThread
  virtual Status StopAdvertisingImpl(ClientProxy* client) = 0;

  // @PcpHandlerThread
  virtual StartOperationResult StartDiscoveryImpl(
      ClientProxy* client, const std::string& service_id,
      const ConnectionOptions& options) = 0;
  // @PcpHandlerThread
  virtual Status StopDiscoveryImpl(ClientProxy* client) = 0;

  // @PcpHandlerThread
  virtual Status InjectEndpointImpl(
      ClientProxy* client, const std::string& service_id,
      const OutOfBandConnectionMetadata& metadata) = 0;

  // @PcpHandlerThread
  virtual ConnectImplResult ConnectImpl(ClientProxy* client,
                                        DiscoveredEndpoint* endpoint) = 0;

  virtual std::vector<proto::connections::Medium>
  GetConnectionMediumsByPriority() = 0;
  virtual proto::connections::Medium GetDefaultUpgradeMedium() = 0;

  // Returns the first discovered endpoint for the given endpoint_id.
  DiscoveredEndpoint* GetDiscoveredEndpoint(const std::string& endpoint_id);

  // Returns a vector of discovered endpoints, sorted in order of decreasing
  // preference.
  std::vector<BasePcpHandler::DiscoveredEndpoint*> GetDiscoveredEndpoints(
      const std::string& endpoint_id);

  // Returns a vector of discovered endpoints that share a given Medium.
  std::vector<BasePcpHandler::DiscoveredEndpoint*> GetDiscoveredEndpoints(
      const proto::connections::Medium medium);

  mediums::PeerId CreatePeerIdFromAdvertisement(const string& service_id,
                                                const string& endpoint_id,
                                                const ByteArray& endpoint_info);

  Mediums* mediums_;
  EndpointManager* endpoint_manager_;
  EndpointChannelManager* channel_manager_;

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
    void LocalEndpointAcceptedConnection(
        const std::string& endpoint_id,
        const PayloadListener& payload_listener);

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
    ConnectionOptions options;

    // Only set for outgoing connections. If set, we must call
    // result->Set() when connection is established, or rejected.
    std::weak_ptr<Future<Status>> result;

    // Only (possibly) vector for incoming connections.
    std::vector<proto::connections::Medium> supported_mediums;

    // Keep track of a channel before we pass it to EndpointChannelManager.
    std::unique_ptr<EndpointChannel> channel;

    // Crypto context; initially empty; established first thing after channel
    // creation by running UKey2 session. While it is in progress, we keep track
    // of channel ourselves. Once it is done, we pass channel over to
    // EndpointChannelManager. We keep crypto context until connection is
    // accepted. Crypto context is passed over to channel_manager_ before
    // switching to connected state, where Payload may be exchanged.
    std::unique_ptr<securegcm::UKey2Handshake> ukey2;
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
      EndpointChannel* endpoint_channel, const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info, std::int32_t nonce,
      const std::vector<proto::connections::Medium>& supported_mediums);

  static constexpr absl::Duration kConnectionRequestReadTimeout =
      absl::Seconds(2);
  static constexpr absl::Duration kRejectedConnectionCloseDelay =
      absl::Seconds(2);

  void OnConnectionResponse(ClientProxy* client, const std::string& endpoint_id,
                            const OfflineFrame& frame);

  // Returns true if the new endpoint is preferred over the old endpoint.
  bool IsPreferred(const BasePcpHandler::DiscoveredEndpoint& new_endpoint,
                   const BasePcpHandler::DiscoveredEndpoint& old_endpoint);

  // Returns true, if connection party should respect the specified topology.
  bool ShouldEnforceTopologyConstraints(
      const ConnectionOptions& local_advertising_options) const;

  // Returns true, if connection party should attempt to upgrade itself to
  // use a higher bandwidth medium, if it is available.
  bool AutoUpgradeBandwidth(
      const ConnectionOptions& local_advertising_options) const;

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

  // Called when an incoming connection has been accepted by both sides.
  //
  // @param client_proxy The client
  // @param endpoint_id The id of the remote device
  // @param supported_mediums The mediums supported by the remote device.
  // Empty
  //        for outgoing connections and older devices that don't report their
  //        supported mediums.
  void InitiateBandwidthUpgrade(
      ClientProxy* client, const std::string& endpoint_id,
      const std::vector<proto::connections::Medium>& supported_mediums);

  // Returns the optimal medium supported by both devices.
  proto::connections::Medium ChooseBestUpgradeMedium(
      const std::vector<proto::connections::Medium>& supported_mediums,
      const ConnectionOptions& local_advertising_options);

  // Returns true if the bluetooth endpoint based on remote bluetooth mac
  // address is created and appended into discovered_endpoints_ with key
  // endpoint_id.
  bool AppendRemoteBluetoothMacAddressEndpoint(
      const std::string& endpoint_id,
      const std::string& remote_bluetooth_mac_address,
      const ConnectionOptions& local_discovery_options);

  // Returns true if the webrtc endpoint is created and appended into
  // discovered_endpoints_ with key endpoint_id.
  bool AppendWebRTCEndpoint(const std::string& endpoint_id,
                            const ConnectionOptions& local_discovery_options);

  void ProcessPreConnectionInitiationFailure(const std::string& endpoint_id,
                                             EndpointChannel* channel,
                                             Status status,
                                             Future<Status>* result);
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

  ExceptionOr<OfflineFrame> ReadConnectionRequestFrame(
      EndpointChannel* channel);

  void WaitForLatch(const std::string& method_name, CountDownLatch* latch);
  Status WaitForResult(const std::string& method_name, std::int64_t client_id,
                       Future<Status>* future);
  bool MediumSupportedByClientOptions(
      const proto::connections::Medium& medium,
      const ConnectionOptions& client_options) const;
  std::vector<proto::connections::Medium>
  GetSupportedConnectionMediumsByPriority(
      const ConnectionOptions& local_option);
  std::string GetStringValueOfSupportedMediums(
      const ConnectionOptions& options) const;

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
  absl::flat_hash_map<std::string, CancelableAlarm> pending_alarms_;

  // The active ClientProxy's connection lifecycle listener. Non-null while
  // advertising.
  ConnectionListener advertising_listener_;

  AtomicBoolean stop_{false};
  Pcp pcp_;
  Strategy strategy_{PcpToStrategy(pcp_)};
  Prng prng_;
  EncryptionRunner encryption_runner_;
  BwuManager* bwu_manager_;
  EndpointManager::FrameProcessor::Handle handle_ = nullptr;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_BASE_PCP_HANDLER_H_
