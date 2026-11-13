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

#ifndef CORE_INTERNAL_PAYLOAD_MANAGER_H_
#define CORE_INTERNAL_PAYLOAD_MANAGER_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/incoming_payload_manager.h"
#include "connections/implementation/outgoing_payload_manager.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "internal/platform/single_thread_executor.h"
#include "proto/connections_enums.pb.h"

namespace nearby::connections {

class PayloadManager : public EndpointManager::FrameProcessor {
 public:
  static constexpr absl::Duration kWaitCloseTimeout = absl::Milliseconds(5000);

  explicit PayloadManager(EndpointManager& endpoint_manager);
  ~PayloadManager() override;

  void SendPayload(ClientProxy* client,
                   const std::vector<std::string>& endpoint_ids,
                   Payload payload);
  Status CancelPayload(ClientProxy* client, Payload::Id payload_id);

  // @EndpointManagerReaderThread
  void OnIncomingFrame(
      location::nearby::connections::OfflineFrame& offline_frame,
      const std::string& from_endpoint_id, ClientProxy* to_client,
      location::nearby::proto::connections::Medium current_medium) override;

  // @EndpointManagerThread
  void OnEndpointDisconnect(
      ClientProxy* client, const std::string& service_id,
      const std::string& endpoint_id,
      std::shared_ptr<absl::Notification> barrier,
      location::nearby::proto::connections::DisconnectionReason reason)
      override;

  void DisconnectFromEndpointManager();

  void SetCustomSavePath(ClientProxy* client, const std::string& path);

  // Returns the one-line summary that is logged when a payload finishes
  // successfully, similar to the "Received FILE ..." / "Sent FILE ..." logs on
  // Android. Exposed for testing.
  //
  // `total_bytes` is the final size of the payload. `start_offset` is the
  // number of bytes that had already been transferred when the transfer start
  // was marked (see `PendingPayload::SetTransferStart()`); those bytes are
  // excluded from the throughput. `duration` is the time between the transfer
  // start and the handling of the last chunk on the transfer thread.
  static std::string FormatTransferSummary(
      bool is_incoming, absl::string_view endpoint_id,
      const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int64_t total_bytes, int64_t start_offset, absl::Duration duration,
      location::nearby::proto::connections::Medium medium);

 private:
  void ProcessControlPacket(ClientProxy* to_client,
                            const std::string& from_endpoint_id,
                            location::nearby::connections::PayloadTransferFrame&
                                payload_transfer_frame);

  void RunOnStatusUpdateThread(const std::string& name,
                               absl::AnyInvocable<void()> runnable);
  void CancelAllPayloads();

  EndpointManager* endpoint_manager_;
  std::atomic<bool> shutdown_{false};
  SingleThreadExecutor payload_status_update_executor_;
  IncomingPayloadManager incoming_payload_manager_;
  OutgoingPayloadManager outgoing_payload_manager_;
};

}  // namespace nearby::connections

#endif  // CORE_INTERNAL_PAYLOAD_MANAGER_H_
