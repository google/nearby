#include "core/internal/base_bandwidth_upgrade_handler.h"

namespace location {
namespace nearby {
namespace connections {

namespace base_bandwidth_upgrade_handler {

template <typename Platform>
class RevertRunnable : public Runnable {
 public:
  void run() {}
};

template <typename Platform>
class InitiateBandwidthUpgradeForEndpointRunnable : public Runnable {
 public:
  void run() {}
};

template <typename Platform>
class ProcessEndpointDisconnectionRunnable : public Runnable {
 public:
  void run() {}
};

template <typename Platform>
class ProcessBandwidthUpgradeNegotiationFrameRunnable : public Runnable {
 public:
  void run() {}
};

}  // namespace base_bandwidth_upgrade_handler

template <typename Platform>
BaseBandwidthUpgradeHandler<Platform>::BaseBandwidthUpgradeHandler(
    Ptr<EndpointChannelManager<Platform> > endpoint_channel_manager)
    : endpoint_channel_manager_(endpoint_channel_manager),
      alarm_executor_(),
      serial_executor_(),
      previous_endpoint_channels_(),
      in_progress_upgrades_(),
      safe_to_close_write_timestamps_() {}

template <typename Platform>
BaseBandwidthUpgradeHandler<Platform>::~BaseBandwidthUpgradeHandler() {}

template <typename Platform>
void BaseBandwidthUpgradeHandler<Platform>::revert() {}

template <typename Platform>
void BaseBandwidthUpgradeHandler<Platform>::processEndpointDisconnection(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    Ptr<CountDownLatch> process_disconnection_barrier) {}

template <typename Platform>
void BaseBandwidthUpgradeHandler<Platform>::initiateBandwidthUpgradeForEndpoint(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id) {}

template <typename Platform>
void BaseBandwidthUpgradeHandler<Platform>::
    processBandwidthUpgradeNegotiationFrame(
        ConstPtr<BandwidthUpgradeNegotiationFrame>
            bandwidth_upgrade_negotiation,
        Ptr<ClientProxy<Platform> > to_client_proxy,
        const string& from_endpoint_id,
        proto::connections::Medium current_medium) {}

template <typename Platform>
Ptr<EndpointChannelManager<Platform> >
BaseBandwidthUpgradeHandler<Platform>::getEndpointChannelManager() {
  return endpoint_channel_manager_;
}

template <typename Platform>
void BaseBandwidthUpgradeHandler<Platform>::onIncomingConnection(
    Ptr<IncomingSocketConnection> incoming_socket_connection) {}

template <typename Platform>
void BaseBandwidthUpgradeHandler<Platform>::runOnBandwidthUpgradeHandlerThread(
    Ptr<Runnable> runnable) {}

template <typename Platform>
void BaseBandwidthUpgradeHandler<Platform>::runUpgradeProtocol(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    Ptr<EndpointChannel> new_endpoint_channel) {}

template <typename Platform>
void BaseBandwidthUpgradeHandler<Platform>::
    processBandwidthUpgradePathAvailableEvent(
        const string& endpoint_id, Ptr<ClientProxy<Platform> > client_proxy,
        ConstPtr<BandwidthUpgradeNegotiationFrame::UpgradePathInfo>
            upgrade_path_info,
        proto::connections::Medium current_medium) {}

template <typename Platform>
Ptr<EndpointChannel> BaseBandwidthUpgradeHandler<Platform>::
    processBandwidthUpgradePathAvailableEventInternal(
        const string& endpoint_id, Ptr<ClientProxy<Platform> > client_proxy,
        ConstPtr<BandwidthUpgradeNegotiationFrame::UpgradePathInfo>
            upgrade_path_info) {
  return Ptr<EndpointChannel>();
}

template <typename Platform>
void BaseBandwidthUpgradeHandler<Platform>::processLastWriteToPriorChannelEvent(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id) {}

template <typename Platform>
void BaseBandwidthUpgradeHandler<Platform>::processSafeToClosePriorChannelEvent(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id) {}

template <typename Platform>
std::int64_t BaseBandwidthUpgradeHandler<Platform>::calculateCloseDelay(
    const string& endpoint_id) {
  return 0;
}

template <typename Platform>
std::int64_t
BaseBandwidthUpgradeHandler<Platform>::getMillisSinceSafeCloseWritten(
    const string& endpoint_id) {
  return 0;
}

// TODO(ahlee): This will differ from the Java code as we don't have to handle
// analytics in the C++ code.
template <typename Platform>
void BaseBandwidthUpgradeHandler<Platform>::
    attemptToRecordBandwidthUpgradeErrorForUnknownEndpoint(
        proto::connections::BandwidthUpgradeResult result,
        proto::connections::BandwidthUpgradeErrorStage error_stage) {}

// TODO(ahlee): This will differ from the Java code (previously threw an
// UpgradeException).
template <typename Platform>
Ptr<BandwidthUpgradeNegotiationFrame::ClientIntroduction>
BaseBandwidthUpgradeHandler<Platform>::readClientIntroductionFrame(
    Ptr<EndpointChannel> endpoint_channel) {
  return Ptr<BandwidthUpgradeNegotiationFrame::ClientIntroduction>();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
