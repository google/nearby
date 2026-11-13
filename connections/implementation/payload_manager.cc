// Copyright 2021-2023 Google LLC
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

#include "connections/implementation/payload_manager.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/pending_payload.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "internal/platform/logging.h"
#include "proto/connections_enums.pb.h"

namespace nearby::connections {

namespace {
using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::PayloadTransferFrame;
using ::location::nearby::connections::V1Frame;
using ::location::nearby::proto::connections::OperationResultCode;
using ::location::nearby::proto::connections::PayloadStatus;
using ::nearby::analytics::AnalyticsRecorder;
}  // namespace

std::string PayloadManager::FormatTransferSummary(
    bool is_incoming, absl::string_view endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    int64_t total_bytes, int64_t start_offset, absl::Duration duration,
    location::nearby::proto::connections::Medium medium) {
  return FormatPayloadTransferSummary(is_incoming, endpoint_id, payload_header,
                                      total_bytes, start_offset, duration,
                                      medium);
}

PayloadManager::PayloadManager(EndpointManager& endpoint_manager)
    : endpoint_manager_(&endpoint_manager),
      incoming_payload_manager_(endpoint_manager,
                                payload_status_update_executor_, shutdown_),
      outgoing_payload_manager_(endpoint_manager,
                                payload_status_update_executor_, shutdown_) {
  endpoint_manager_->RegisterFrameProcessor(V1Frame::PAYLOAD_TRANSFER, this);
}

void PayloadManager::CancelAllPayloads() {
  VLOG(1) << "PayloadManager: canceling payloads; self=" << this;
  incoming_payload_manager_.CancelAllPayloads();
  outgoing_payload_manager_.CancelAllPayloadsAndWait();
}

void PayloadManager::DisconnectFromEndpointManager() {
  if (shutdown_.exchange(true)) return;
  // Unregister ourselves from the FrameProcessors.
  endpoint_manager_->UnregisterFrameProcessor(V1Frame::PAYLOAD_TRANSFER, this);
}

PayloadManager::~PayloadManager() {
  VLOG(1) << "PayloadManager: going down; self=" << this;
  DisconnectFromEndpointManager();
  CancelAllPayloads();
  VLOG(1) << "PayloadManager: turn down payload executors; self=" << this;
  outgoing_payload_manager_.ShutdownExecutors();

  absl::Notification stop_notification;
  // Clear our tracked pending payloads.
  RunOnStatusUpdateThread(
      "~payload-manager", [this, &stop_notification]() {
        VLOG(1) << "PayloadManager: stop tracking payloads; self=" << this;
        incoming_payload_manager_.StopTrackingAllPayloads();
        outgoing_payload_manager_.StopTrackingAllPayloads();
        stop_notification.Notify();
      });
  stop_notification.WaitForNotification();

  VLOG(1) << "PayloadManager: turn down notification executor; self=" << this;
  // Stop all the ongoing Runnables (as gracefully as possible).
  payload_status_update_executor_.Shutdown();

  VLOG(1) << "PayloadManager: down; self=" << this;
}

void PayloadManager::SendPayload(ClientProxy* client,
                                 const std::vector<std::string>& endpoint_ids,
                                 Payload payload) {
  outgoing_payload_manager_.SendPayload(client, endpoint_ids,
                                        std::move(payload));
}

Status PayloadManager::CancelPayload(ClientProxy* client,
                                     Payload::Id payload_id) {
  // Mark the payload as canceled and close the underlying file/stream handle
  // immediately to unblock any pending read and allow callers to delete
  // partially written incoming files even if no further chunks arrive.
  if (incoming_payload_manager_.CancelPayload(payload_id)) {
    return {Status::kSuccess};
  }
  if (outgoing_payload_manager_.CancelPayload(payload_id)) {
    return {Status::kSuccess};
  }

  VLOG(1) << "Client requested cancel for unknown payload_id=" << payload_id
          << ", ignoring.";
  return {Status::kPayloadUnknown};
}

// @EndpointManagerDataPool
void PayloadManager::OnIncomingFrame(
    OfflineFrame& offline_frame, const std::string& from_endpoint_id,
    ClientProxy* to_client,
    location::nearby::proto::connections::Medium current_medium) {
  PayloadTransferFrame& frame =
      *offline_frame.mutable_v1()->mutable_payload_transfer();

  // Block any payload before the connection been accepted by both sides
  // to prevent unauthorized transfer.
  if (!to_client->IsConnectedToEndpoint(from_endpoint_id)) {
    VLOG(1) << "PayloadManager skipped process payloads before PCP connected, "
            << frame.payload_header().id();
    return;
  }

  switch (frame.packet_type()) {
    case PayloadTransferFrame::CONTROL:
      VLOG(1) << "PayloadManager::OnIncomingFrame [CONTROL]: self=" << this
              << "; endpoint_id=" << from_endpoint_id;
      ProcessControlPacket(to_client, from_endpoint_id, frame);
      break;
    case PayloadTransferFrame::DATA:
      incoming_payload_manager_.ProcessDataPacket(to_client, from_endpoint_id,
                                                  frame, current_medium);
      break;
    case PayloadTransferFrame::PAYLOAD_ACK:
      VLOG(1) << "[safe-to-disconnect][PAYLOAD_RECEIVED_ACK] sender "
                 "received payload ack from "
              << from_endpoint_id;
      break;
    default:
      LOG(WARNING) << "PayloadManager: invalid frame; remote endpoint: self="
                   << this << "; endpoint_id=" << from_endpoint_id;
      break;
  }
}

void PayloadManager::OnEndpointDisconnect(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id,
    std::shared_ptr<absl::Notification> barrier,
    location::nearby::proto::connections::DisconnectionReason reason) {
  if (shutdown_.load()) {
    barrier->Notify();
    return;
  }
  RunOnStatusUpdateThread(
      "payload-manager-on-disconnect",
      [this, client, endpoint_id, barrier = std::move(barrier),
       reason]() {
        // Iterate through all our payloads and look for payloads associated
        // with this endpoint.
        PayloadStatus payload_status;
        OperationResultCode operation_result_code;
        switch (reason) {
          case location::nearby::proto::connections::DisconnectionReason::
              LOCAL_DISCONNECTION:
            payload_status = PayloadStatus::LOCAL_CLIENT_DISCONNECTION;
            operation_result_code =
                OperationResultCode::CLIENT_CANCELLATION_LOCAL_DISCONNECT;
            break;
          case location::nearby::proto::connections::DisconnectionReason::
              REMOTE_DISCONNECTION:
            payload_status = PayloadStatus::REMOTE_CLIENT_DISCONNECTION;
            operation_result_code =
                OperationResultCode::CLIENT_CANCELLATION_REMOTE_DISCONNECT;
            break;
          case location::nearby::proto::connections::DisconnectionReason::
              IO_ERROR:
          default:
            payload_status = PayloadStatus::ENDPOINT_IO_ERROR;
            operation_result_code =
                AnalyticsRecorder::GetChannelIoErrorResultCodeFromMedium(
                    client->GetConnectedMedium(endpoint_id));
            break;
        }

        incoming_payload_manager_.OnEndpointDisconnect(
            client, endpoint_id, payload_status, operation_result_code);
        outgoing_payload_manager_.OnEndpointDisconnect(
            client, endpoint_id, payload_status, operation_result_code);

        barrier->Notify();
      });
}

// @EndpointManagerDataPool
void PayloadManager::ProcessControlPacket(
    ClientProxy* to_client, const std::string& from_endpoint_id,
    PayloadTransferFrame& payload_transfer_frame) {
  if (incoming_payload_manager_.ProcessControlPacket(
          to_client, from_endpoint_id, payload_transfer_frame)) {
    return;
  }
  if (outgoing_payload_manager_.ProcessControlPacket(
          to_client, from_endpoint_id, payload_transfer_frame)) {
    return;
  }

  VLOG(1) << "Got ControlMessage for unknown payload_id="
          << payload_transfer_frame.payload_header().id()
          << ", ignoring: " << payload_transfer_frame.control_message().event();
}

void PayloadManager::SetCustomSavePath(ClientProxy* client,
                                       const std::string& path) {
  incoming_payload_manager_.SetCustomSavePath(client, path);
}

void PayloadManager::RunOnStatusUpdateThread(
    const std::string& name, absl::AnyInvocable<void()> runnable) {
  payload_status_update_executor_.Execute(name, std::move(runnable));
}

}  // namespace nearby::connections
