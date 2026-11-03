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

#include "connections/implementation/outgoing_payload_manager.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/internal_payload.h"
#include "connections/implementation/internal_payload_factory.h"
#include "connections/implementation/pending_payload.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/listeners.h"
#include "connections/payload.h"
#include "connections/payload_type.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/expected.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "internal/platform/single_thread_executor.h"
#include "proto/connections_enums.pb.h"

namespace nearby::connections {

namespace {
using ::location::nearby::connections::PayloadTransferFrame;
using ::location::nearby::proto::connections::OperationResultCode;
using ::location::nearby::proto::connections::PayloadStatus;
using ::nearby::analytics::AnalyticsRecorder;

std::string EndpointIdsToString(const std::vector<std::string>& endpoint_ids) {
  return absl::StrCat(endpoint_ids.size(), ":",
                      absl::StrJoin(endpoint_ids, ","));
}

}  // namespace

OutgoingPayloadManager::OutgoingPayloadManager(
    EndpointManager& endpoint_manager,
    SingleThreadExecutor& status_update_executor, std::atomic<bool>& shutdown)
    : endpoint_manager_(&endpoint_manager),
      status_update_executor_(&status_update_executor),
      shutdown_(&shutdown) {}

int OutgoingPayloadManager::SendPayloadLoop(
    ClientProxy* client, OutgoingPendingPayload& pending_payload,
    PayloadTransferFrame::PayloadHeader& payload_header,
    int64_t next_chunk_offset, size_t resume_offset, int index) {
  std::vector<std::pair<std::string, EndpointInfo::Status>> endpoint_statuses =
      pending_payload.GetEndpointStatuses();

  std::vector<std::string> available_endpoint_ids;
  available_endpoint_ids.reserve(endpoint_statuses.size());
  // First, handle any non-available endpoints.
  for (const auto& endpoint_status : endpoint_statuses) {
    if (endpoint_status.second != EndpointInfo::Status::kAvailable) {
      HandleFinishedOutgoingPayload(
          client, {endpoint_status.first}, payload_header, next_chunk_offset,
          EndpointInfoStatusToOperationResultCode(endpoint_status.second),
          EndpointInfoStatusToPayloadStatus(endpoint_status.second));
    } else {
      available_endpoint_ids.push_back(endpoint_status.first);
    }
  }

  // Update the still-active recipients of this payload.
  if (available_endpoint_ids.empty()) {
    VLOG(1) << "PayloadManager short-circuiting payload_id="
            << pending_payload.GetId() << " after sending " << next_chunk_offset
            << " bytes because none of the endpoints are available anymore.";
    return -1;
  }

  // Check if the payload has been cancelled by the client and, if so,
  // notify the remaining recipients.
  if (pending_payload.IsLocallyCanceled()) {
    VLOG(1) << "Aborting send of payload_id=" << pending_payload.GetId()
            << " at offset " << next_chunk_offset
            << " since it is marked canceled.";
    HandleFinishedOutgoingPayload(
        client, available_endpoint_ids, payload_header, next_chunk_offset,
        OperationResultCode::CLIENT_CANCELLATION_LOCAL_CANCEL_PAYLOAD,
        PayloadStatus::LOCAL_CANCELLATION);
    return -1;
  }

  OutgoingInternalPayload* outgoing_payload =
      pending_payload.GetOutgoingInternalPayload();

  if (next_chunk_offset == 0 && resume_offset > 0) {
    ExceptionOr<size_t> real_offset =
        outgoing_payload->SkipToOffset(resume_offset);
    if (!real_offset.ok()) {
      // Stop sending since it may cause remote file merging failed.
      LOG(WARNING) << "PayloadManager failed to skip offset " << resume_offset
                   << " on payload_id " << outgoing_payload->GetId();
      HandleFinishedOutgoingPayload(client, available_endpoint_ids,
                                    payload_header, next_chunk_offset,
                                    OperationResultCode::IO_FILE_READING_ERROR,
                                    PayloadStatus::LOCAL_ERROR);
      return -1;
    }
    VLOG(1) << "PayloadManager successfully skipped " << real_offset.GetResult()
            << " bytes on payload_id " << outgoing_payload->GetId();
    next_chunk_offset = real_offset.GetResult();
  }
  // Update the current offsets for all endpoints still active for this
  // payload. For the sake of accuracy, we update the pending payload here
  // because it's after all payload terminating events are handled, but
  // right before we actually start detaching the next chunk.
  for (const auto& endpoint_id : available_endpoint_ids) {
    pending_payload.SetOffsetForEndpoint(endpoint_id, next_chunk_offset);
  }

  // This will block if there is no data to transfer.
  // It will resume when new data arrives, or if Close() is called.
  int chunk_size = GetOptimalChunkSize(available_endpoint_ids);
  ByteArray next_chunk = outgoing_payload->DetachNextChunk(chunk_size);
  if (shutdown_->load()) return -1;
  // Check again in case CancelPayload() closed the payload while
  // DetachNextChunk() was blocked; otherwise an empty chunk from the closed
  // stream/file would be treated as normal EOF (SUCCESS) or a read error
  // (LOCAL_ERROR) instead of LOCAL_CANCELLATION.
  if (pending_payload.IsLocallyCanceled()) {
    VLOG(1) << "Aborting send of payload_id=" << outgoing_payload->GetId()
            << " at offset " << next_chunk_offset
            << " since it is marked canceled.";
    HandleFinishedOutgoingPayload(
        client, available_endpoint_ids, payload_header, next_chunk_offset,
        OperationResultCode::CLIENT_CANCELLATION_LOCAL_CANCEL_PAYLOAD,
        PayloadStatus::LOCAL_CANCELLATION);
    return -1;
  }
  // Save chunk size. We'll need it after we move next_chunk.
  size_t next_chunk_size = next_chunk.size();
  // If there are no more chunks, check if there should be more data to send.
  if (next_chunk_size == 0 && outgoing_payload->GetTotalSize() > 0 &&
      outgoing_payload->GetTotalSize() > next_chunk_offset) {
    VLOG(1) << "Payload xfer failed: payload_id=" << outgoing_payload->GetId();
    HandleFinishedOutgoingPayload(
        client, available_endpoint_ids, payload_header, next_chunk_offset,
        OperationResultCode::IO_FILE_READING_ERROR, PayloadStatus::LOCAL_ERROR);
    return -1;
  }

  // Only need to handle outgoing data chunk offset, because the offset will be
  // used to decide if the received chunk is the initial payload chunk.
  // In other cases, the offset should only be used in both side logs when error
  // happened.
  PayloadTransferFrame::PayloadChunk payload_chunk(CreatePayloadChunk(
      next_chunk_offset - resume_offset, std::move(next_chunk), index));
  const std::vector<std::string>& failed_endpoint_ids =
      endpoint_manager_->SendPayloadChunk(payload_header, payload_chunk,
                                          available_endpoint_ids);
  // Check whether at least one endpoint failed.
  if (!failed_endpoint_ids.empty()) {
    VLOG(1) << "Payload xfer: endpoints failed: payload_id="
            << payload_header.id() << "; endpoint_ids={"
            << EndpointIdsToString(failed_endpoint_ids) << "}";
    HandleFinishedOutgoingPayload(
        client, failed_endpoint_ids, payload_header, next_chunk_offset,
        OperationResultCode::CONNECTIVITY_GENERIC_WRITING_CHANNEL_IO_ERROR,
        PayloadStatus::ENDPOINT_IO_ERROR);
  }
  // Check whether at least one endpoint succeeded -- if they all failed,
  // we'll just go right back to the top of the loop and break out when
  // availableEndpointIds is re-synced and found to be empty at that point.
  if (failed_endpoint_ids.size() < available_endpoint_ids.size()) {
    for (const auto& endpoint_id : available_endpoint_ids) {
      if (!absl::c_linear_search(failed_endpoint_ids, endpoint_id)) {
        HandleSuccessfulOutgoingChunk(
            client, endpoint_id, payload_header, payload_chunk.flags(),
            payload_chunk.offset(), payload_chunk.body().size());
      }
    }

    if (next_chunk_size == 0) {
      // That was the last chunk, we're outta here.
      VLOG(1) << "Payload xfer done: payload_id=" << outgoing_payload->GetId()
              << "; size=" << next_chunk_offset;
      return -1;
    } else {
      VLOG(1) << "PayloadManager done sending chunk at offset "
              << next_chunk_offset
              << " of payload_id=" << outgoing_payload->GetId();
    }
  }

  return next_chunk_size;
}

std::string OutgoingPayloadManager::ToString(EndpointInfo::Status status) {
  switch (status) {
    case EndpointInfo::Status::kAvailable:
      return std::string("Available");
    case EndpointInfo::Status::kCanceled:
      return std::string("Cancelled");
    case EndpointInfo::Status::kError:
      return std::string("Error");
    case EndpointInfo::Status::kUnknown:
      return std::string("Unknown");
  }
}

// Creates and starts tracking an OutgoingPendingPayload for this Payload.
Payload::Id OutgoingPayloadManager::CreateOutgoingPayload(
    Payload payload, const std::vector<std::string>& endpoint_ids) {
  ErrorOr<std::unique_ptr<OutgoingInternalPayload>> result =
      CreateOutgoingInternalPayload(std::move(payload));
  if (result.has_error()) {
    LOG(ERROR) << "Failed to create outgoing internal payload: "
               << result.error().operation_result_code().value();
    return Payload::Id();
  }
  std::unique_ptr<OutgoingInternalPayload> internal_payload =
      std::move(result.value());
  Payload::Id payload_id = internal_payload->GetId();
  VLOG(1) << "CreateOutgoingPayload: payload_id=" << payload_id;
  absl::MutexLock lock(mutex_);
  outgoing_pending_payloads_.StartTrackingPayload(
      payload_id, std::make_unique<OutgoingPendingPayload>(
                      std::move(internal_payload), endpoint_ids, [this]() {
                        RunOnStatusUpdateThread("~OutgoingPendingPayload",
                                                [this]() { NotifyShutdown(); });
                      }));

  return payload_id;
}

void OutgoingPayloadManager::CancelAllPayloadsAndWait() {
  {
    absl::MutexLock lock(mutex_);
    int pending_outgoing_payloads = 0;
    outgoing_pending_payloads_.ForEachPayload(
        [&](OutgoingPendingPayload* pending) {
          pending_outgoing_payloads++;
          pending->MarkLocallyCanceledAndClose();
        });

    if (pending_outgoing_payloads) {
      shutdown_barrier_ =
          std::make_unique<CountDownLatch>(pending_outgoing_payloads);
    }
  }
  if (shutdown_barrier_) {
    VLOG(1) << "PayloadManager: waiting for pending outgoing "
               "payloads; self="
            << this;
    shutdown_barrier_->Await();
  }
}

void OutgoingPayloadManager::ShutdownExecutors() {
  bytes_payload_executor_.Shutdown();
  stream_payload_executor_.Shutdown();
  file_payload_executor_.Shutdown();
}

void OutgoingPayloadManager::StopTrackingAllPayloads() {
  absl::MutexLock lock(mutex_);
  outgoing_pending_payloads_.StopTrackingAllPayloads();
}

bool OutgoingPayloadManager::NotifyShutdown() {
  absl::MutexLock lock(mutex_);
  if (!shutdown_->load()) return false;
  if (!shutdown_barrier_) return false;
  VLOG(1) << "PayloadManager [shutdown mode]";
  shutdown_barrier_->CountDown();
  return true;
}

void OutgoingPayloadManager::SendPayload(
    ClientProxy* client, const std::vector<std::string>& endpoint_ids,
    Payload payload) {
  if (shutdown_->load()) return;
  VLOG(1) << "SendPayload: endpoint_ids={" << EndpointIdsToString(endpoint_ids)
          << "}";
  // Before transfer to internal payload, retrieves the Payload size for
  // analytics.
  int64_t payload_total_size;
  switch (payload.GetType()) {
    case connections::PayloadType::kBytes:
      payload_total_size = payload.AsBytes().size();
      break;
    case connections::PayloadType::kFile:
      payload_total_size = payload.AsFile()->GetTotalSize();
      break;
    case connections::PayloadType::kStream:
    case connections::PayloadType::kUnknown:
      payload_total_size = -1;
      break;
  }

  auto executor = GetOutgoingPayloadExecutor(payload.GetType());
  // The |executor| will be null if the payload is of a type we cannot work
  // with. This should never be reached since the ServiceControllerRouter has
  // already checked whether or not we can work with this Payload type.
  if (!executor) {
    RecordInvalidPayloadAnalytics(
        client, endpoint_ids, payload.GetId(), payload.GetType(),
        payload.GetOffset(), payload_total_size,
        OperationResultCode::NEARBY_GENERIC_OUTGOING_PAYLOAD_CREATION_FAILURE);
    VLOG(1) << "PayloadManager failed to determine the right executor for "
               "outgoing payload_id="
            << payload.GetId() << ", payload_type=" << payload.GetType();
    return;
  }

  // Each payload is sent in FCFS order within each Payload type, blocking any
  // other payload of the same type from even starting until this one is
  // completely done with. If we ever want to provide isolation across
  // ClientProxy objects this will need to be significantly re-architected.
  PayloadType payload_type = payload.GetType();
  size_t resume_offset =
      FeatureFlags::GetInstance().GetFlags().enable_send_payload_offset
          ? payload.GetOffset()
          : 0;

  Payload::Id payload_id =
      CreateOutgoingPayload(std::move(payload), endpoint_ids);
  executor->Execute("send-payload", [this, client, endpoint_ids, payload_id,
                                     payload_type, resume_offset,
                                     payload_total_size]() {
    if (shutdown_->load()) return;
    std::shared_ptr<OutgoingPendingPayload> pending_payload =
        GetOutgoingPayload(payload_id);
    if (!pending_payload) {
      RecordInvalidPayloadAnalytics(
          client, endpoint_ids, payload_id, payload_type, resume_offset,
          payload_total_size,
          OperationResultCode::
              NEARBY_GENERIC_OUTGOING_PAYLOAD_CREATION_FAILURE);
      VLOG(1) << "PayloadManager failed to create InternalPayload for outgoing "
                 "payload_id="
              << payload_id << ", payload_type=" << payload_type
              << ", aborting sendPayload().";
      return;
    }
    OutgoingInternalPayload* outgoing_payload =
        pending_payload->GetOutgoingInternalPayload();
    if (!outgoing_payload) return;

    // Payloads of the same type are sent sequentially on this executor, so a
    // payload may have been waiting in the queue for a while. Mark the
    // transfer start now so the logged duration/throughput only covers the
    // actual transfer, not the queueing delay. Bytes before `resume_offset`
    // are not sent and are therefore excluded from the throughput.
    pending_payload->SetTransferStart(SystemClock::ElapsedRealtime(),
                                      resume_offset);

    RecordPayloadStartedAnalytics(client, endpoint_ids, payload_id,
                                  payload_type, resume_offset,
                                  outgoing_payload->GetTotalSize());

    PayloadTransferFrame::PayloadHeader payload_header{
        CreatePayloadHeader(*outgoing_payload, resume_offset)};

    bool should_continue = true;
    int64_t next_chunk_offset = 0;
    int index = 0;

    while (should_continue && !shutdown_->load()) {
      int bytes_sent = SendPayloadLoop(client, *pending_payload, payload_header,
                                       next_chunk_offset, resume_offset, index);
      should_continue = (bytes_sent >= 0);
      if (should_continue) {
        if (next_chunk_offset == 0 && resume_offset > 0) {
          next_chunk_offset = resume_offset;
        }
        next_chunk_offset += bytes_sent;
      }
      index++;
    }

    RunOnStatusUpdateThread("destroy-payload", [this, payload_id]() {
      outgoing_pending_payloads_.StopTrackingPayload(payload_id);
    });
  });
  VLOG(1) << "PayloadManager: xfer scheduled: self=" << this
          << "; payload_id=" << payload_id << ", payload_type=" << payload_type;
}

std::shared_ptr<OutgoingPayloadManager::OutgoingPendingPayload>
OutgoingPayloadManager::GetOutgoingPayload(Payload::Id payload_id) const {
  return outgoing_pending_payloads_.GetPayload(payload_id);
}

bool OutgoingPayloadManager::CancelPayload(Payload::Id payload_id) {
  if (std::shared_ptr<OutgoingPendingPayload> canceled_outgoing_payload =
          GetOutgoingPayload(payload_id)) {
    canceled_outgoing_payload->MarkLocallyCanceledAndClose();
    VLOG(1) << "Cancelling outgoing payload_id=" << payload_id
            << " at request of client.";
    return true;
  }
  return false;
}

void OutgoingPayloadManager::OnEndpointDisconnect(
    ClientProxy* client, const std::string& endpoint_id,
    PayloadStatus payload_status, OperationResultCode operation_result_code) {
  absl::MutexLock lock(mutex_);
  outgoing_pending_payloads_.ForEachPayload(
      [&](OutgoingPendingPayload* pending_payload) {
        // Stop tracking the endpoint for this payload.
        std::optional<EndpointInfo> endpoint_info =
            pending_payload->RemoveEndpoint(endpoint_id);
        if (!endpoint_info) return;
        int64_t endpoint_offset = endpoint_info->offset;

        int64_t payload_total_size =
            pending_payload->GetOutgoingInternalPayload()->GetTotalSize();

        // If no endpoints are left for this payload, close it.
        if (pending_payload->IsEndpointsEmpty()) {
          pending_payload->Close();
        }
        // Create the payload transfer update.
        PayloadProgressInfo update{pending_payload->GetId(),
                                   PayloadProgressInfo::Status::kFailure,
                                   payload_total_size, endpoint_offset};

        // Send a client notification of a payload transfer failure.
        client->OnPayloadProgress(endpoint_id, update);

        client->GetAnalyticsRecorder().OnOutgoingPayloadDone(
            endpoint_id, pending_payload->GetId(), payload_status,
            operation_result_code);
      });
}

PayloadStatus OutgoingPayloadManager::EndpointInfoStatusToPayloadStatus(
    EndpointInfo::Status status) {
  switch (status) {
    case EndpointInfo::Status::kCanceled:
      return PayloadStatus::REMOTE_CANCELLATION;
    case EndpointInfo::Status::kError:
      return PayloadStatus::REMOTE_ERROR;
    case EndpointInfo::Status::kAvailable:
      return PayloadStatus::SUCCESS;
    default:
      VLOG(1) << "PayloadManager: Unknown PayloadStatus";
      return PayloadStatus::UNKNOWN_PAYLOAD_STATUS;
  }
}

OperationResultCode
OutgoingPayloadManager::EndpointInfoStatusToOperationResultCode(
    EndpointInfo::Status status) {
  switch (status) {
    case EndpointInfo::Status::kCanceled:
      return OperationResultCode::CLIENT_CANCELLATION_REMOTE_IN_CANCELED_STATE;
    case EndpointInfo::Status::kError:
      return OperationResultCode::NEARBY_GENERIC_REMOTE_ENDPOINT_STATUS_ERROR;
    case EndpointInfo::Status::kAvailable:
      return OperationResultCode::DETAIL_SUCCESS;
    default:
      VLOG(1) << "PayloadManager: Unknown PayloadStatus";
      return OperationResultCode::DETAIL_UNKNOWN;
  }
}

SingleThreadExecutor* OutgoingPayloadManager::GetOutgoingPayloadExecutor(
    PayloadType payload_type) {
  switch (payload_type) {
    case PayloadType::kBytes:
      return &bytes_payload_executor_;
    case PayloadType::kFile:
      return &file_payload_executor_;
    case PayloadType::kStream:
      return &stream_payload_executor_;
    default:
      return nullptr;
  }
}

int OutgoingPayloadManager::GetOptimalChunkSize(
    const std::vector<std::string>& endpoint_ids) {
  int min_chunk_size = std::numeric_limits<int>::max();
  for (const auto& endpoint_id : endpoint_ids) {
    min_chunk_size =
        std::min(min_chunk_size,
                 endpoint_manager_->GetMaxTransmitPacketSize(endpoint_id));
  }
  return min_chunk_size;
}

PayloadTransferFrame::PayloadHeader OutgoingPayloadManager::CreatePayloadHeader(
    const OutgoingInternalPayload& outgoing_payload, size_t offset) {
  PayloadTransferFrame::PayloadHeader payload_header;
  size_t payload_size = outgoing_payload.GetTotalSize();

  payload_header.set_id(outgoing_payload.GetId());
  payload_header.set_type(outgoing_payload.GetType());
  if (outgoing_payload.GetType() ==
      nearby::connections::PayloadTransferFrame::PayloadTransferFrame::
          PayloadHeader::FILE) {
    payload_header.set_file_name(outgoing_payload.GetFileName());
    payload_header.set_parent_folder(outgoing_payload.GetParentFolder());
    payload_header.set_last_modified_timestamp_millis(
        absl::ToUnixMillis(outgoing_payload.GetLastModifiedTime()));
  }
  payload_header.set_total_size(payload_size == kPayloadIndeterminateSize
                                    ? kPayloadIndeterminateSize
                                    : payload_size - offset);
  return payload_header;
}

PayloadTransferFrame::PayloadChunk OutgoingPayloadManager::CreatePayloadChunk(
    int64_t payload_chunk_offset, ByteArray payload_chunk_body, int index) {
  PayloadTransferFrame::PayloadChunk payload_chunk;

  payload_chunk.set_offset(payload_chunk_offset);
  payload_chunk.set_flags(0);
  if (!payload_chunk_body.Empty()) {
    payload_chunk.set_body(std::string(std::move(payload_chunk_body)));
  } else {
    payload_chunk.set_flags(payload_chunk.flags() |
                            PayloadTransferFrame::PayloadChunk::LAST_CHUNK);
  }
  payload_chunk.set_index(index);

  return payload_chunk;
}

void OutgoingPayloadManager::SendClientCallbacksForFinishedOutgoingPayload(
    ClientProxy* client, const std::vector<std::string>& finished_endpoint_ids,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    int64_t num_bytes_successfully_transferred, PayloadStatus status,
    OperationResultCode operation_result_code) {
  RunOnStatusUpdateThread(
      "outgoing-payload-callbacks",
      [this, client, finished_endpoint_ids, payload_header,
       num_bytes_successfully_transferred, status, operation_result_code]() {
        // Make sure we're still tracking this payload.
        std::shared_ptr<OutgoingPendingPayload> pending_payload =
            GetOutgoingPayload(payload_header.id());
        if (!pending_payload) {
          return;
        }

        PayloadProgressInfo update{
            payload_header.id(), PayloadStatusToTransferUpdateStatus(status),
            payload_header.total_size(), num_bytes_successfully_transferred};
        for (const auto& endpoint_id : finished_endpoint_ids) {
          // Skip sending notifications if we have stopped tracking this
          // endpoint.
          if (!pending_payload->HasEndpoint(endpoint_id)) {
            continue;
          }

          // Notify the client.
          client->OnPayloadProgress(endpoint_id, update);

          // Mark this payload as done for analytics.
          client->GetAnalyticsRecorder().OnOutgoingPayloadDone(
              endpoint_id, payload_header.id(), status,
              (operation_result_code == OperationResultCode::DETAIL_UNKNOWN &&
               status == PayloadStatus::ENDPOINT_IO_ERROR)
                  ? AnalyticsRecorder::GetChannelIoErrorResultCodeFromMedium(
                        client->GetConnectedMedium(endpoint_id))
                  : operation_result_code);
        }

        // Remove these endpoints from our tracking list for this payload.
        pending_payload->RemoveEndpoints(finished_endpoint_ids);

        // Close the payload if no endpoints remain.
        if (pending_payload->IsEndpointsEmpty()) {
          pending_payload->Close();
        }
      });
}

void OutgoingPayloadManager::HandleFinishedOutgoingPayload(
    ClientProxy* client, const std::vector<std::string>& finished_endpoint_ids,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    int64_t num_bytes_successfully_transferred,
    OperationResultCode operation_result_code, PayloadStatus status) {
  // This call will destroy a pending payload.
  SendClientCallbacksForFinishedOutgoingPayload(
      client, finished_endpoint_ids, payload_header,
      num_bytes_successfully_transferred, status, operation_result_code);

  switch (status) {
    case PayloadStatus::LOCAL_ERROR:
      SendControlMessage(*endpoint_manager_, finished_endpoint_ids,
                         payload_header, num_bytes_successfully_transferred,
                         PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
      break;
    case PayloadStatus::LOCAL_CANCELLATION:
      VLOG(1) << "Sending PAYLOAD_CANCEL to receiver side; payload_id="
              << payload_header.id();
      SendControlMessage(
          *endpoint_manager_, finished_endpoint_ids, payload_header,
          num_bytes_successfully_transferred,
          PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
      break;
    case PayloadStatus::ENDPOINT_IO_ERROR:
      // Unregister these endpoints, since we had an IO error on the physical
      // connection.
      for (const auto& endpoint_id : finished_endpoint_ids) {
        endpoint_manager_->DiscardEndpoint(
            client, endpoint_id,
            location::nearby::proto::connections::DisconnectionReason::
                IO_ERROR);
      }
      break;
    case PayloadStatus::REMOTE_ERROR:
    case PayloadStatus::REMOTE_CANCELLATION:
      // No special handling needed for these.
      break;
    default:
      VLOG(1) << "PayloadManager: Unhandled finished outgoing payload with "
                 "payload_status="
              << status;
      break;
  }
}

void OutgoingPayloadManager::HandleSuccessfulOutgoingChunk(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    int32_t payload_chunk_flags, int64_t payload_chunk_offset,
    int64_t payload_chunk_body_size) {
  {
    absl::MutexLock lock(chunk_update_mutex_);
    ++outgoing_chunk_update_count_;
  }
  // Capture the time the chunk was actually sent, before the update gets
  // queued on the status update thread.
  absl::Time chunk_sent_time = SystemClock::ElapsedRealtime();
  RunOnStatusUpdateThread("outgoing-chunk-success", [this, client, endpoint_id,
                                                     payload_header,
                                                     payload_chunk_flags,
                                                     payload_chunk_offset,
                                                     payload_chunk_body_size,
                                                     chunk_sent_time]() {
    // Make sure we're still tracking this payload and its associated
    // endpoint.
    bool is_last_chunk = (payload_chunk_flags &
                          PayloadTransferFrame::PayloadChunk::LAST_CHUNK) != 0;

    {
      absl::MutexLock lock(chunk_update_mutex_);
      --outgoing_chunk_update_count_;
      if (payload_header.has_type() &&
          payload_header.type() ==
              PayloadTransferFrame::PayloadTransferFrame::PayloadHeader::FILE) {
        if (outgoing_chunk_update_count_ > 0 && !is_last_chunk &&
            payload_chunk_offset != 0) {
          VLOG(1) << "Skip the outgoing chunk update with offset="
                  << payload_chunk_offset;
          client->GetAnalyticsRecorder().OnPayloadChunkSent(
              endpoint_id, payload_header.id(), payload_chunk_body_size);
          return;
        }

        absl::Time current_time = SystemClock::ElapsedRealtime();
        if (!is_last_chunk && payload_chunk_offset != 0 &&
            current_time - last_outgoing_chunk_update_time_ <
                kMinTransferUpdateInterval) {
          VLOG(1) << "Skip the outgoing chunk update with offset="
                  << payload_chunk_offset
                  << " because it's too close to the previous update.";
          client->GetAnalyticsRecorder().OnPayloadChunkSent(
              endpoint_id, payload_header.id(), payload_chunk_body_size);
          return;
        }

        last_outgoing_chunk_update_time_ = current_time;
      }
    }

    std::shared_ptr<OutgoingPendingPayload> pending_payload =
        GetOutgoingPayload(payload_header.id());
    if (!pending_payload || !pending_payload->HasEndpoint(endpoint_id)) {
      VLOG(1) << "HandleSuccessfulOutgoingChunk: endpoint not found: "
                 "endpoint_id="
              << endpoint_id;
      return;
    }

    PayloadProgressInfo update{
        payload_header.id(),
        is_last_chunk ? PayloadProgressInfo::Status::kSuccess
                      : PayloadProgressInfo::Status::kInProgress,
        payload_header.total_size(),
        is_last_chunk ? payload_chunk_offset
                      : payload_chunk_offset + payload_chunk_body_size};

    // Notify the client.
    client->OnPayloadProgress(endpoint_id, update);

    if (is_last_chunk) {
      LogPayloadTransferSummary(
          /*is_incoming=*/false, endpoint_id, payload_header,
          payload_chunk_offset + pending_payload->GetTransferStartOffset(),
          pending_payload->GetTransferStartTime(),
          pending_payload->GetTransferStartOffset(), chunk_sent_time,
          client->GetConnectedMedium(endpoint_id));
      client->GetAnalyticsRecorder().OnOutgoingPayloadDone(
          endpoint_id, payload_header.id(), PayloadStatus::SUCCESS,
          OperationResultCode::DETAIL_SUCCESS);

      // Stop tracking this endpoint.
      pending_payload->RemoveEndpoint(endpoint_id);

      // Close the payload if no endpoints remain.
      if (pending_payload->IsEndpointsEmpty()) {
        pending_payload->Close();
      }
    } else {
      client->GetAnalyticsRecorder().OnPayloadChunkSent(
          endpoint_id, payload_header.id(), payload_chunk_body_size);
    }
  });
}

// @EndpointManagerDataPool
bool OutgoingPayloadManager::ProcessControlPacket(
    ClientProxy* to_client, const std::string& from_endpoint_id,
    PayloadTransferFrame& payload_transfer_frame) {
  const PayloadTransferFrame::PayloadHeader& payload_header =
      payload_transfer_frame.payload_header();
  const PayloadTransferFrame::ControlMessage& control_message =
      payload_transfer_frame.control_message();
  std::shared_ptr<OutgoingPendingPayload> outgoing_payload =
      GetOutgoingPayload(payload_header.id());
  if (!outgoing_payload) {
    return false;
  }
  if (control_message.event() ==
          PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED ||
      control_message.event() ==
          PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR) {
    VLOG(1) << "Outgoing control message " << control_message.event()
            << " for payload_id=" << outgoing_payload->GetId()
            << " from endpoint_id=" << from_endpoint_id << "; self=" << this;
    outgoing_payload->SetEndpointStatusFromControlMessage(from_endpoint_id,
                                                          control_message);
  } else {
    VLOG(1) << "Unhandled control message " << control_message.event()
            << " for outgoing payload_id=" << outgoing_payload->GetId();
  }
  return true;
}

void OutgoingPayloadManager::RecordPayloadStartedAnalytics(
    ClientProxy* client, const std::vector<std::string>& endpoint_ids,
    int64_t payload_id, PayloadType payload_type, int64_t offset,
    int64_t total_size) {
  client->GetAnalyticsRecorder().OnOutgoingPayloadStarted(
      endpoint_ids, payload_id, payload_type,
      total_size == -1 ? -1 : total_size - offset);
}

void OutgoingPayloadManager::RecordInvalidPayloadAnalytics(
    ClientProxy* client, const std::vector<std::string>& endpoint_ids,
    int64_t payload_id, PayloadType payload_type, int64_t offset,
    int64_t total_size, OperationResultCode operation_result_code) {
  RecordPayloadStartedAnalytics(client, endpoint_ids, payload_id, payload_type,
                                offset, total_size);

  for (const auto& endpoint_id : endpoint_ids) {
    client->GetAnalyticsRecorder().OnOutgoingPayloadDone(
        endpoint_id, payload_id, PayloadStatus::LOCAL_ERROR,
        operation_result_code);
  }
}

void OutgoingPayloadManager::RunOnStatusUpdateThread(
    const std::string& name, absl::AnyInvocable<void()> runnable) {
  status_update_executor_->Execute(name, std::move(runnable));
}

///////////////////////////////// EndpointInfo /////////////////////////////////

OutgoingPayloadManager::EndpointInfo::Status
OutgoingPayloadManager::EndpointInfo::ControlMessageEventToEndpointInfoStatus(
    PayloadTransferFrame::ControlMessage::EventType event) {
  switch (event) {
    case PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR:
      return Status::kError;
    case PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED:
      return Status::kCanceled;
    default:
      VLOG(1) << "Unknown EndpointInfo.Status for ControlMessage.EventType "
              << event;
      return Status::kUnknown;
  }
}

void OutgoingPayloadManager::EndpointInfo::SetStatusFromControlMessage(
    const PayloadTransferFrame::ControlMessage& control_message) {
  status.store(
      ControlMessageEventToEndpointInfoStatus(control_message.event()));
  VLOG(1) << "Marked endpoint " << id << " with status "
          << ToString(status.load()) << " based on OOB ControlMessage";
}

////////////////////////// OutgoingPendingPayload //////////////////////////////

OutgoingPayloadManager::OutgoingPendingPayload::OutgoingPendingPayload(
    std::unique_ptr<OutgoingInternalPayload> outgoing_payload,
    const std::vector<std::string>& endpoint_ids,
    absl::AnyInvocable<void() &&> destroy_callback)
    : outgoing_payload_(std::move(outgoing_payload)),
      destroy_callback_(std::move(destroy_callback)) {
  // Initially we mark all endpoints as available.
  // Later on some may become canceled, some may experience data transfer
  // failures. Any of these situations will cause endpoint to be marked as
  // unavailable.
  for (const auto& id : endpoint_ids) {
    EndpointInfo endpoint_info;
    endpoint_info.id = id;
    endpoint_info.status.store(EndpointInfo::Status::kAvailable);

    endpoints_.emplace(id, std::move(endpoint_info));
  }
}

std::vector<
    std::pair<std::string, OutgoingPayloadManager::EndpointInfo::Status>>
OutgoingPayloadManager::OutgoingPendingPayload::GetEndpointStatuses() const {
  std::vector<std::pair<std::string, EndpointInfo::Status>> results;
  absl::MutexLock lock(mutex_);
  results.reserve(endpoints_.size());
  for (const auto& endpoints_it : endpoints_) {
    results.emplace_back(endpoints_it.second.id,
                         endpoints_it.second.status.load());
  }
  return results;
}

bool OutgoingPayloadManager::OutgoingPendingPayload::IsEndpointsEmpty() const {
  absl::MutexLock lock(mutex_);

  return endpoints_.empty();
}

bool OutgoingPayloadManager::OutgoingPendingPayload::HasEndpoint(
    absl::string_view endpoint_id) {
  absl::MutexLock lock(mutex_);

  return endpoints_.contains(endpoint_id);
}

std::optional<OutgoingPayloadManager::EndpointInfo>
OutgoingPayloadManager::OutgoingPendingPayload::RemoveEndpoint(
    absl::string_view endpoint_id) {
  absl::MutexLock lock(mutex_);

  auto node = endpoints_.extract(endpoint_id);
  if (node.empty()) {
    return std::nullopt;
  }
  return std::move(node.mapped());
}

void OutgoingPayloadManager::OutgoingPendingPayload::RemoveEndpoints(
    const std::vector<std::string>& endpoint_ids) {
  absl::MutexLock lock(mutex_);

  for (const auto& id : endpoint_ids) {
    endpoints_.erase(id);
  }
}

void OutgoingPayloadManager::OutgoingPendingPayload::
    SetEndpointStatusFromControlMessage(
        const std::string& endpoint_id,
        const PayloadTransferFrame::ControlMessage& control_message) {
  absl::MutexLock lock(mutex_);

  auto item = endpoints_.find(endpoint_id);
  if (item != endpoints_.end()) {
    item->second.SetStatusFromControlMessage(control_message);
  }
}

void OutgoingPayloadManager::OutgoingPendingPayload::SetOffsetForEndpoint(
    const std::string& endpoint_id, int64_t offset) {
  absl::MutexLock lock(mutex_);

  auto item = endpoints_.find(endpoint_id);
  if (item != endpoints_.end()) {
    item->second.offset = offset;
  }
}

std::string OutgoingPayloadManager::OutgoingPendingPayload::ToString() const {
  return absl::StrCat("Payload(outgoing", GetId(), ")");
}

///////////////////////// OutgoingPendingPayloads //////////////////////////////

void OutgoingPayloadManager::OutgoingPendingPayloads::StartTrackingPayload(
    Payload::Id payload_id,
    std::unique_ptr<OutgoingPendingPayload> pending_payload) {
  absl::MutexLock lock(mutex_);

  // If the |payload_id| is being re-used, always prefer the newer payload.
  Remove(pending_payloads_.find(payload_id));
  VLOG(1) << "StartTrackingPayload: " << pending_payload->ToString();
  pending_payloads_[payload_id] = std::move(pending_payload);
}

void OutgoingPayloadManager::OutgoingPendingPayloads::StopTrackingPayload(
    Payload::Id payload_id) {
  absl::MutexLock lock(mutex_);
  VLOG(1) << "StopTrackingPayload " << payload_id;
  Remove(pending_payloads_.find(payload_id));
}

void OutgoingPayloadManager::OutgoingPendingPayloads::Remove(
    absl::flat_hash_map<Payload::Id,
                        std::shared_ptr<OutgoingPendingPayload>>::iterator it) {
  if (it != pending_payloads_.end()) {
    VLOG(1) << "Removing payload: " << it->second->ToString();
    it->second->Close();
    pending_payloads_.erase(it);
  }
}

std::shared_ptr<OutgoingPayloadManager::OutgoingPendingPayload>
OutgoingPayloadManager::OutgoingPendingPayloads::GetPayload(
    Payload::Id payload_id) const {
  absl::MutexLock lock(mutex_);

  auto item = pending_payloads_.find(payload_id);
  if (item == pending_payloads_.end()) {
    return nullptr;
  }
  return item->second;
}

void OutgoingPayloadManager::OutgoingPendingPayloads::
    StopTrackingAllPayloads() {
  absl::MutexLock lock(mutex_);

  for (auto it = pending_payloads_.begin(); it != pending_payloads_.end();) {
    Remove(it++);
  }
}

void OutgoingPayloadManager::OutgoingPendingPayloads::ForEachPayload(
    absl::AnyInvocable<void(OutgoingPendingPayload*)> callback) {
  absl::MutexLock lock(mutex_);

  for (const auto& item : pending_payloads_) {
    callback(item.second.get());
  }
}

}  // namespace nearby::connections
