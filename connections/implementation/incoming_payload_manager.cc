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

#include "connections/implementation/incoming_payload_manager.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/internal_payload.h"
#include "connections/implementation/internal_payload_factory.h"
#include "connections/implementation/pending_payload.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/listeners.h"
#include "connections/payload.h"
#include "connections/payload_type.h"
#include "internal/platform/expected.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "internal/platform/single_thread_executor.h"
#include "proto/connections_enums.pb.h"

namespace nearby::connections {

namespace {
using ::location::nearby::connections::PayloadTransferFrame;
using ::location::nearby::proto::connections::OperationResultCode;
using ::location::nearby::proto::connections::PayloadStatus;

PayloadStatus ControlMessageEventToPayloadStatus(
    PayloadTransferFrame::ControlMessage::EventType event) {
  switch (event) {
    case PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR:
      return PayloadStatus::REMOTE_ERROR;
    case PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED:
      return PayloadStatus::REMOTE_CANCELLATION;
    default:
      VLOG(1) << "IncomingPayloadManager: unknown event=" << event;
      return PayloadStatus::UNKNOWN_PAYLOAD_STATUS;
  }
}

OperationResultCode ControlMessageEventToOperationResultCode(
    PayloadTransferFrame::ControlMessage::EventType event) {
  switch (event) {
    case PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR:
      return OperationResultCode::NEARBY_GENERIC_REMOTE_REPORT_PAYLOADS_ERROR;
    case PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED:
      return OperationResultCode::CLIENT_CANCELLATION_REMOTE_CANCEL_PAYLOAD;
    default:
      VLOG(1) << "IncomingPayloadManager: unknown event=" << event;
      return OperationResultCode::DETAIL_UNKNOWN;
  }
}

PayloadType FramePayloadTypeToPayloadType(
    PayloadTransferFrame::PayloadHeader::PayloadType type) {
  switch (type) {
    case PayloadTransferFrame::PayloadHeader::BYTES:
      return connections::PayloadType::kBytes;
    case PayloadTransferFrame::PayloadHeader::FILE:
      return connections::PayloadType::kFile;
    case PayloadTransferFrame::PayloadHeader::STREAM:
      return connections::PayloadType::kStream;
    default:
      return connections::PayloadType::kUnknown;
  }
}

}  // namespace

IncomingPayloadManager::IncomingPayloadManager(
    EndpointManager& endpoint_manager,
    SingleThreadExecutor& status_update_executor, std::atomic<bool>& shutdown)
    : endpoint_manager_(&endpoint_manager),
      status_update_executor_(&status_update_executor),
      shutdown_(&shutdown) {}

void IncomingPayloadManager::SetCustomSavePath(ClientProxy* client,
                                               const std::string& path) {
  custom_save_path_ = path;
}

bool IncomingPayloadManager::CancelPayload(Payload::Id payload_id) {
  if (std::shared_ptr<IncomingPendingPayload> canceled_incoming_payload =
          GetIncomingPayload(payload_id)) {
    canceled_incoming_payload->MarkLocallyCanceledAndClose();
    VLOG(1) << "Cancelling incoming payload_id=" << payload_id
            << " at request of client.";
    return true;
  }
  return false;
}

void IncomingPayloadManager::CancelAllPayloads() {
  incoming_pending_payloads_.ForEachPayload(
      [&](IncomingPendingPayload* pending) {
        pending->MarkLocallyCanceledAndClose();
      });
}

void IncomingPayloadManager::StopTrackingAllPayloads() {
  incoming_pending_payloads_.StopTrackingAllPayloads();
}

void IncomingPayloadManager::OnEndpointDisconnect(
    ClientProxy* client, const std::string& endpoint_id,
    PayloadStatus payload_status, OperationResultCode operation_result_code) {
  std::vector<Payload::Id> finished_incoming_payloads;
  incoming_pending_payloads_.ForEachPayload(
      [&](IncomingPendingPayload* pending_payload) {
        if (pending_payload->GetEndpointId() != endpoint_id) return;
        int64_t endpoint_offset = pending_payload->GetOffset();

        int64_t payload_total_size =
            pending_payload->GetIncomingInternalPayload()->GetTotalSize();

        pending_payload->Close();
        finished_incoming_payloads.push_back(pending_payload->GetId());
        // Create the payload transfer update.
        PayloadProgressInfo update{pending_payload->GetId(),
                                   PayloadProgressInfo::Status::kFailure,
                                   payload_total_size, endpoint_offset};

        // Send a client notification of a payload transfer failure.
        client->OnPayloadProgress(endpoint_id, update);

        client->GetAnalyticsRecorder().OnIncomingPayloadDone(
            endpoint_id, pending_payload->GetId(), payload_status,
            operation_result_code);
      });
  for (Payload::Id payload_id : finished_incoming_payloads) {
    incoming_pending_payloads_.StopTrackingPayload(payload_id);
  }
}

std::shared_ptr<IncomingPayloadManager::IncomingPendingPayload>
IncomingPayloadManager::GetIncomingPayload(Payload::Id payload_id) const {
  return incoming_pending_payloads_.GetPayload(payload_id);
}

ErrorOr<std::shared_ptr<IncomingPayloadManager::IncomingPendingPayload>>
IncomingPayloadManager::CreateIncomingPayload(const PayloadTransferFrame& frame,
                                              const std::string& endpoint_id,
                                              const std::string& save_path) {
  ErrorOr<std::unique_ptr<IncomingInternalPayload>> result =
      CreateIncomingInternalPayload(
          frame, save_path.empty() ? custom_save_path_ : save_path);
  if (result.has_error()) {
    return {result.error()};
  }
  std::unique_ptr<IncomingInternalPayload> internal_payload =
      std::move(result.value());
  Payload::Id payload_id = internal_payload->GetId();
  VLOG(1) << "CreateIncomingPayload: payload_id=" << payload_id;
  incoming_pending_payloads_.StartTrackingPayload(
      payload_id, std::make_unique<IncomingPendingPayload>(
                      std::move(internal_payload), endpoint_id));
  return {GetIncomingPayload(payload_id)};
}

void IncomingPayloadManager::HandleIncomingPayloadError(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    int64_t offset_bytes, PayloadStatus status,
    OperationResultCode operation_result_code) {
  RunOnStatusUpdateThread(
      "incoming-payload-callbacks",
      [this, client, endpoint_id, payload_header, offset_bytes, status,
       operation_result_code]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(status_update_executor_) {
            HandleIncomingPayloadErrorOnStatusUpdateThread(
                client, endpoint_id, payload_header, offset_bytes, status,
                operation_result_code);
          });

  switch (status) {
    case PayloadStatus::LOCAL_ERROR:
      SendControlMessage(*endpoint_manager_, {endpoint_id}, payload_header,
                         offset_bytes,
                         PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
      break;
    case PayloadStatus::LOCAL_CANCELLATION:
      SendControlMessage(
          *endpoint_manager_, {endpoint_id}, payload_header, offset_bytes,
          PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
      break;
    default:
      VLOG(1) << "Unhandled finished incoming payload_id="
              << payload_header.id() << " with payload_status=" << status;
      break;
  }
}

// @PayloadManagerStatusUpdateThread
void IncomingPayloadManager::HandleIncomingPayloadErrorOnStatusUpdateThread(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    int64_t offset_bytes, PayloadStatus status,
    OperationResultCode operation_result_code) {
  // Make sure we're still tracking this payload.
  std::shared_ptr<IncomingPendingPayload> pending_payload =
      GetIncomingPayload(payload_header.id());
  if (!pending_payload) {
    return;
  }

  // Unless we never started tracking this payload (meaning we
  // failed to even create the InternalPayload), close it and notify the
  // client.
  incoming_pending_payloads_.StopTrackingPayload(payload_header.id());
  PayloadProgressInfo update{payload_header.id(),
                             PayloadStatusToTransferUpdateStatus(status),
                             payload_header.total_size(), offset_bytes};
  NotifyClientOfIncomingPayloadProgressInfo(client, endpoint_id, update);

  // Analyze
  client->GetAnalyticsRecorder().OnIncomingPayloadDone(
      endpoint_id, payload_header.id(), status, operation_result_code);
}

void IncomingPayloadManager::HandleSuccessfulIncomingChunk(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    int32_t payload_chunk_flags, int64_t payload_chunk_offset,
    int64_t payload_chunk_body_size) {
  {
    absl::MutexLock lock(chunk_update_mutex_);
    ++incoming_chunk_update_count_;
  }
  // Capture the time the chunk was actually received, before the update gets
  // queued on the status update thread.
  absl::Time chunk_received_time = SystemClock::ElapsedRealtime();
  RunOnStatusUpdateThread(
      "incoming-chunk-success",
      [this, client, endpoint_id, payload_header, payload_chunk_flags,
       payload_chunk_offset, payload_chunk_body_size, chunk_received_time]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(status_update_executor_) {
            // Make sure we're still tracking this payload.
            bool is_last_chunk =
                (payload_chunk_flags &
                 PayloadTransferFrame::PayloadChunk::LAST_CHUNK) != 0;

            {
              absl::MutexLock lock(chunk_update_mutex_);
              --incoming_chunk_update_count_;
              if (payload_header.has_type() &&
                  payload_header.type() ==
                      PayloadTransferFrame::PayloadTransferFrame::
                          PayloadHeader::FILE) {
                if (incoming_chunk_update_count_ > 0 && !is_last_chunk &&
                    payload_chunk_offset != 0) {
                  VLOG(1) << "Skip the incoming chunk update with offset="
                          << payload_chunk_offset;
                  client->GetAnalyticsRecorder().OnPayloadChunkReceived(
                      endpoint_id, payload_header.id(),
                      payload_chunk_body_size);
                  return;
                }

                absl::Time current_time = SystemClock::ElapsedRealtime();
                if (!is_last_chunk && payload_chunk_offset != 0 &&
                    current_time - last_incoming_chunk_update_time_ <
                        kMinTransferUpdateInterval) {
                  VLOG(1) << "Skip the incoming chunk update with offset="
                          << payload_chunk_offset
                          << " because it's too close to the previous update.";
                  client->GetAnalyticsRecorder().OnPayloadChunkReceived(
                      endpoint_id, payload_header.id(),
                      payload_chunk_body_size);
                  return;
                }

                last_incoming_chunk_update_time_ = current_time;
              }
            }

            std::shared_ptr<IncomingPendingPayload> pending_payload =
                GetIncomingPayload(payload_header.id());
            if (!pending_payload || shutdown_->load()) {
              return;
            }
            // CancelPayload() may have been called after ProcessDataPacket()
            // queued this callback on the status update thread. Handle the
            // cancellation inline since we are already on the status update
            // thread, rather than calling HandleIncomingPayloadError() which
            // would re-post to the back of the status update executor queue.
            if (pending_payload->IsLocallyCanceled()) {
              HandleIncomingPayloadErrorOnStatusUpdateThread(
                  client, endpoint_id, payload_header, payload_chunk_offset,
                  PayloadStatus::LOCAL_CANCELLATION,
                  OperationResultCode::
                      CLIENT_CANCELLATION_LOCAL_CANCEL_PAYLOAD);
              if (!is_last_chunk) {
                SendControlMessage(
                    *endpoint_manager_, {endpoint_id}, payload_header,
                    payload_chunk_offset,
                    PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
              }
              return;
            }

            PayloadProgressInfo update{
                payload_header.id(),
                is_last_chunk ? PayloadProgressInfo::Status::kSuccess
                              : PayloadProgressInfo::Status::kInProgress,
                payload_header.total_size(),
                is_last_chunk ? payload_chunk_offset
                              : payload_chunk_offset + payload_chunk_body_size};

            // Notify the client of this update.
            NotifyClientOfIncomingPayloadProgressInfo(client, endpoint_id,
                                                      update);

            // Analyze the success.
            if (is_last_chunk) {
              LogPayloadTransferSummary(
                  /*is_incoming=*/true, endpoint_id, payload_header,
                  payload_chunk_offset, pending_payload->GetTransferStartTime(),
                  pending_payload->GetTransferStartOffset(),
                  chunk_received_time, client->GetConnectedMedium(endpoint_id));
              incoming_pending_payloads_.StopTrackingPayload(
                  payload_header.id());
              client->GetAnalyticsRecorder().OnIncomingPayloadDone(
                  endpoint_id, payload_header.id(), PayloadStatus::SUCCESS,
                  OperationResultCode::DETAIL_SUCCESS);
            } else {
              client->GetAnalyticsRecorder().OnPayloadChunkReceived(
                  endpoint_id, payload_header.id(), payload_chunk_body_size);
            }
          });
}

// @EndpointManagerDataPool
void IncomingPayloadManager::ProcessDataPacket(
    ClientProxy* to_client, const std::string& from_endpoint_id,
    PayloadTransferFrame& payload_transfer_frame,
    location::nearby::proto::connections::Medium medium) {
  PayloadTransferFrame::PayloadHeader& payload_header =
      *payload_transfer_frame.mutable_payload_header();
  PayloadTransferFrame::PayloadChunk& payload_chunk =
      *payload_transfer_frame.mutable_payload_chunk();
  VLOG(1) << "PayloadManager got data OfflineFrame for payload_id="
          << payload_header.id() << " from endpoint_id=" << from_endpoint_id
          << " at offset " << payload_chunk.offset();
  // We explicitly deny payloads with ID 0.
  if (payload_header.id() == 0) {
    LOG(WARNING) << "Denying payload with ID 0 for endpoint_id="
                 << from_endpoint_id << ", aborting receipt.";
    // Send the error to the remote endpoint.
    SendControlMessage(*endpoint_manager_, {from_endpoint_id}, payload_header,
                       payload_chunk.offset(),
                       PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
    return;
  }
  Payload::Id payload_id = payload_header.id();
  std::shared_ptr<IncomingPendingPayload> pending_payload;
  if (payload_chunk.offset() == 0) {
    absl::Time first_chunk_arrival_time = SystemClock::ElapsedRealtime();
    if (GetIncomingPayload(payload_id)) {
      LOG(WARNING) << "ProcessDataPacket: [duplicate] endpoint_id="
                   << from_endpoint_id << "; payload_id=" << payload_id
                   << ", aborting receipt.";
      HandleIncomingPayloadError(to_client, from_endpoint_id, payload_header,
                                 payload_chunk.offset(),
                                 PayloadStatus::LOCAL_ERROR,
                                 OperationResultCode::IO_FILE_OPENING_ERROR);
      return;
    }
    RunOnStatusUpdateThread(
        "process-data-packet",
        [to_client, from_endpoint_id, payload_header]()
            ABSL_EXCLUSIVE_LOCKS_REQUIRED(status_update_executor_) {
              // This is the first chunk of a new incoming
              // payload. Start the analysis.
              to_client->GetAnalyticsRecorder().OnIncomingPayloadStarted(
                  from_endpoint_id, payload_header.id(),
                  FramePayloadTypeToPayloadType(payload_header.type()),
                  payload_header.total_size());
            });

    ErrorOr<std::shared_ptr<IncomingPendingPayload>> result =
        CreateIncomingPayload(payload_transfer_frame, from_endpoint_id,
                              to_client->GetSavePath(from_endpoint_id));
    if (result.has_error()) {
      LOG(WARNING) << "PayloadManager failed to create InternalPayload from "
                      "PayloadTransferFrame with payload_id="
                   << payload_header.id() << " and type "
                   << payload_header.type() << ", aborting receipt.";

      // Analyticize.
      OperationResultCode operation_result_code =
          result.error().operation_result_code().value();
      RunOnStatusUpdateThread(
          "process-data-packet",
          [to_client, from_endpoint_id, payload_header, operation_result_code]()
              ABSL_EXCLUSIVE_LOCKS_REQUIRED(status_update_executor_) {
                to_client->GetAnalyticsRecorder().OnIncomingPayloadDone(
                    from_endpoint_id, payload_header.id(),
                    PayloadStatus::LOCAL_ERROR, operation_result_code);
              });

      // Send the error to the remote endpoint.
      SendControlMessage(*endpoint_manager_, {from_endpoint_id}, payload_header,
                         payload_chunk.offset(),
                         PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
      return;
    }
    pending_payload = std::move(result.value());

    // The body of this first chunk has already been fully received over the
    // medium at this point, so exclude it from the measured throughput; only
    // the bytes received after this point are counted against the duration.
    pending_payload->SetTransferStart(first_chunk_arrival_time,
                                      payload_chunk.body().size());
    // Also, let the client know of this new incoming payload.
    RunOnStatusUpdateThread(
        "process-data-packet",
        [to_client, from_endpoint_id,
         pending_payload = GetIncomingPayload(payload_id)]()
            ABSL_EXCLUSIVE_LOCKS_REQUIRED(status_update_executor_) {
              if (!pending_payload) return;
              IncomingInternalPayload* incoming_payload =
                  pending_payload->GetIncomingInternalPayload();
              if (incoming_payload == nullptr) return;
              VLOG(1) << "PayloadManager received new payload_id="
                      << incoming_payload->GetId()
                      << " from endpoint_id=" << from_endpoint_id;
              to_client->OnPayload(from_endpoint_id,
                                   incoming_payload->ReleasePayload());
            });
  } else {
    pending_payload = GetIncomingPayload(payload_header.id());
  }

  if (!pending_payload) {
    LOG(WARNING) << "ProcessDataPacket: [missing] endpoint_id="
                 << from_endpoint_id << "; payload_id=" << payload_header.id();
    return;
  }
  if (pending_payload->IsLocallyCanceled()) {
    // This incoming payload was canceled by the client. Drop this frame and
    // do all the cleanup. See go/nc-cancel-payload
    VLOG(1) << "ProcessDataPacket: [cancel] endpoint_id=" << from_endpoint_id
            << "; payload_id=" << pending_payload->GetId();
    HandleIncomingPayloadError(
        to_client, from_endpoint_id, payload_header, payload_chunk.offset(),
        PayloadStatus::LOCAL_CANCELLATION,
        OperationResultCode::CLIENT_CANCELLATION_LOCAL_CANCEL_PAYLOAD);
    return;
  }

  // Update the offset for this payload. An endpoint disconnection might occur
  // from another thread and we would need to know the current offset to
  // report back to the client. For the sake of accuracy, we update the
  // pending payload here because it's after all payload terminating events
  // are handled, but right before we actually start attaching the next chunk.
  if (pending_payload->GetEndpointId() == from_endpoint_id) {
    pending_payload->SetOffset(payload_chunk.offset());
  }

  // Save size of packet before we move it.
  int64_t payload_body_size = payload_chunk.body().size();

  IncomingInternalPayload* incoming_payload =
      pending_payload->GetIncomingInternalPayload();
  if (incoming_payload == nullptr ||
      incoming_payload->AttachNextChunk(payload_chunk.body()).Raised()) {
    LOG(ERROR) << "ProcessDataPacket: [data: error] endpoint_id="
               << from_endpoint_id
               << "; payload_id=" << pending_payload->GetId();
    HandleIncomingPayloadError(
        to_client, from_endpoint_id, payload_header, payload_chunk.offset(),
        PayloadStatus::LOCAL_ERROR, OperationResultCode::IO_FILE_WRITING_ERROR);
    return;
  }
  HandleSuccessfulIncomingChunk(to_client, from_endpoint_id, payload_header,
                                payload_chunk.flags(), payload_chunk.offset(),
                                payload_body_size);
}

// @EndpointManagerDataPool
bool IncomingPayloadManager::ProcessControlPacket(
    ClientProxy* to_client, const std::string& from_endpoint_id,
    PayloadTransferFrame& payload_transfer_frame) {
  const PayloadTransferFrame::PayloadHeader& payload_header =
      payload_transfer_frame.payload_header();
  const PayloadTransferFrame::ControlMessage& control_message =
      payload_transfer_frame.control_message();
  std::shared_ptr<IncomingPendingPayload> incoming_payload =
      GetIncomingPayload(payload_header.id());
  if (!incoming_payload) {
    return false;
  }
  if (control_message.event() ==
          PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED ||
      control_message.event() ==
          PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR) {
    VLOG(1) << "Incoming control message " << control_message.event()
            << " for payload_id=" << incoming_payload->GetId()
            << " from endpoint_id=" << from_endpoint_id << "; self=" << this;
    // No need to mark the pending payload as cancelled, since this is a
    // remote cancellation for an incoming payload -- we handle everything
    // inline here.
    HandleIncomingPayloadError(
        to_client, from_endpoint_id, payload_header, control_message.offset(),
        ControlMessageEventToPayloadStatus(control_message.event()),
        ControlMessageEventToOperationResultCode(control_message.event()));
  } else {
    VLOG(1) << "Unhandled control message " << control_message.event()
            << " for incoming payload_id=" << incoming_payload->GetId();
  }
  return true;
}

// @PayloadManagerStatusUpdateThread
void IncomingPayloadManager::NotifyClientOfIncomingPayloadProgressInfo(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadProgressInfo& payload_transfer_update) {
  client->OnPayloadProgress(endpoint_id, payload_transfer_update);
}

void IncomingPayloadManager::RunOnStatusUpdateThread(
    const std::string& name, absl::AnyInvocable<void()> runnable) {
  status_update_executor_->Execute(name, std::move(runnable));
}

////////////////////////// IncomingPendingPayload //////////////////////////////

IncomingPayloadManager::IncomingPendingPayload::IncomingPendingPayload(
    std::unique_ptr<IncomingInternalPayload> incoming_payload,
    absl::string_view endpoint_id)
    : incoming_payload_(std::move(incoming_payload)),
      endpoint_id_(endpoint_id) {}

std::string IncomingPayloadManager::IncomingPendingPayload::ToString() const {
  return absl::StrCat("Payload(incoming", GetId(), ")");
}

void IncomingPayloadManager::IncomingPendingPayloads::StartTrackingPayload(
    Payload::Id payload_id,
    std::unique_ptr<IncomingPendingPayload> pending_payload) {
  absl::MutexLock lock(mutex_);

  // If the |payload_id| is being re-used, always prefer the newer payload.
  Remove(pending_payloads_.find(payload_id));
  VLOG(1) << "StartTrackingPayload: " << pending_payload->ToString();
  pending_payloads_[payload_id] = std::move(pending_payload);
}

void IncomingPayloadManager::IncomingPendingPayloads::StopTrackingPayload(
    Payload::Id payload_id) {
  absl::MutexLock lock(mutex_);
  VLOG(1) << "StopTrackingPayload " << payload_id;
  Remove(pending_payloads_.find(payload_id));
}

void IncomingPayloadManager::IncomingPendingPayloads::Remove(
    absl::flat_hash_map<Payload::Id,
                        std::shared_ptr<IncomingPendingPayload>>::iterator it) {
  if (it != pending_payloads_.end()) {
    VLOG(1) << "Removing payload: " << it->second->ToString();
    it->second->Close();
    pending_payloads_.erase(it);
  }
}

std::shared_ptr<IncomingPayloadManager::IncomingPendingPayload>
IncomingPayloadManager::IncomingPendingPayloads::GetPayload(
    Payload::Id payload_id) const {
  absl::MutexLock lock(mutex_);

  auto item = pending_payloads_.find(payload_id);
  if (item == pending_payloads_.end()) {
    return nullptr;
  }
  return item->second;
}

void IncomingPayloadManager::IncomingPendingPayloads::
    StopTrackingAllPayloads() {
  absl::MutexLock lock(mutex_);

  for (auto it = pending_payloads_.begin(); it != pending_payloads_.end();) {
    Remove(it++);
  }
}

void IncomingPayloadManager::IncomingPendingPayloads::ForEachPayload(
    absl::AnyInvocable<void(IncomingPendingPayload*)> callback) {
  absl::MutexLock lock(mutex_);

  for (const auto& item : pending_payloads_) {
    callback(item.second.get());
  }
}

}  // namespace nearby::connections
