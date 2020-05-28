#ifndef CORE_V2_INTERNAL_BASE_PCP_HANDLER_H_
#define CORE_V2_INTERNAL_BASE_PCP_HANDLER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core_v2/internal/client_proxy.h"
#include "core_v2/internal/encryption_runner.h"
#include "core_v2/internal/endpoint_channel_manager.h"
#include "core_v2/internal/endpoint_manager.h"
#include "core_v2/internal/pcp.h"
#include "core_v2/internal/pcp_handler.h"
#include "core_v2/listeners.h"
#include "core_v2/options.h"
#include "core_v2/status.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform_v2/base/prng.h"
#include "platform_v2/public/atomic_reference.h"
#include "platform_v2/public/cancelable_alarm.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/future.h"
#include "platform_v2/public/scheduled_executor.h"
#include "platform_v2/public/single_thread_executor.h"
#include "platform_v2/public/system_clock.h"
#include "proto/connections_enums.pb.h"
#include "securegcm/d2d_connection_context_v1.h"
#include "securegcm/ukey2_handshake.h"
#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace connections {

// Define a class that supports move operation for pointers using std::swap.
// It replicates std::unique_ptr<> behavior, but it does not own the pointer,
// so it does not attempt destroy it.
// This approach was recommended during code review, as a better alternative to
// reuse of std::unique_ptr<> with custom no-op deleter, for the sake of
// readability.
template <typename T>
class Swapper {
 public:
  Swapper(T* pointer) : pointer_(pointer) {}  // NOLINT.
  Swapper(Swapper&& other) { *this = std::move(other); }
  Swapper& operator=(Swapper&& other) {
    std::swap(pointer_, other.pointer_);
    return *this;
  }
  T* operator->() const { return pointer_; }
  T& operator*() { return *pointer_; }
  operator T*() { return pointer_; }  // NOLINT.
  T* get() const { return pointer_; }
  void reset() { pointer_ = nullptr; }

 private:
  T* pointer_ = nullptr;
};

template <typename T>
Swapper<T> MakeSwapper(T* value) {
  return Swapper<T>(value);
}

// A base implementation of the PcpHandler interface that takes care of all
// bookkeeping and handshake protocols that are common across all PcpHandler
// implementations -- thus, every concrete PcpHandler implementation must extend
// this class, so that they can focus exclusively on the medium-specific
// operations.
class BasePcpHandler : public PcpHandler,
                       public EndpointManager::FrameProcessor {
 public:
  using FrameProcessor = EndpointManager::FrameProcessor;

  // TODO(tracyzhou): Add SecureRandom.
  BasePcpHandler(EndpointManager* endpoint_manager,
                 EndpointChannelManager* channel_manager);
  ~BasePcpHandler() override;
  BasePcpHandler(BasePcpHandler&&) = delete;
  BasePcpHandler& operator=(BasePcpHandler&&) = delete;

  // We have been asked by the client to start advertising. Once we successfully
  // start advertising, we'll change the ClientProxy's state.
  // ConnectionListener (info.listener) will be notified in case of any event.
  // See
  // https://source.corp.google.com/piper///depot/google3/core_v2/listeners.h;l=78
  Status StartAdvertising(ClientProxy* client_proxy,
                          const std::string& service_id,
                          const ConnectionOptions& options,
                          const ConnectionRequestInfo& info) override;

  // If Advertising is active, stop it, and change CLientProxy state,
  // otherwise do nothing.
  void StopAdvertising(ClientProxy* client_proxy) override;

  // Start discovery of endpoints that may be advertising.
  // Update ClientProxy state once discovery started.
  // DiscoveryListener will get called in case of any event.
  Status StartDiscovery(ClientProxy* client_proxy,
                        const std::string& service_id,
                        const ConnectionOptions& options,
                        const DiscoveryListener& listener) override;

  // If Discovery is active, stop it, and change CLientProxy state,
  // otherwise do nothing.
  void StopDiscovery(ClientProxy* client_proxy) override;

  // If remote endpoint has been successfully discovered, request it to form a
  // connection, update state on ClientProxy.
  Status RequestConnection(ClientProxy* client_proxy,
                           const std::string& endpoint_id,
                           const ConnectionRequestInfo& info) override {
    return Status{Status::kError};
  }

  // Either party may call this to accept connection on their part.
  // Until both parties call it, connection will not reach a data phase.
  // Update state in ClientProxy.
  Status AcceptConnection(ClientProxy* client_proxy,
                          const std::string& endpoint_id,
                          const PayloadListener& payload_listener) override {
    return Status{Status::kError};
  }

  // Either party may call this to accept connection on their part.
  // If either party does call it, connection will terminate.
  // Update state in ClientProxy.
  Status RejectConnection(ClientProxy* client_proxy,
                          const std::string& endpoint_id) override {
    return Status{Status::kError};
  }

  // @EndpointManagerReaderThread
  void OnIncomingFrame(const OfflineFrame& frame,
                       const std::string& endpoint_id, ClientProxy* client,
                       proto::connections::Medium medium) override {}

  // Called when an endpoint disconnects while we're waiting for both sides to
  // approve/reject the connection.
  // @EndpointManagerThread
  void OnEndpointDisconnect(ClientProxy* client_proxy,
                            const std::string& endpoint_id,
                            CountDownLatch* barrier) override {}

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
  class DiscoveredEndpoint {
   public:
    virtual ~DiscoveredEndpoint() = default;

    virtual std::string GetEndpointId() const = 0;
    virtual std::string GetEndpointName() const = 0;
    virtual std::string GetServiceId() const = 0;
    virtual proto::connections::Medium GetMedium() const = 0;
  };

  struct ConnectImplResult {
    proto::connections::Medium medium =
        proto::connections::Medium::UNKNOWN_MEDIUM;
    Status status = {Status::kError};
    std::unique_ptr<EndpointChannel> endpoint_channel;
  };

  void RunOnPcpHandlerThread(Runnable runnable);

  ConnectionOptions GetConnectionOptions() const;

  // @PcpHandlerThread
  void OnEndpointFound(ClientProxy* client_proxy,
                       std::unique_ptr<DiscoveredEndpoint> endpoint);

  // @PcpHandlerThread
  void OnEndpointLost(ClientProxy* client_proxy,
                      const DiscoveredEndpoint* endpoint);

  Exception OnIncomingConnection(
      ClientProxy* client_proxy, const std::string& remote_device_name,
      std::unique_ptr<EndpointChannel> endpoint_channel,
      proto::connections::Medium medium);  // throws Exception::IO

  // @PcpHandlerThread
  virtual StartOperationResult StartAdvertisingImpl(
      ClientProxy* client_proxy, const std::string& service_id,
      const std::string& local_endpoint_id,
      const std::string& local_endpoint_name,
      const ConnectionOptions& options) = 0;
  // @PcpHandlerThread
  virtual Status StopAdvertisingImpl(ClientProxy* client_proxy) = 0;

  // @PcpHandlerThread
  virtual StartOperationResult StartDiscoveryImpl(
      ClientProxy* client_proxy, const std::string& service_id,
      const ConnectionOptions& options) = 0;
  // @PcpHandlerThread
  virtual Status StopDiscoveryImpl(ClientProxy* client_proxy) = 0;

  // @PcpHandlerThread
  virtual ConnectImplResult ConnectImpl(ClientProxy* client_proxy,
                                        DiscoveredEndpoint* endpoint) = 0;

  virtual std::vector<proto::connections::Medium>
  GetConnectionMediumsByPriority() = 0;
  virtual proto::connections::Medium GetDefaultUpgradeMedium() = 0;

  EndpointManager* endpoint_manager_;
  EndpointChannelManager* channel_manager_;

 private:
  static Exception WriteConnectionRequestFrame(
      EndpointChannel* endpoint_channel, const std::string& local_endpoint_id,
      const std::string& local_endpoint_name, std::int32_t nonce,
      const std::vector<proto::connections::Medium>& supported_mediums);

  static constexpr absl::Duration kConnectionRequestReadTimeout =
      absl::Seconds(2);
  static constexpr absl::Duration kRejectedConnectionCloseDelay =
      absl::Seconds(2);

  void OnConnectionResponse(ClientProxy* client_proxy,
                            const std::string& endpoint_id,
                            const OfflineFrame& frame);

  // Returns true if the new endpoint is preferred over the old endpoint.
  bool IsPreferred(const BasePcpHandler::DiscoveredEndpoint& new_endpoint,
                   const BasePcpHandler::DiscoveredEndpoint& old_endpoint);

  // Called when an incoming connection has been accepted by both sides.
  //
  // @param client_proxy The client
  // @param endpoint_id The id of the remote device
  // @param supported_mediums The mediums supported by the remote device.
  // Empty
  //        for outgoing connections and older devices that don't report their
  //        supported mediums.
  void InitiateBandwidthUpgrade(
      ClientProxy* client_proxy, const std::string& endpoint_id,
      const std::vector<proto::connections::Medium>& supported_mediums);

  // Returns the optimal medium supported by both devices.
  proto::connections::Medium ChooseBestUpgradeMedium(
      const std::vector<proto::connections::Medium>& supported_mediums);

  void ProcessPreConnectionInitiationFailure(const std::string& endpoint_id,
                                             EndpointChannel* channel,
                                             Status status,
                                             Future<Status>* result);
  void ProcessPreConnectionResultFailure(ClientProxy* client_proxy,
                                         const std::string& endpoint_id);
  DiscoveredEndpoint* GetDiscoveredEndpoint(const std::string& endpoint_id);

  // Called when either side accepts/rejects the connection, but only takes
  // effect after both have accepted or one side has rejected.
  //
  // NOTE: We also take in a 'can_close_immediately' variable. This is because
  // any writes in transit are dropped when we close. To avoid having a reject
  // write being dropped (which causes the other side to report
  // onResult(DISCONNECTED) instead of onResult(REJECTED)), we delay our
  // close. If the other side behaves properly, we shouldn't even see the
  // delay (because they will also close the connection).
  void EvaluateConnectionResult(ClientProxy* client_proxy,
                                const std::string& endpoint_id,
                                bool can_close_immediately);

  ExceptionOr<OfflineFrame> ReadConnectionRequestFrame(
      EndpointChannel* channel);

  void WaitForLatch(const std::string& method_name, CountDownLatch* latch);
  Status WaitForResult(const std::string& method_name, std::int64_t client_id,
                       Future<Status>* future);

  AtomicReference<proto::connections::Medium> bandwidth_upgrade_medium_{
      proto::connections::Medium::UNKNOWN_MEDIUM};
  ScheduledExecutor alarm_executor_;
  SingleThreadExecutor serial_executor_;

  // A map of endpoint id -> DiscoveredEndpoint.
  absl::flat_hash_map<std::string, std::unique_ptr<DiscoveredEndpoint>>
      discovered_endpoints_;
  // A map of endpoint id -> alarm. These alarms delay closing the
  // EndpointChannel to give the other side enough time to read the rejection
  // message. It's expected that the other side will close the connection
  // after reading the message (in which case, this alarm should be cancelled
  // as it's no longer needed), but this alarm is the fallback in case that
  // doesn't happen.
  absl::flat_hash_map<std::string, CancelableAlarm> pending_alarms_;

  // The active ClientProxy's advertising constraints. Empty()
  // returns true if the client hasn't started advertising false otherwise.
  // Note: this is not cleared when the client stops advertising because it
  // might still be useful downstream of advertising (eg: establishing
  // connections, performing bandwidth upgrades, etc.)
  ConnectionOptions advertising_options_;
  // The active ClientProxy's connection lifecycle listener. Non-null while
  // advertising.
  ConnectionListener advertising_listener_;

  // The active ClientProxy's discovery constraints. Null if the client
  // hasn't started discovering. Note: this is not cleared when the client
  // stops discovering because it might still be useful downstream of
  // discovery (eg: connection speed, etc.)
  ConnectionOptions discovery_options_;
  Prng prng_;
  EncryptionRunner encryption_runner_;
  EndpointManager::FrameProcessor::Handle handle_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_BASE_PCP_HANDLER_H_
