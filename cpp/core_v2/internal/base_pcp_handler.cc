#include "core_v2/internal/base_pcp_handler.h"

#include <cassert>
#include <cinttypes>
#include <cstdlib>
#include <limits>
#include <memory>

#include "core_v2/internal/offline_frames.h"
#include "platform_v2/public/logging.h"
#include "platform_v2/public/system_clock.h"
#include "securegcm/d2d_connection_context_v1.h"
#include "securegcm/ukey2_handshake.h"
#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"

namespace location {
namespace nearby {
namespace connections {

BasePcpHandler::BasePcpHandler(EndpointManager* endpoint_manager,
                               EndpointChannelManager* channel_manager)
    : endpoint_manager_(endpoint_manager), channel_manager_(channel_manager) {}

BasePcpHandler::~BasePcpHandler() {
  // Unregister ourselves from the FrameProcessors.
  endpoint_manager_->UnregisterFrameProcessor(V1Frame::CONNECTION_RESPONSE,
                                              handle_);

  // Stop all the ongoing Runnables (as gracefully as possible).
  serial_executor_.Shutdown();
  alarm_executor_.Shutdown();
}

Status BasePcpHandler::StartAdvertising(ClientProxy* client,
                                        const string& service_id,
                                        const ConnectionOptions& options,
                                        const ConnectionRequestInfo& info) {
  Future<Status> response;
  RunOnPcpHandlerThread(
      [this, client, &service_id, &info, &options, &response]() {
        auto result = StartAdvertisingImpl(client, service_id,
                                           client->GenerateLocalEndpointId(),
                                           info.name, options);
        if (!result.status.Ok()) {
          response.Set(result.status);
          return;
        }

        // Now that we've succeeded, mark the client as advertising.
        advertising_options_ = options;
        advertising_listener_ = info.listener;
        client->StartedAdvertising(service_id, GetStrategy(), info.listener,
                                   absl::MakeSpan(result.mediums));
        response.Set({Status::kSuccess});
      });
  return WaitForResult(absl::StrCat("StartAdvertising(", info.name, ")"),
                       client->GetClientId(), &response);
}

void BasePcpHandler::StopAdvertising(ClientProxy* client) {
  CountDownLatch latch(1);
  RunOnPcpHandlerThread([this, client, &latch]() {
    StopAdvertisingImpl(client);
    client->StoppedAdvertising();
    advertising_options_.Clear();
    latch.CountDown();
  });
  WaitForLatch("StopAdvertising", &latch);
}

Status BasePcpHandler::StartDiscovery(ClientProxy* client,
                                      const string& service_id,
                                      const ConnectionOptions& options,
                                      const DiscoveryListener& listener) {
  Future<Status> response;
  RunOnPcpHandlerThread(
      [this, client, service_id, options, listener, &response]() {
        // Ask the implementation to attempt to start discovery.
        auto result = StartDiscoveryImpl(client, service_id, options);
        if (!result.status.Ok()) {
          response.Set(result.status);
          return;
        }

        // Now that we've succeeded, mark the client as discovering and clear
        // out any old endpoints we had discovered.
        discovery_options_ = options;
        discovered_endpoints_.clear();
        client->StartedDiscovery(service_id, GetStrategy(), listener,
                                 absl::MakeSpan(result.mediums));
        response.Set({Status::kSuccess});
      });
  return WaitForResult(absl::StrCat("StartDiscovery(", service_id, ")"),
                       client->GetClientId(), &response);
}

void BasePcpHandler::StopDiscovery(ClientProxy* client) {
  CountDownLatch latch(1);
  RunOnPcpHandlerThread([this, client, &latch]() {
    StopDiscoveryImpl(client);
    client->StoppedDiscovery();
    discovery_options_.Clear();
    latch.CountDown();
  });

  WaitForLatch("stopDiscovery", &latch);
}

void BasePcpHandler::WaitForLatch(const string& method_name,
                                  CountDownLatch* latch) {
  Exception await_exception = latch->Await();
  if (!await_exception.Ok()) {
    if (await_exception.Raised(Exception::kTimeout)) {
      NEARBY_LOG(INFO, "Blocked in %s", method_name.c_str());
    }
  }
}

Status BasePcpHandler::WaitForResult(const string& method_name,
                                     std::int64_t client_id,
                                     Future<Status>* future) {
  if (!future) {
    NEARBY_LOG(INFO, "No future to wait for; return with error");
    return {Status::kError};
  }
  NEARBY_LOG(INFO, "waiting for future to complete");
  ExceptionOr<Status> result = future->Get();
  if (!result.ok()) {
    NEARBY_LOG(INFO, "Future completed with exception: %d", result.exception());
    return {Status::kError};
  }
  NEARBY_LOG(INFO, "Future completed with status: %d", result.result().value);
  return result.result();
}

void BasePcpHandler::RunOnPcpHandlerThread(Runnable runnable) {
  serial_executor_.Execute(std::move(runnable));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
