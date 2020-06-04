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

#ifndef CORE_INTERNAL_BASE_PCP_HANDLER_H_
#define CORE_INTERNAL_BASE_PCP_HANDLER_H_

#include <cstdint>
#include <map>
#include <vector>

#include "core/internal/bandwidth_upgrade_manager.h"
#include "core/internal/client_proxy.h"
#include "core/internal/encryption_runner.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/pcp.h"
#include "core/internal/pcp_handler.h"
#include "core/listeners.h"
#include "core/options.h"
#include "core/status.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/api/atomic_reference.h"
#include "platform/api/count_down_latch.h"
#include "platform/api/settable_future.h"
#include "platform/api/system_clock.h"
#include "platform/cancelable_alarm.h"
#include "platform/port/string.h"
#include "platform/prng.h"
#include "platform/ptr.h"
#include "proto/connections_enums.pb.h"
#include "securegcm/ukey2_handshake.h"

namespace location {
namespace nearby {
namespace connections {

namespace base_pcp_handler {

template <typename>
class StartAdvertisingCallable;
template <typename>
class StopAdvertisingRunnable;
template <typename>
class StartDiscoveryCallable;
template <typename>
class StopDiscoveryRunnable;
template <typename>
class RequestConnectionRunnable;
template <typename>
class AcceptConnectionCallable;
template <typename>
class RejectConnectionCallable;
template <typename>
class ProcessEndpointDisconnectionRunnable;
template <typename>
class OnConnectionResponseRunnable;
template <typename>
class OnEncryptionSuccessRunnable;
template <typename>
class OnEncryptionFailureRunnable;

}  // namespace base_pcp_handler

// A base implementation of the PCPHandler interface that takes care of all
// bookkeeping and handshake protocols that are common across all PCPHandler
// implementations -- thus, every concrete PCPHandler implementation must extend
// this class, so that they can focus exclusively on the medium-specific
// operations.
template <typename Platform>
class BasePCPHandler
    : public PCPHandler<Platform>,
      public EndpointManager<Platform>::IncomingOfflineFrameProcessor {
 public:
  // TODO(tracyzhou): Add SecureRandom.
  BasePCPHandler(Ptr<EndpointManager<Platform> > endpoint_manager,
                 Ptr<EndpointChannelManager> endpoint_channel_manager,
                 Ptr<BandwidthUpgradeManager> bandwidth_upgrade_manager);
  ~BasePCPHandler() override;

  // We have been asked by the client to start advertising. Once we successfully
  // start advertising, we'll change the ClientProxy's state.
  Status::Value startAdvertising(
      Ptr<ClientProxy<Platform> > client_proxy, const string& service_id,
      const string& local_endpoint_name,
      const AdvertisingOptions& advertising_options,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) override;
  void stopAdvertising(Ptr<ClientProxy<Platform> > client_proxy) override;

  Status::Value startDiscovery(
      Ptr<ClientProxy<Platform> > client_proxy, const string& service_id,
      const DiscoveryOptions& discovery_options,
      Ptr<DiscoveryListener> discovery_listener) override;
  void stopDiscovery(Ptr<ClientProxy<Platform> > client_proxy) override;

  Status::Value requestConnection(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_name,
      const string& endpoint_id,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) override;
  Status::Value acceptConnection(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      Ptr<PayloadListener> payload_listener) override;
  Status::Value rejectConnection(Ptr<ClientProxy<Platform> > client_proxy,
                                 const string& endpoint_id) override;

  proto::connections::Medium getBandwidthUpgradeMedium() override;

  // @EndpointManagerReaderThread
  void processIncomingOfflineFrame(
      ConstPtr<OfflineFrame> offline_frame, const string& from_endpoint_id,
      Ptr<ClientProxy<Platform> > to_client_proxy,
      proto::connections::Medium current_medium) override;

  // Called when an endpoint disconnects while we're waiting for both sides to
  // approve/reject the connection.
  // @EndpointManagerThread
  void processEndpointDisconnection(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      Ptr<CountDownLatch> process_disconnection_barrier) override;

  // Conforms to EncryptionRunner::ResultListener::onEncryptionSuccess().
  // @EncryptionRunnerThread
  void onEncryptionSuccessImpl(const string& endpoint_id,
                               Ptr<securegcm::UKey2Handshake> ukey2_handshake,
                               const string& authentication_token,
                               ConstPtr<ByteArray> raw_authentication_token);

  // EncryptionRunner::ResultListener::onEncryptionFailure().
  // @EncryptionRunnerThread
  void onEncryptionFailureImpl(const string& endpoint_id,
                               Ptr<EndpointChannel> channel);

 protected:
  // The result of a call to startAdvertisingImpl() or startDiscoveryImpl().
  class StartOperationResult {
   public:
    static Ptr<StartOperationResult> error(Status::Value status) {
      return MakePtr(new StartOperationResult(status));
    }

    static Ptr<StartOperationResult> success(
        const std::vector<proto::connections::Medium>& mediums) {
      // Note: check here and not in the constructor, since for errors we have
      // null mediums.
      return MakePtr(new StartOperationResult(mediums));
    }

   private:
    template <typename>
    friend class base_pcp_handler::StartAdvertisingCallable;
    template <typename>
    friend class base_pcp_handler::StartDiscoveryCallable;

    explicit StartOperationResult(Status::Value status)
        : status_(status), mediums_() {}
    explicit StartOperationResult(
        const std::vector<proto::connections::Medium>& mediums)
        : status_(Status::SUCCESS), mediums_(mediums) {}

    // The status to be returned to the client.
    Status::Value status_;
    // If success, the mediums on which we are now advertising/discovering, for
    // analytics.
    std::vector<proto::connections::Medium> mediums_;
  };

  // Represents an endpoint that we've discovered. Typically, the implementation
  // will know how to connect to this endpoint if asked. (eg. It holds on to a
  // BluetoothDevice)
  class DiscoveredEndpoint {
   public:
    virtual ~DiscoveredEndpoint() {}

    virtual string getEndpointId() = 0;
    virtual string getEndpointName() = 0;
    virtual string getServiceId() = 0;
    virtual proto::connections::Medium getMedium() = 0;
  };

  struct ConnectImplResult {
    proto::connections::Medium medium;
    Status::Value status;
    Ptr<EndpointChannel> endpoint_channel;

    explicit ConnectImplResult(Ptr<EndpointChannel> endpoint_channel)
        : medium(proto::connections::Medium::UNKNOWN_MEDIUM),
          status(Status::SUCCESS),
          endpoint_channel(endpoint_channel) {}
    ConnectImplResult(proto::connections::Medium medium, Status::Value status)
        : medium(medium), status(status), endpoint_channel() {}
  };

  void runOnPCPHandlerThread(Ptr<Runnable> runnable);

  Ptr<AdvertisingOptions> getAdvertisingOptions();

  // @PCPHandlerThread
  void onEndpointFound(Ptr<ClientProxy<Platform> > client_proxy,
                       Ptr<DiscoveredEndpoint> endpoint);

  // @PCPHandlerThread
  void onEndpointLost(Ptr<ClientProxy<Platform> > client_proxy,
                      Ptr<DiscoveredEndpoint> endpoint);

  Exception::Value onIncomingConnection(
      Ptr<ClientProxy<Platform> > client_proxy,
      const string& remote_device_name, Ptr<EndpointChannel> endpoint_channel,
      proto::connections::Medium medium);  // throws Exception::IO

  virtual bool hasOutgoingConnections(Ptr<ClientProxy<Platform> > client_proxy);
  virtual bool hasIncomingConnections(Ptr<ClientProxy<Platform> > client_proxy);

  virtual bool canSendOutgoingConnection(
      Ptr<ClientProxy<Platform> > client_proxy);
  virtual bool canReceiveIncomingConnection(
      Ptr<ClientProxy<Platform> > client_proxy);

  // @PCPHandlerThread
  virtual Ptr<StartOperationResult> startAdvertisingImpl(
      Ptr<ClientProxy<Platform> > client_proxy, const string& service_id,
      const string& local_endpoint_id, const string& local_endpoint_name,
      const AdvertisingOptions& options) = 0;
  // @PCPHandlerThread
  virtual Status::Value stopAdvertisingImpl(
      Ptr<ClientProxy<Platform> > client_proxy) = 0;

  // @PCPHandlerThread
  virtual Ptr<StartOperationResult> startDiscoveryImpl(
      Ptr<ClientProxy<Platform> > client_proxy, const string& service_id,
      const DiscoveryOptions& options) = 0;
  // @PCPHandlerThread
  virtual Status::Value stopDiscoveryImpl(
      Ptr<ClientProxy<Platform> > client_proxy) = 0;

  // @PCPHandlerThread
  virtual ConnectImplResult connectImpl(
      Ptr<ClientProxy<Platform> > client_proxy,
      Ptr<DiscoveredEndpoint> endpoint) = 0;

  virtual std::vector<proto::connections::Medium>
  getConnectionMediumsByPriority() = 0;
  virtual proto::connections::Medium getDefaultUpgradeMedium() = 0;

  Ptr<EndpointManager<Platform> > endpoint_manager_;
  Ptr<EndpointChannelManager> endpoint_channel_manager_;
  Ptr<BandwidthUpgradeManager> bandwidth_upgrade_manager_;

 private:
  template <typename>
  friend class base_pcp_handler::StartAdvertisingCallable;
  template <typename>
  friend class base_pcp_handler::StopAdvertisingRunnable;
  template <typename>
  friend class base_pcp_handler::StartDiscoveryCallable;
  template <typename>
  friend class base_pcp_handler::StopDiscoveryRunnable;
  template <typename>
  friend class base_pcp_handler::RequestConnectionRunnable;
  template <typename>
  friend class base_pcp_handler::AcceptConnectionCallable;
  template <typename>
  friend class base_pcp_handler::RejectConnectionCallable;
  template <typename>
  friend class base_pcp_handler::OnConnectionResponseRunnable;
  template <typename>
  friend class base_pcp_handler::ProcessEndpointDisconnectionRunnable;
  template <typename>
  friend class base_pcp_handler::OnEncryptionSuccessRunnable;
  template <typename>
  friend class base_pcp_handler::OnEncryptionFailureRunnable;

  class ResultListenerFacade
      : public EncryptionRunner<Platform>::ResultListener {
   public:
    explicit ResultListenerFacade(Ptr<BasePCPHandler<Platform> > impl)
        : impl_(impl) {}

    void onEncryptionSuccess(
        const string& endpoint_id,
        Ptr<securegcm::UKey2Handshake> ukey2_handshake,
        const string& authentication_token,
        ConstPtr<ByteArray> raw_authentication_token) override {
      impl_->onEncryptionSuccessImpl(endpoint_id, ukey2_handshake,
                                     authentication_token,
                                     raw_authentication_token);
    }

    void onEncryptionFailure(const string& endpoint_id,
                             Ptr<EndpointChannel> channel) override {
      impl_->onEncryptionFailureImpl(endpoint_id, channel);
    }

   private:
    Ptr<BasePCPHandler<Platform> > impl_;
  };

  class PendingConnectionInfo {
   public:
    static Ptr<PendingConnectionInfo> newIncomingPendingConnectionInfo(
        Ptr<ClientProxy<Platform> > client_proxy,
        const string& remote_endpoint_name,
        Ptr<EndpointChannel> endpoint_channel, std::int32_t nonce,
        std::int64_t start_time_millis,
        Ptr<ConnectionLifecycleListener> connection_lifecycle_listener,
        const std::vector<proto::connections::Medium>& supported_mediums);

    static Ptr<PendingConnectionInfo> newOutgoingPendingConnectionInfo(
        Ptr<ClientProxy<Platform> > client_proxy,
        const string& remote_endpoint_name,
        Ptr<EndpointChannel> endpoint_channel, std::int32_t nonce,
        std::int64_t start_time_millis,
        Ptr<ConnectionLifecycleListener> connection_lifecycle_listener,
        Ptr<SettableFuture<Status::Value> > request_connection_result);

    ~PendingConnectionInfo();

    void setUKey2Handshake(Ptr<securegcm::UKey2Handshake> ukey2_handshake);

    void localEndpointAcceptedConnection(const string& endpoint_id,
                                         Ptr<PayloadListener> payload_listener);

    void localEndpointRejectedConnection(const string& endpoint_id);

   private:
    template <typename>
    friend class BasePCPHandler;
    template <typename>
    friend class base_pcp_handler::RequestConnectionRunnable;
    template <typename>
    friend class base_pcp_handler::AcceptConnectionCallable;
    template <typename>
    friend class base_pcp_handler::RejectConnectionCallable;
    template <typename>
    friend class base_pcp_handler::OnEncryptionSuccessRunnable;
    template <typename>
    friend class base_pcp_handler::OnEncryptionFailureRunnable;

    PendingConnectionInfo(
        Ptr<ClientProxy<Platform> > client_proxy,
        const string& remote_endpoint_name,
        Ptr<EndpointChannel> endpoint_channel, std::int32_t nonce,
        bool is_incoming, std::int64_t start_time_millis,
        Ptr<ConnectionLifecycleListener> connection_lifecycle_listener,
        Ptr<SettableFuture<Status::Value> > request_connection_result,
        const std::vector<proto::connections::Medium>& supported_mediums);

    Ptr<ClientProxy<Platform> > client_proxy_;
    const string remote_endpoint_name_;
    // Can be released prior to destructor.
    ScopedPtr<Ptr<EndpointChannel> > endpoint_channel_;
    const std::int32_t nonce_;
    const bool is_incoming_;
    const std::int64_t start_time_millis_;
    // Can be released prior to destructor.
    ScopedPtr<Ptr<ConnectionLifecycleListener> > connection_lifecycle_listener_;

    // Only set for outgoing connections. Can be released prior to destructor.
    // TODO(b/77783039): Consider creating a one-time-use-only wrapper class
    // around the Ptr<SettableFuture> that's passed in (that also implements the
    // SettableFuture interface) so we can avoid the easy-to-forget calls to
    // request_connection_result_.clear() peppered through multiple places in
    // the code.
    Ptr<SettableFuture<Status::Value> > request_connection_result_;

    // Only (possibly) set for incoming connections.
    const std::vector<proto::connections::Medium> supported_mediums_;

    // If set, this is owned.
    Ptr<securegcm::UKey2Handshake> ukey2_handshake_;
  };

  static Exception::Value writeConnectionRequestFrame(
      Ptr<EndpointChannel> endpoint_channel, const string& local_endpoint_id,
      const string& local_endpoint_name, std::int32_t nonce,
      const std::vector<proto::connections::Medium>& supported_mediums);

  static const std::int64_t kConnectionRequestReadTimeoutMillis;
  static const std::int64_t kRejectedConnectionCloseDelayMillis;

  template <typename T>
  Ptr<Future<T> > runOnPCPHandlerThread(Ptr<Callable<T> > callable);

  // The interface deviates from the Java code to convey a better ownership
  // story. Ownership of 'connection_response_offline_frame' is transferred to
  // the callee by calling this method.
  void onConnectionResponse(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      ConstPtr<OfflineFrame> connection_response_offline_frame);

  // Returns true if the new endpoint is preferred over the old endpoint.
  bool isPreferred(Ptr<DiscoveredEndpoint> new_endpoint,
                   Ptr<DiscoveredEndpoint> old_endpoint);

  bool shouldEnforceTopologyConstraints();
  bool autoUpgradeBandwidth();

  // Returns true if the incoming connection should be killed. This only happens
  // when an incoming connection arrives while we have an outgoing connection to
  // the same endpoint and we need to stop one connection.
  bool breakTie(Ptr<ClientProxy<Platform> > client_proxy,
                const string& endpoint_id, std::int32_t incoming_nonce,
                Ptr<EndpointChannel> endpoint_channel);
  // We're not sure how far our outgoing connection has gotten. We may (or may
  // not) have called ClientProxy.onConnectionInitiated. Therefore, we'll call
  // both preInit and preResult failures.
  void processTieBreakLoss(Ptr<ClientProxy<Platform> > client_proxy,
                           const string& endpoint_id,
                           Ptr<PendingConnectionInfo> connection_info);

  // Called when an incoming connection has been accepted by both sides.
  //
  // @param client_proxy The client
  // @param endpoint_id The id of the remote device
  // @param supported_mediums The mediums supported by the remote device. Empty
  //        for outgoing connections and older devices that don't report their
  //        supported mediums.
  void initiateBandwidthUpgrade(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      const std::vector<proto::connections::Medium>& supported_mediums);

  // Returns the optimal medium supported by both devices.
  proto::connections::Medium chooseBestUpgradeMedium(
      const std::vector<proto::connections::Medium>& their_supported_mediums);

  // This method should assume ownership of endpoint_id.
  void processPreConnectionInitiationFailure(
      Ptr<ClientProxy<Platform> > client_proxy,
      proto::connections::Medium medium, const string& endpoint_id,
      Ptr<EndpointChannel> endpoint_channel, bool is_incoming,
      std::int64_t start_time_millis, Status::Value status,
      Ptr<SettableFuture<Status::Value> > request_connection_result);
  void processPreConnectionResultFailure(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id);
  Ptr<DiscoveredEndpoint> getDiscoveredEndpoint(const string& endpoint_id);

  // Called when either side accepts/rejects the connection, but only takes
  // effect after both have accepted or one side has rejected.
  //
  // NOTE: We also take in a 'can_close_immediately' variable. This is because
  // any writes in transit are dropped when we close. To avoid having a reject
  // write being dropped (which causes the other side to report
  // onResult(DISCONNECTED) instead of onResult(REJECTED)), we delay our close.
  // If the other side behaves properly, we shouldn't even see the delay
  // (because they will also close the connection).
  void evaluateConnectionResult(Ptr<ClientProxy<Platform> > client_proxy,
                                const string& endpoint_id,
                                bool can_close_immediately);

  ExceptionOr<ConstPtr<OfflineFrame> > readConnectionRequestFrame(
      Ptr<EndpointChannel> endpoint_channel);

  void waitForLatch(const string& method_name, Ptr<CountDownLatch> latch);
  Status::Value waitForResult(const string& method_name, std::int64_t client_id,
                              Ptr<Future<Status::Value> > result_future);

  ScopedPtr<Ptr<AtomicReference<proto::connections::Medium> > >
      bandwidth_upgrade_medium_;
  ScopedPtr<Ptr<typename Platform::ScheduledExecutorType> > alarm_executor_;
  ScopedPtr<Ptr<typename Platform::SingleThreadExecutorType> > serial_executor_;
  ScopedPtr<Ptr<SystemClock> > system_clock_;
  Prng prng_;

  // A map of endpoint id -> PendingConnectionInfo. Entries in this map imply
  // that there is an active connection to the endpoint and we're waiting for
  // both sides to accept before allowing payloads through. Once the fate of the
  // connection is decided (either accepted or rejected), it should be removed
  // from this map.
  typedef std::map<string, Ptr<PendingConnectionInfo> > PendingConnectionsMap;
  PendingConnectionsMap pending_connections_;
  // A map of endpoint id -> DiscoveredEndpoint.
  typedef std::map<string, Ptr<DiscoveredEndpoint> > DiscoveredEndpointsMap;
  DiscoveredEndpointsMap discovered_endpoints_;
  // A map of endpoint id -> alarm. These alarms delay closing the
  // EndpointChannel to give the other side enough time to read the rejection
  // message. It's expected that the other side will close the connection after
  // reading the message (in which case, this alarm should be cancelled as it's
  // no longer needed), but this alarm is the fallback in case that doesn't
  // happen.
  typedef std::map<string, Ptr<CancelableAlarm> >
      PendingRejectedConnectionCloseAlarmsMap;
  PendingRejectedConnectionCloseAlarmsMap
      pending_rejected_connection_close_alarms_;

  // The active ClientProxy's advertising constraints. Null if the client hasn't
  // started advertising. Note: this is not cleared when the client stops
  // advertising because it might still be useful downstream of advertising (eg:
  // establishing connections, performing bandwidth upgrades, etc.)
  Ptr<AdvertisingOptions> advertising_options_;
  // The active ClientProxy's connection lifecycle listener. Non-null while
  // advertising.
  Ptr<ConnectionLifecycleListener> advertising_connection_lifecycle_listener_;

  // The active ClientProxy's discovery constraints. Null if the client
  // hasn't started discovering. Note: this is not cleared when the client
  // stops discovering because it might still be useful downstream of
  // discovery (eg: connection speed, etc.)
  Ptr<DiscoveryOptions> discovery_options_;

  // This should have been a ScopedPtr, but we are making this a Ptr to manually
  // control the order of destruction.
  Ptr<EncryptionRunner<Platform> > encryption_runner_;
  std::shared_ptr<BasePCPHandler> self_{this, [](void*){}};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/base_pcp_handler.cc"

#endif  // CORE_INTERNAL_BASE_PCP_HANDLER_H_
