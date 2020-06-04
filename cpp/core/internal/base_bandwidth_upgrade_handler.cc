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

#include "core/internal/base_bandwidth_upgrade_handler.h"

namespace location {
namespace nearby {
namespace connections {

namespace {
using Platform = platform::ImplementationPlatform;
}

namespace base_bandwidth_upgrade_handler {

class RevertRunnable : public Runnable {
 public:
  void run() override {}
};

class InitiateBandwidthUpgradeForEndpointRunnable : public Runnable {
 public:
  void run() override {}
};

class ProcessEndpointDisconnectionRunnable : public Runnable {
 public:
  void run() override {}
};

class ProcessBandwidthUpgradeNegotiationFrameRunnable : public Runnable {
 public:
  void run() override {}
};

}  // namespace base_bandwidth_upgrade_handler

BaseBandwidthUpgradeHandler::BaseBandwidthUpgradeHandler(
    Ptr<EndpointChannelManager> endpoint_channel_manager)
    : endpoint_channel_manager_(endpoint_channel_manager),
      alarm_executor_(nullptr),
      serial_executor_(nullptr),
      previous_endpoint_channels_(),
      in_progress_upgrades_(),
      safe_to_close_write_timestamps_() {}

BaseBandwidthUpgradeHandler::~BaseBandwidthUpgradeHandler() {}

void BaseBandwidthUpgradeHandler::revert() {}

void BaseBandwidthUpgradeHandler::processEndpointDisconnection(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    Ptr<CountDownLatch> process_disconnection_barrier) {}

void BaseBandwidthUpgradeHandler::initiateBandwidthUpgradeForEndpoint(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id) {}

void BaseBandwidthUpgradeHandler::processBandwidthUpgradeNegotiationFrame(
    ConstPtr<BandwidthUpgradeNegotiationFrame> bandwidth_upgrade_negotiation,
    Ptr<ClientProxy<Platform> > to_client_proxy, const string& from_endpoint_id,
    proto::connections::Medium current_medium) {}

Ptr<EndpointChannelManager>
BaseBandwidthUpgradeHandler::getEndpointChannelManager() {
  return endpoint_channel_manager_;
}

void BaseBandwidthUpgradeHandler::onIncomingConnection(
    Ptr<IncomingSocketConnection> incoming_socket_connection) {}

void BaseBandwidthUpgradeHandler::runOnBandwidthUpgradeHandlerThread(
    Ptr<Runnable> runnable) {}

void BaseBandwidthUpgradeHandler::runUpgradeProtocol(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    Ptr<EndpointChannel> new_endpoint_channel) {}

void BaseBandwidthUpgradeHandler::processBandwidthUpgradePathAvailableEvent(
    const string& endpoint_id, Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<BandwidthUpgradeNegotiationFrame::UpgradePathInfo>
        upgrade_path_info,
    proto::connections::Medium current_medium) {}

Ptr<EndpointChannel>
BaseBandwidthUpgradeHandler::processBandwidthUpgradePathAvailableEventInternal(
    const string& endpoint_id, Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<BandwidthUpgradeNegotiationFrame::UpgradePathInfo>
        upgrade_path_info) {
  return Ptr<EndpointChannel>();
}

void BaseBandwidthUpgradeHandler::processLastWriteToPriorChannelEvent(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id) {}

void BaseBandwidthUpgradeHandler::processSafeToClosePriorChannelEvent(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id) {}

std::int64_t BaseBandwidthUpgradeHandler::calculateCloseDelay(
    const string& endpoint_id) {
  return 0;
}

std::int64_t BaseBandwidthUpgradeHandler::getMillisSinceSafeCloseWritten(
    const string& endpoint_id) {
  return 0;
}

// TODO(ahlee): This will differ from the Java code as we don't have to handle
// analytics in the C++ code.
void BaseBandwidthUpgradeHandler::
    attemptToRecordBandwidthUpgradeErrorForUnknownEndpoint(
        proto::connections::BandwidthUpgradeResult result,
        proto::connections::BandwidthUpgradeErrorStage error_stage) {}

// TODO(ahlee): This will differ from the Java code (previously threw an
// UpgradeException).
Ptr<BandwidthUpgradeNegotiationFrame::ClientIntroduction>
BaseBandwidthUpgradeHandler::readClientIntroductionFrame(
    Ptr<EndpointChannel> endpoint_channel) {
  return Ptr<BandwidthUpgradeNegotiationFrame::ClientIntroduction>();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
