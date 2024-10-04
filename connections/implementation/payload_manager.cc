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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "connections/implementation/analytics/throughput_recorder.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/internal_payload_factory.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/listeners.h"
#include "connections/payload.h"
#include "connections/payload_type.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/single_thread_executor.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {

using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::V1Frame;
using ::location::nearby::proto::connections::PayloadStatus;
using ::nearby::analytics::PacketMetaData;
using ::nearby::analytics::ThroughputRecorderContainer;
using ::nearby::connections::PayloadDirection;

// C++14 requires to declare this.
// TODO(apolyudov): remove when migration to c++17 is possible.
constexpr absl::Duration PayloadManager::kWaitCloseTimeout;

bool PayloadManager::SendPayloadLoop(
    ClientProxy* client, PendingPayload& pending_payload,
    PayloadTransferFrame::PayloadHeader& payload_header,
    std::int64_t& next_chunk_offset, size_t resume_offset, int index) {
  // in lieu of structured binding:
  auto pair = GetAvailableAndUnavailableEndpoints(pending_payload);
  const EndpointIds& available_endpoint_ids =
      EndpointsToEndpointIds(pair.first);
  const Endpoints& unavailable_endpoints = pair.second;
  PacketMetaData packet_meta_data;

  // First, handle any non-available endpoints.
  for (const auto& endpoint : unavailable_endpoints) {
    HandleFinishedOutgoingPayload(
        client, {endpoint->id}, payload_header, next_chunk_offset,
        EndpointInfoStatusToPayloadStatus(endpoint->status.Get()));
  }

  // Update the still-active recipients of this payload.
  if (available_endpoint_ids.empty()) {
    LOG(INFO)
        << "PayloadManager short-circuiting payload_id="
        << pending_payload.GetInternalPayload()->GetId() << " after sending "
        << next_chunk_offset
        << " bytes because none of the endpoints are available anymore.";
    return false;
  }

  // Check if the payload has been cancelled by the client and, if so,
  // notify the remaining recipients.
  if (pending_payload.IsLocallyCanceled()) {
    LOG(INFO) << "Aborting send of payload_id="
                      << pending_payload.GetInternalPayload()->GetId()
                      << " at offset " << next_chunk_offset
                      << " since it is marked canceled.";
    HandleFinishedOutgoingPayload(client, available_endpoint_ids,
                                  payload_header, next_chunk_offset,
                                  location::nearby::proto::connections::
                                      PayloadStatus::LOCAL_CANCELLATION);
    return false;
  }

  // Update the current offsets for all endpoints still active for this
  // payload. For the sake of accuracy, we update the pending payload here
  // because it's after all payload terminating events are handled, but
  // right before we actually start detaching the next chunk.
  if (next_chunk_offset == 0 && resume_offset > 0) {
    ExceptionOr<size_t> real_offset =
        pending_payload.GetInternalPayload()->SkipToOffset(resume_offset);
    if (!real_offset.ok()) {
      // Stop sending since it may cause remote file merging failed.
      LOG(WARNING) << "PayloadManager failed to skip offset "
                           << resume_offset << " on payload_id "
                           << pending_payload.GetInternalPayload()->GetId();
      HandleFinishedOutgoingPayload(
          client, available_endpoint_ids, payload_header, next_chunk_offset,
          location::nearby::proto::connections::PayloadStatus::LOCAL_ERROR);
      return false;
    }
    NEARBY_VLOG(1) << "PayloadManager successfully skipped "
                   << real_offset.GetResult() << " bytes on payload_id "
                   << pending_payload.GetInternalPayload()->GetId();
    next_chunk_offset = real_offset.GetResult();
  }
  for (const auto& endpoint_id : available_endpoint_ids) {
    pending_payload.SetOffsetForEndpoint(endpoint_id, next_chunk_offset);
  }

  // This will block if there is no data to transfer.
  // It will resume when new data arrives, or if Close() is called.
  int chunk_size = GetOptimalChunkSize(available_endpoint_ids);
  packet_meta_data.StartFileIo();
  ByteArray next_chunk =
      pending_payload.GetInternalPayload()->DetachNextChunk(chunk_size);
  packet_meta_data.StopFileIo();
  if (shutdown_.Get()) return false;
  // Save chunk size. We'll need it after we move next_chunk.
  auto next_chunk_size = next_chunk.size();
  if (!next_chunk_size &&
      pending_payload.GetInternalPayload()->GetTotalSize() > 0 &&
      pending_payload.GetInternalPayload()->GetTotalSize() <
          next_chunk_offset) {
    LOG(INFO) << "Payload xfer failed: payload_id="
                      << pending_payload.GetInternalPayload()->GetId();
    HandleFinishedOutgoingPayload(
        client, available_endpoint_ids, payload_header, next_chunk_offset,
        location::nearby::proto::connections::PayloadStatus::LOCAL_ERROR);
    return false;
  }

  // Only need to handle outgoing data chunk offset, because the offset will be
  // used to decide if the received chunk is the initial payload chunk.
  // In other cases, the offset should only be used in both side logs when error
  // happened.
  PayloadTransferFrame::PayloadChunk payload_chunk(CreatePayloadChunk(
      next_chunk_offset - resume_offset, std::move(next_chunk), index));
  const EndpointIds& failed_endpoint_ids = endpoint_manager_->SendPayloadChunk(
      payload_header, payload_chunk, available_endpoint_ids, packet_meta_data);
  // Check whether at least one endpoint failed.
  if (!failed_endpoint_ids.empty()) {
    LOG(INFO) << "Payload xfer: endpoints failed: payload_id="
                      << payload_header.id() << "; endpoint_ids={"
                      << ToString(failed_endpoint_ids) << "}",
        HandleFinishedOutgoingPayload(client, failed_endpoint_ids,
                                      payload_header, next_chunk_offset,
                                      location::nearby::proto::connections::
                                          PayloadStatus::ENDPOINT_IO_ERROR);
  }
  bool is_last_chunk = IsLastChunk(payload_chunk);
  // Check whether at least one endpoint succeeded -- if they all failed,
  // we'll just go right back to the top of the loop and break out when
  // availableEndpointIds is re-synced and found to be empty at that point.
  if (failed_endpoint_ids.size() < available_endpoint_ids.size()) {
    for (const auto& endpoint_id : available_endpoint_ids) {
      if (std::find(failed_endpoint_ids.begin(), failed_endpoint_ids.end(),
                    endpoint_id) == failed_endpoint_ids.end()) {
        if (!WaitForReceivedAck(client, endpoint_id, pending_payload,
                                payload_header, next_chunk_offset,
                                is_last_chunk)) {
          continue;
        }

        HandleSuccessfulOutgoingChunk(
            client, endpoint_id, payload_header, payload_chunk.flags(),
            payload_chunk.offset(), payload_chunk.body().size());
      }
    }
    NEARBY_VLOG(1) << "PayloadManager done sending chunk at offset "
                   << next_chunk_offset << " of payload_id="
                   << pending_payload.GetInternalPayload()->GetId();
    next_chunk_offset += next_chunk_size;

    if (!next_chunk_size) {
      // That was the last chunk, we're outta here.
      LOG(INFO) << "Payload xfer done: payload_id="
                        << pending_payload.GetInternalPayload()->GetId()
                        << "; size=" << next_chunk_offset;
      ThroughputRecorderContainer::GetInstance()
          .GetTPRecorder(pending_payload.GetInternalPayload()->GetId(),
                         PayloadDirection::OUTGOING_PAYLOAD)
          ->MarkAsSuccess();
      return false;
    }
  }

  return true;
}

std::pair<PayloadManager::Endpoints, PayloadManager::Endpoints>
PayloadManager::GetAvailableAndUnavailableEndpoints(
    const PendingPayload& pending_payload) {
  Endpoints available;
  Endpoints unavailable;
  for (auto* endpoint_info : pending_payload.GetEndpoints()) {
    if (endpoint_info->status.Get() ==
        PayloadManager::EndpointInfo::Status::kAvailable) {
      available.push_back(endpoint_info);
    } else {
      unavailable.push_back(endpoint_info);
    }
  }
  return std::make_pair(std::move(available), std::move(unavailable));
}

PayloadManager::EndpointIds PayloadManager::EndpointsToEndpointIds(
    const Endpoints& endpoints) {
  EndpointIds endpoint_ids;
  endpoint_ids.reserve(endpoints.size());
  for (const auto& item : endpoints) {
    if (item) {
      endpoint_ids.emplace_back(item->id);
    }
  }
  return endpoint_ids;
}

std::string PayloadManager::ToString(const Endpoints& endpoints) {
  std::string endpoints_string = absl::StrCat(endpoints.size(), ": ");
  bool first = true;
  for (const auto& item : endpoints) {
    if (first) {
      absl::StrAppend(&endpoints_string, item->id);
      first = false;
    } else {
      absl::StrAppend(&endpoints_string, ", ", item->id);
    }
  }
  return endpoints_string;
}

std::string PayloadManager::ToString(const EndpointIds& endpoint_ids) {
  std::string endpoints_string = absl::StrCat(endpoint_ids.size(), ": ");
  bool first = true;
  for (const auto& id : endpoint_ids) {
    if (first) {
      absl::StrAppend(&endpoints_string, id);
      first = false;
    } else {
      absl::StrAppend(&endpoints_string, ", ", id);
    }
  }
  return endpoints_string;
}

std::string PayloadManager::ToString(PayloadType type) {
  switch (type) {
    case PayloadType::kBytes:
      return std::string("Bytes");
    case PayloadType::kStream:
      return std::string("Stream");
    case PayloadType::kFile:
      return std::string("File");
    case PayloadType::kUnknown:
      return std::string("Unknown");
  }
}

std::string PayloadManager::ToString(EndpointInfo::Status status) {
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

// Creates and starts tracking a PendingPayload for this Payload.
Payload::Id PayloadManager::CreateOutgoingPayload(
    Payload payload, const EndpointIds& endpoint_ids) {
  auto internal_payload{CreateOutgoingInternalPayload(std::move(payload))};
  Payload::Id payload_id = internal_payload->GetId();
  LOG(INFO) << "CreateOutgoingPayload: payload_id=" << payload_id;
  MutexLock lock(&mutex_);
  pending_payloads_.StartTrackingPayload(
      payload_id,
      std::make_unique<PendingPayload>(
          std::move(internal_payload), endpoint_ids,
          /*is_incoming=*/false,
          absl::bind_front(&PayloadManager::OnPendingPayloadDestroy, this)));

  return payload_id;
}

PayloadManager::PayloadManager(EndpointManager& endpoint_manager)
    : endpoint_manager_(&endpoint_manager) {
  endpoint_manager_->RegisterFrameProcessor(V1Frame::PAYLOAD_TRANSFER, this);
  custom_save_path_ = "";
}

void PayloadManager::CancelAllPayloads() {
  LOG(INFO) << "PayloadManager: canceling payloads; self=" << this;
  {
    MutexLock lock(&mutex_);
    int pending_outgoing_payloads = 0;
    pending_payloads_.ForEachPayload([&](PendingPayload* pending) {
      if (!pending->IsIncoming()) pending_outgoing_payloads++;
      pending->MarkLocallyCanceled();
      pending->Close();  // To unblock the sender thread, if there is no data.
    });

    if (pending_outgoing_payloads) {
      shutdown_barrier_ =
          std::make_unique<CountDownLatch>(pending_outgoing_payloads);
    }
  }
  if (shutdown_barrier_) {
    LOG(INFO) << "PayloadManager: waiting for pending outgoing "
                         "payloads; self="
                      << this;
    shutdown_barrier_->Await();
  }
}

void PayloadManager::DisconnectFromEndpointManager() {
  if (shutdown_.Set(true)) return;
  // Unregister ourselves from the FrameProcessors.
  endpoint_manager_->UnregisterFrameProcessor(V1Frame::PAYLOAD_TRANSFER, this);
}

PayloadManager::~PayloadManager() {
  LOG(INFO) << "PayloadManager: going down; self=" << this;
  ThroughputRecorderContainer::GetInstance().Shutdown();
  DisconnectFromEndpointManager();
  CancelAllPayloads();
  LOG(INFO) << "PayloadManager: turn down payload executors; self="
                    << this;
  bytes_payload_executor_.Shutdown();
  stream_payload_executor_.Shutdown();
  file_payload_executor_.Shutdown();
  send_payload_ack_executor_.Shutdown();

  CountDownLatch stop_latch(1);
  // Clear our tracked pending payloads.
  RunOnStatusUpdateThread(
      "~payload-manager",
      [this, &stop_latch]() RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD() {
        LOG(INFO) << "PayloadManager: stop tracking payloads; self="
                          << this;
        MutexLock lock(&mutex_);
        pending_payloads_.StopTrackingAllPayloads();
        stop_latch.CountDown();
      });
  stop_latch.Await();

  LOG(INFO) << "PayloadManager: turn down notification executor; self="
                    << this;
  // Stop all the ongoing Runnables (as gracefully as possible).
  payload_status_update_executor_.Shutdown();

  LOG(INFO) << "PayloadManager: down; self=" << this;
}

bool PayloadManager::NotifyShutdown() {
  MutexLock lock(&mutex_);
  if (!shutdown_.Get()) return false;
  if (!shutdown_barrier_) return false;
  LOG(INFO) << "PayloadManager [shutdown mode]";
  shutdown_barrier_->CountDown();
  return true;
}

void PayloadManager::SendPayload(ClientProxy* client,
                                 const EndpointIds& endpoint_ids,
                                 Payload payload) {
  if (shutdown_.Get()) return;
  LOG(INFO) << "SendPayload: endpoint_ids={" << ToString(endpoint_ids)
                    << "}";
  // Before transfer to internal payload, retrieves the Payload size for
  // analytics.
  std::int64_t payload_total_size;
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
    RecordInvalidPayloadAnalytics(client, endpoint_ids, payload.GetId(),
                                  payload.GetType(), payload.GetOffset(),
                                  payload_total_size);
    LOG(INFO)
        << "PayloadManager failed to determine the right executor for "
           "outgoing payload_id="
        << payload.GetId() << ", payload_type=" << ToString(payload.GetType());
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
  executor->Execute(
      "send-payload", [this, client, endpoint_ids, payload_id, payload_type,
                       resume_offset, payload_total_size]() {
        if (shutdown_.Get()) return;
        PendingPayloadHandle pending_payload = GetPayload(payload_id);
        if (!pending_payload) {
          RecordInvalidPayloadAnalytics(client, endpoint_ids, payload_id,
                                        payload_type, resume_offset,
                                        payload_total_size);
          LOG(INFO)
              << "PayloadManager failed to create InternalPayload for outgoing "
                 "payload_id="
              << payload_id << ", payload_type=" << ToString(payload_type)
              << ", aborting sendPayload().";
          return;
        }
        auto* internal_payload = pending_payload->GetInternalPayload();
        if (!internal_payload) return;

        RecordPayloadStartedAnalytics(client, endpoint_ids, payload_id,
                                      payload_type, resume_offset,
                                      internal_payload->GetTotalSize());

        PayloadTransferFrame::PayloadHeader payload_header{
            CreatePayloadHeader(*internal_payload, resume_offset,
                                internal_payload->GetParentFolder(),
                                internal_payload->GetFileName())};

        bool should_continue = true;
        std::int64_t next_chunk_offset = 0;
        int index = 0;

        ThroughputRecorderContainer::GetInstance()
            .GetTPRecorder(payload_id, PayloadDirection::OUTGOING_PAYLOAD)
            ->Start(payload_type, PayloadDirection::OUTGOING_PAYLOAD);
        while (should_continue && !shutdown_.Get()) {
          should_continue =
              SendPayloadLoop(client, *pending_payload, payload_header,
                              next_chunk_offset, resume_offset, index);
          index++;
        }

        RunOnStatusUpdateThread("destroy-payload",
                                [this, payload_id]()
                                    RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD() {
                                      DestroyPendingPayload(payload_id);
                                    });
      });
  LOG(INFO) << "PayloadManager: xfer scheduled: self=" << this
                    << "; payload_id=" << payload_id
                    << ", payload_type=" << ToString(payload_type);
}

PayloadManager::PendingPayloadHandle PayloadManager::GetPayload(
    Payload::Id payload_id) const {
  return pending_payloads_.GetPayload(payload_id);
}

Status PayloadManager::CancelPayload(ClientProxy* client,
                                     Payload::Id payload_id) {
  PendingPayloadHandle canceled_payload = GetPayload(payload_id);
  if (!canceled_payload) {
    LOG(INFO) << "Client requested cancel for unknown payload_id="
                      << payload_id << ", ignoring.";
    return {Status::kPayloadUnknown};
  }

  // Mark the payload as canceled.
  canceled_payload->MarkLocallyCanceled();
  LOG(INFO) << "Cancelling "
                    << (canceled_payload->IsIncoming() ? "incoming"
                                                       : "outgoing")
                    << " payload_id=" << payload_id << " at request of client.";

  // Return SUCCESS immediately. Remaining cleanup and updates will be sent
  // in SendPayload() or OnIncomingFrame()
  return {Status::kSuccess};
}

// @EndpointManagerDataPool
void PayloadManager::OnIncomingFrame(
    OfflineFrame& offline_frame, const std::string& from_endpoint_id,
    ClientProxy* to_client,
    location::nearby::proto::connections::Medium current_medium,
    PacketMetaData& packet_meta_data) {
  PayloadTransferFrame& frame =
      *offline_frame.mutable_v1()->mutable_payload_transfer();

  // Block any payload before the connection been accepted by both sides
  // to prevent unauthorized transfer.
  if (!to_client->IsConnectedToEndpoint(from_endpoint_id)) {
    if (frame.packet_type() == PayloadTransferFrame::DATA) {
      PendingPayloadHandle pending_payload =
          pending_payloads_.GetPayload(frame.payload_header().id());
      bool is_last = IsLastChunk(frame.payload_chunk());
      // If payload need to be ack'd receiving, then send back the ACK frame.
      if (pending_payload && is_last &&
          IsPayloadReceivedAckEnabled(to_client, from_endpoint_id,
                                      *pending_payload)) {
        SendPayloadReceivedAck(to_client, *pending_payload, from_endpoint_id,
                               is_last);
      }
    }
    LOG(INFO)
        << "PayloadManager skipped process payloads before PCP connected, "
        << frame.payload_header().id();
    return;
  }

  switch (frame.packet_type()) {
    case PayloadTransferFrame::CONTROL:
      LOG(INFO) << "PayloadManager::OnIncomingFrame [CONTROL]: self="
                        << this << "; endpoint_id=" << from_endpoint_id;
      ProcessControlPacket(to_client, from_endpoint_id, frame);
      break;
    case PayloadTransferFrame::DATA:
      ProcessDataPacket(to_client, from_endpoint_id, frame, current_medium,
                        packet_meta_data);
      break;
    case PayloadTransferFrame::PAYLOAD_ACK:
      LOG(INFO) << "[safe-to-disconnect][PAYLOAD_RECEIVED_ACK] sender "
                           "received payload ack from "
                        << from_endpoint_id;
      ProcessPayloadAckPacket(from_endpoint_id, frame);
      break;
    default:
      LOG(WARNING)
          << "PayloadManager: invalid frame; remote endpoint: self=" << this
          << "; endpoint_id=" << from_endpoint_id;
      break;
  }
}

void PayloadManager::OnEndpointDisconnect(ClientProxy* client,
                                          const std::string& service_id,
                                          const std::string& endpoint_id,
                                          CountDownLatch barrier,
                                          DisconnectionReason reason) {
  if (shutdown_.Get()) {
    barrier.CountDown();
    return;
  }
  RunOnStatusUpdateThread(
      "payload-manager-on-disconnect",
      [this, client, endpoint_id, barrier,
       reason]() RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD() mutable {
        // Iterate through all our payloads and look for payloads associated
        // with this endpoint.
        MutexLock lock(&mutex_);
        pending_payloads_.ForEachPayload([&](PendingPayload* pending_payload) {
          auto endpoint_info = pending_payload->GetEndpoint(endpoint_id);
          if (!endpoint_info) return;
          std::int64_t endpoint_offset = endpoint_info->offset;
          // Stop tracking the endpoint for this payload.
          pending_payload->RemoveEndpoints({endpoint_id});
          // |endpoint_info| is longer valid after calling
          // RemoveEndpoints.
          endpoint_info = nullptr;

          std::int64_t payload_total_size =
              pending_payload->GetInternalPayload()->GetTotalSize();

          // If no endpoints are left for this payload, close it.
          if (pending_payload->GetEndpoints().empty()) {
            pending_payload->Close();
          }
          // Create the payload transfer update.
          PayloadProgressInfo update{pending_payload->GetId(),
                                     PayloadProgressInfo::Status::kFailure,
                                     payload_total_size, endpoint_offset};

          // Send a client notification of a payload transfer failure.
          client->OnPayloadProgress(endpoint_id, update);

          PayloadStatus payload_status;
          switch (reason) {
            case DisconnectionReason::LOCAL_DISCONNECTION:
              payload_status = PayloadStatus::LOCAL_CLIENT_DISCONNECTION;
              break;
            case DisconnectionReason::REMOTE_DISCONNECTION:
              payload_status = PayloadStatus::REMOTE_CLIENT_DISCONNECTION;
              break;
            case DisconnectionReason::IO_ERROR:
            default:
              payload_status = PayloadStatus::ENDPOINT_IO_ERROR;
              break;
          }

          if (pending_payload->IsIncoming()) {
            client->GetAnalyticsRecorder().OnIncomingPayloadDone(
                endpoint_id, pending_payload->GetId(), payload_status);
          } else {
            client->GetAnalyticsRecorder().OnOutgoingPayloadDone(
                endpoint_id, pending_payload->GetId(), payload_status);
          }
        });

        barrier.CountDown();
      });
}

location::nearby::proto::connections::PayloadStatus
PayloadManager::EndpointInfoStatusToPayloadStatus(EndpointInfo::Status status) {
  switch (status) {
    case EndpointInfo::Status::kCanceled:
      return location::nearby::proto::connections::PayloadStatus::
          REMOTE_CANCELLATION;
    case EndpointInfo::Status::kError:
      return location::nearby::proto::connections::PayloadStatus::REMOTE_ERROR;
    case EndpointInfo::Status::kAvailable:
      return location::nearby::proto::connections::PayloadStatus::SUCCESS;
    default:
      LOG(INFO) << "PayloadManager: Unknown PayloadStatus";
      return location::nearby::proto::connections::PayloadStatus::
          UNKNOWN_PAYLOAD_STATUS;
  }
}

location::nearby::proto::connections::PayloadStatus
PayloadManager::ControlMessageEventToPayloadStatus(
    PayloadTransferFrame::ControlMessage::EventType event) {
  switch (event) {
    case PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR:
      return location::nearby::proto::connections::PayloadStatus::REMOTE_ERROR;
    case PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED:
      return location::nearby::proto::connections::PayloadStatus::
          REMOTE_CANCELLATION;
    default:
      LOG(INFO) << "PayloadManager: unknown event=" << event;
      return location::nearby::proto::connections::PayloadStatus::
          UNKNOWN_PAYLOAD_STATUS;
  }
}

PayloadProgressInfo::Status PayloadManager::PayloadStatusToTransferUpdateStatus(
    location::nearby::proto::connections::PayloadStatus status) {
  switch (status) {
    case location::nearby::proto::connections::LOCAL_CANCELLATION:
    case location::nearby::proto::connections::REMOTE_CANCELLATION:
      return PayloadProgressInfo::Status::kCanceled;
    case location::nearby::proto::connections::SUCCESS:
      return PayloadProgressInfo::Status::kSuccess;
    default:
      return PayloadProgressInfo::Status::kFailure;
  }
}

SingleThreadExecutor* PayloadManager::GetOutgoingPayloadExecutor(
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

int PayloadManager::GetOptimalChunkSize(EndpointIds endpoint_ids) {
  int minChunkSize = std::numeric_limits<int>::max();
  for (const auto& endpoint_id : endpoint_ids) {
    minChunkSize = std::min(
        minChunkSize, endpoint_manager_->GetMaxTransmitPacketSize(endpoint_id));
  }
  return minChunkSize;
}

PayloadTransferFrame::PayloadHeader PayloadManager::CreatePayloadHeader(
    const InternalPayload& internal_payload, size_t offset,
    const std::string& parent_folder, const std::string& file_name) {
  PayloadTransferFrame::PayloadHeader payload_header;
  size_t payload_size = internal_payload.GetTotalSize();

  payload_header.set_id(internal_payload.GetId());
  payload_header.set_type(internal_payload.GetType());
  if (internal_payload.GetType() ==
      nearby::connections::PayloadTransferFrame::PayloadTransferFrame::
          PayloadHeader::FILE) {
    payload_header.set_file_name(file_name);
    payload_header.set_parent_folder(parent_folder);
  }
  payload_header.set_total_size(payload_size ==
                                        InternalPayload::kIndeterminateSize
                                    ? InternalPayload::kIndeterminateSize
                                    : payload_size - offset);

  return payload_header;
}

PayloadTransferFrame::PayloadChunk PayloadManager::CreatePayloadChunk(
    std::int64_t payload_chunk_offset, ByteArray payload_chunk_body,
    int index) {
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

PayloadManager::PendingPayloadHandle PayloadManager::CreateIncomingPayload(
    const PayloadTransferFrame& frame, const std::string& endpoint_id) {
  auto internal_payload =
      CreateIncomingInternalPayload(frame, custom_save_path_);
  if (!internal_payload) {
    return PendingPayloadHandle();
  }

  Payload::Id payload_id = internal_payload->GetId();
  LOG(INFO) << "CreateIncomingPayload: payload_id=" << payload_id;
  pending_payloads_.StartTrackingPayload(
      payload_id,
      std::make_unique<PendingPayload>(
          std::move(internal_payload), EndpointIds{endpoint_id}, true,
          absl::bind_front(&PayloadManager::OnPendingPayloadDestroy, this)));
  return pending_payloads_.GetPayload(payload_id);
}

void PayloadManager::OnPendingPayloadDestroy(const PendingPayload* payload) {
  LOG(INFO) << "PayloadManager: destroying " << payload->ToString()
                    << " self=" << this;
  ThroughputRecorderContainer::GetInstance().StopTPRecorder(
      payload->GetId(), payload->IsIncoming()
                            ? PayloadDirection::INCOMING_PAYLOAD
                            : PayloadDirection::OUTGOING_PAYLOAD);
  if (payload->IsIncoming()) return;
  RunOnStatusUpdateThread(
      "~PendingPayload",
      [this]() RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD() { NotifyShutdown(); });
}

void PayloadManager::SendClientCallbacksForFinishedOutgoingPayload(
    ClientProxy* client, const EndpointIds& finished_endpoint_ids,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int64_t num_bytes_successfully_transferred,
    location::nearby::proto::connections::PayloadStatus status) {
  RunOnStatusUpdateThread(
      "outgoing-payload-callbacks",
      [this, client, finished_endpoint_ids, payload_header,
       num_bytes_successfully_transferred,
       status]() RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD() {
        // Make sure we're still tracking this payload.
        PendingPayloadHandle pending_payload = GetPayload(payload_header.id());
        if (!pending_payload) {
          return;
        }

        PayloadProgressInfo update{
            payload_header.id(),
            PayloadManager::PayloadStatusToTransferUpdateStatus(status),
            payload_header.total_size(), num_bytes_successfully_transferred};
        for (const auto& endpoint_id : finished_endpoint_ids) {
          // Skip sending notifications if we have stopped tracking this
          // endpoint.
          if (!pending_payload->GetEndpoint(endpoint_id)) {
            continue;
          }

          // Notify the client.
          client->OnPayloadProgress(endpoint_id, update);

          // Mark this payload as done for analytics.
          client->GetAnalyticsRecorder().OnOutgoingPayloadDone(
              endpoint_id, payload_header.id(), status);
        }

        // Remove these endpoints from our tracking list for this payload.
        pending_payload->RemoveEndpoints(finished_endpoint_ids);

        // Close the payload if no endpoints remain.
        if (pending_payload->GetEndpoints().empty()) {
          pending_payload->Close();
        }
      });
}

void PayloadManager::SendClientCallbacksForFinishedIncomingPayload(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int64_t offset_bytes,
    location::nearby::proto::connections::PayloadStatus status) {
  RunOnStatusUpdateThread(
      "incoming-payload-callbacks",
      [this, client, endpoint_id, payload_header, offset_bytes,
       status]() RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD() {
        // Make sure we're still tracking this payload.
        PendingPayloadHandle pending_payload = GetPayload(payload_header.id());
        if (!pending_payload) {
          return;
        }

        // Unless we never started tracking this payload (meaning we
        // failed to even create the InternalPayload), notify the client
        // (and close it).
        PayloadProgressInfo update{
            payload_header.id(),
            PayloadManager::PayloadStatusToTransferUpdateStatus(status),
            payload_header.total_size(), offset_bytes};
        NotifyClientOfIncomingPayloadProgressInfo(client, endpoint_id, update);
        DestroyPendingPayload(payload_header.id());

        // Analyze
        client->GetAnalyticsRecorder().OnIncomingPayloadDone(
            endpoint_id, payload_header.id(), status);
      });
}

void PayloadManager::SendControlMessage(
    const EndpointIds& endpoint_ids,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int64_t num_bytes_successfully_transferred,
    PayloadTransferFrame::ControlMessage::EventType event_type) {
  PayloadTransferFrame::ControlMessage control_message;
  control_message.set_event(event_type);
  control_message.set_offset(num_bytes_successfully_transferred);

  endpoint_manager_->SendControlMessage(payload_header, control_message,
                                        endpoint_ids);
}

void PayloadManager::SendPayloadReceivedAck(ClientProxy* client,
                                            PendingPayload& pending_payload,
                                            const std::string& endpoint_id,
                                            bool is_last_chunk) {
  if (!is_last_chunk ||
      !IsPayloadReceivedAckEnabled(client, endpoint_id, pending_payload)) {
    return;
  }

  send_payload_ack_executor_.Execute(
      "send_payload_ack", [this, &pending_payload, endpoint_id]() {
        endpoint_manager_->SendPayloadAck(pending_payload.GetId(),
                                          {endpoint_id});
        LOG(INFO) << "[safe-to-disconnect] Send "
                             "PAYLOAD_RECEIVED_ACK frame to: "
                          << endpoint_id << " done";
      });
  // Send the PAYLOAD_RECEIVED_ACK to the remote endpoint for the sender asap.
  LOG(INFO) << "[safe-to-disconnect] " << pending_payload.GetId()
            << " isLastChunk, receiver send ack to " << endpoint_id;
}

bool PayloadManager::WaitForReceivedAck(
    ClientProxy* client, const std::string& endpoint_id,
    PendingPayload& pending_payload,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int64_t payload_chunk_offset, bool is_last_chunk) {
  if (!is_last_chunk ||
      !IsPayloadReceivedAckEnabled(client, endpoint_id, pending_payload)) {
    return true;
  }

  LOG(INFO) << "[safe-to-disconnect] Last Chunk, sender wait for "
                       "PAYLOAD_RECEIVED_ACK frame from: "
                    << endpoint_id;
  while (true) {
    PendingPayloadHandle latest_pending_payload =
        GetPayload(payload_header.id());
    // Make sure we're still tracking this payload and its associated endpoint.
    if (!latest_pending_payload) {
      LOG(INFO) << "[safe-to-disconnect] short-circuiting "
                           "latest_pending_payload is null for "
                        << payload_header.id() << ", stop wait ack.";
      return false;
    }

    auto* endpoint_info = latest_pending_payload->GetEndpoint(endpoint_id);
    if (endpoint_info == nullptr) {
      LOG(INFO) << "[safe-to-disconnect] short-circuiting "
                           "endpointInfo is null for "
                        << payload_header.id() << ", stop wait ack.";
      return false;
    }

    // Local payload cancellation
    if (latest_pending_payload->IsLocallyCanceled()) {
      HandleFinishedOutgoingPayload(client, {endpoint_id}, payload_header,
                                    payload_chunk_offset,
                                    location::nearby::proto::connections::
                                        PayloadStatus::LOCAL_CANCELLATION);
      LOG(INFO) << "[safe-to-disconnect] short-circuiting local "
                           "payload cancellation for "
                        << payload_header.id() << ", stop wait ack.";
      return false;
    }
    // Remote payload cancellation, etc
    if (!endpoint_info->IsEndpointAvailable(client,
                                            endpoint_info->status.Get())) {
      HandleFinishedOutgoingPayload(
          client, {endpoint_id}, payload_header, payload_chunk_offset,
          EndpointInfoStatusToPayloadStatus(endpoint_info->status.Get()));
      LOG(INFO) << "[safe-to-disconnect] short-circuiting remote "
                           "payload cancellation for "
                        << payload_header.id() << ", stop wait ack.";
      return false;
    }
    {
      MutexLock lock(&endpoint_info->payload_received_ack_mutex);
      if (endpoint_info->is_payload_received_ack) {
        LOG(INFO) << "[safe-to-disconnect][PAYLOAD_RECEIVED_ACK] sender already"
                     " received payload ack from "
                  << endpoint_id << ", stop wait PAYLOAD_RECEIVED_ACK.";
        endpoint_info->is_payload_received_ack = false;
        return true;
      }
      Exception wait_exception = endpoint_info->payload_received_ack_cond.Wait(
          FeatureFlags::GetInstance()
              .GetFlags()
              .wait_payload_received_ack_millis);
      if (!wait_exception.Ok()) {
        endpoint_info->is_payload_received_ack = false;
        LOG(INFO)
            << "[safe-to-disconnect][PAYLOAD_RECEIVED_ACK] sender wait for "
               "received payload ack from "
            << endpoint_id << " end with exception: " << wait_exception.value;
        return false;
      }
      if (endpoint_info->is_payload_received_ack) {
        LOG(INFO) << "[safe-to-disconnect][PAYLOAD_RECEIVED_ACK] Received "
                     "notification that sender "
                     "received payload ack from "
                  << endpoint_id;
        endpoint_info->is_payload_received_ack = false;
        return true;
      } else {
        LOG(INFO) << "[safe-to-disconnect][PAYLOAD_RECEIVED_ACK] sender doesn't"
                     " received payload ack from "
                  << endpoint_id << ", end with timeout.";
        return false;
     }
    }
  }
  return true;
}

bool PayloadManager::IsPayloadReceivedAckEnabled(
    ClientProxy* client, const std::string& endpoint_id,
    PendingPayload& pending_payload) {
  return NearbyFlags::GetInstance().GetBoolFlag(
             config_package_nearby::nearby_connections_feature::
                 kEnablePayloadReceivedAck) &&
         client->IsPayloadReceivedAckEnabled(endpoint_id) &&
         (pending_payload.GetInternalPayload()->GetType() !=
          nearby::connections::PayloadTransferFrame::PayloadTransferFrame::
              PayloadHeader::BYTES);
}

void PayloadManager::HandleFinishedOutgoingPayload(
    ClientProxy* client, const EndpointIds& finished_endpoint_ids,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int64_t num_bytes_successfully_transferred,
    location::nearby::proto::connections::PayloadStatus status) {
  // This call will destroy a pending payload.
  SendClientCallbacksForFinishedOutgoingPayload(
      client, finished_endpoint_ids, payload_header,
      num_bytes_successfully_transferred, status);

  switch (status) {
    case location::nearby::proto::connections::PayloadStatus::LOCAL_ERROR:
      SendControlMessage(finished_endpoint_ids, payload_header,
                         num_bytes_successfully_transferred,
                         PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
      break;
    case location::nearby::proto::connections::PayloadStatus::
        LOCAL_CANCELLATION:
      LOG(INFO)
          << "Sending PAYLOAD_CANCEL to receiver side; payload_id="
          << payload_header.id();
      SendControlMessage(
          finished_endpoint_ids, payload_header,
          num_bytes_successfully_transferred,
          PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
      break;
    case location::nearby::proto::connections::PayloadStatus::ENDPOINT_IO_ERROR:
      // Unregister these endpoints, since we had an IO error on the physical
      // connection.
      for (const auto& endpoint_id : finished_endpoint_ids) {
        endpoint_manager_->DiscardEndpoint(client, endpoint_id,
                                           DisconnectionReason::IO_ERROR);
      }
      break;
    case location::nearby::proto::connections::PayloadStatus::REMOTE_ERROR:
    case location::nearby::proto::connections::PayloadStatus::
        REMOTE_CANCELLATION:
      // No special handling needed for these.
      break;
    default:
      LOG(INFO)
          << "PayloadManager: Unhandled finished outgoing payload with "
             "payload_status="
          << status;
      break;
  }
}

void PayloadManager::HandleFinishedIncomingPayload(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int64_t offset_bytes,
    location::nearby::proto::connections::PayloadStatus status) {
  SendClientCallbacksForFinishedIncomingPayload(
      client, endpoint_id, payload_header, offset_bytes, status);

  switch (status) {
    case location::nearby::proto::connections::PayloadStatus::LOCAL_ERROR:
      SendControlMessage({endpoint_id}, payload_header, offset_bytes,
                         PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
      break;
    case location::nearby::proto::connections::PayloadStatus::
        LOCAL_CANCELLATION:
      SendControlMessage(
          {endpoint_id}, payload_header, offset_bytes,
          PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
      break;
    default:
      LOG(INFO) << "Unhandled finished incoming payload_id="
                        << payload_header.id()
                        << " with payload_status=" << status;
      break;
  }
}

void PayloadManager::HandleSuccessfulOutgoingChunk(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int32_t payload_chunk_flags, std::int64_t payload_chunk_offset,
    std::int64_t payload_chunk_body_size) {
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kEnablePayloadManagerToSkipChunkUpdate)) {
    MutexLock lock(&chunk_update_mutex_);
    ++outgoing_chunk_update_count_;
  }
  RunOnStatusUpdateThread(
      "outgoing-chunk-success",
      [this, client, endpoint_id, payload_header, payload_chunk_flags,
       payload_chunk_offset,
       payload_chunk_body_size]() RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD() {
        // Make sure we're still tracking this payload and its associated
        // endpoint.
        bool is_last_chunk =
            (payload_chunk_flags &
             PayloadTransferFrame::PayloadChunk::LAST_CHUNK) != 0;

        if (NearbyFlags::GetInstance().GetBoolFlag(
                config_package_nearby::nearby_connections_feature::
                    kEnablePayloadManagerToSkipChunkUpdate)) {
          MutexLock lock(&chunk_update_mutex_);
          --outgoing_chunk_update_count_;
          if (outgoing_chunk_update_count_ > 0 && payload_header.has_type() &&
              payload_header.type() ==
                  PayloadTransferFrame::PayloadTransferFrame::PayloadHeader::
                      FILE) {
            if (!is_last_chunk && payload_chunk_offset != 0) {
              LOG(INFO) << "Skip the outgoing chunk update with offset="
                                << payload_chunk_offset;
              client->GetAnalyticsRecorder().OnPayloadChunkSent(
                  endpoint_id, payload_header.id(), payload_chunk_body_size);
              return;
            }
          }
        }

        PendingPayloadHandle pending_payload = GetPayload(payload_header.id());
        if (!pending_payload || !pending_payload->GetEndpoint(endpoint_id)) {
          LOG(INFO)
              << "HandleSuccessfulOutgoingChunk: endpoint not found: "
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
          client->GetAnalyticsRecorder().OnOutgoingPayloadDone(
              endpoint_id, payload_header.id(),
              location::nearby::proto::connections::SUCCESS);

          // Stop tracking this endpoint.
          pending_payload->RemoveEndpoints({endpoint_id});

          // Close the payload if no endpoints remain.
          if (pending_payload->GetEndpoints().empty()) {
            pending_payload->Close();
          }
        } else {
          client->GetAnalyticsRecorder().OnPayloadChunkSent(
              endpoint_id, payload_header.id(), payload_chunk_body_size);
        }
      });
}

// @PayloadManagerStatusUpdateThread
void PayloadManager::DestroyPendingPayload(Payload::Id payload_id) {
  pending_payloads_.StopTrackingPayload(payload_id);
}

void PayloadManager::HandleSuccessfulIncomingChunk(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int32_t payload_chunk_flags, std::int64_t payload_chunk_offset,
    std::int64_t payload_chunk_body_size) {
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kEnablePayloadManagerToSkipChunkUpdate)) {
    MutexLock lock(&chunk_update_mutex_);
    ++incoming_chunk_update_count_;
  }
  RunOnStatusUpdateThread(
      "incoming-chunk-success",
      [this, client, endpoint_id, payload_header, payload_chunk_flags,
       payload_chunk_offset,
       payload_chunk_body_size]() RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD() {
        // Make sure we're still tracking this payload.
        bool is_last_chunk =
            (payload_chunk_flags &
             PayloadTransferFrame::PayloadChunk::LAST_CHUNK) != 0;

        if (NearbyFlags::GetInstance().GetBoolFlag(
                config_package_nearby::nearby_connections_feature::
                    kEnablePayloadManagerToSkipChunkUpdate)) {
          MutexLock lock(&chunk_update_mutex_);
          --incoming_chunk_update_count_;
          if (incoming_chunk_update_count_ > 0 && payload_header.has_type() &&
              payload_header.type() ==
                  PayloadTransferFrame::PayloadTransferFrame::PayloadHeader::
                      FILE) {
            if (!is_last_chunk && payload_chunk_offset != 0) {
              LOG(INFO) << "Skip the incoming chunk update with offset="
                                << payload_chunk_offset;
              client->GetAnalyticsRecorder().OnPayloadChunkReceived(
                  endpoint_id, payload_header.id(), payload_chunk_body_size);
              return;
            }
          }
        }

        PendingPayloadHandle pending_payload = GetPayload(payload_header.id());
        if (!pending_payload) {
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
        NotifyClientOfIncomingPayloadProgressInfo(client, endpoint_id, update);

        // Analyze the success.
        if (is_last_chunk) {
          client->GetAnalyticsRecorder().OnIncomingPayloadDone(
              endpoint_id, payload_header.id(),
              location::nearby::proto::connections::SUCCESS);
        } else {
          client->GetAnalyticsRecorder().OnPayloadChunkReceived(
              endpoint_id, payload_header.id(), payload_chunk_body_size);
        }
      });
}

// @EndpointManagerDataPool
void PayloadManager::ProcessDataPacket(
    ClientProxy* to_client, const std::string& from_endpoint_id,
    PayloadTransferFrame& payload_transfer_frame, Medium medium,
    PacketMetaData& packet_meta_data) {
  PayloadTransferFrame::PayloadHeader& payload_header =
      *payload_transfer_frame.mutable_payload_header();
  PayloadTransferFrame::PayloadChunk& payload_chunk =
      *payload_transfer_frame.mutable_payload_chunk();
  NEARBY_VLOG(1) << "PayloadManager got data OfflineFrame for payload_id="
                 << payload_header.id()
                 << " from endpoint_id=" << from_endpoint_id << " at offset "
                 << payload_chunk.offset();
  // We explicitly deny payloads with ID 0.
  if (payload_header.id() == 0) {
    LOG(WARNING) << "Denying payload with ID 0 for endpoint_id="
                         << from_endpoint_id << ", aborting receipt.";
    // Send the error to the remote endpoint.
    SendControlMessage({from_endpoint_id}, payload_header,
                       payload_chunk.offset(),
                       PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
    return;
  }
  Payload::Id payload_id = payload_header.id();
  PendingPayloadHandle pending_payload;
  if (payload_chunk.offset() == 0) {
    ThroughputRecorderContainer::GetInstance()
        .GetTPRecorder(payload_id, PayloadDirection::INCOMING_PAYLOAD)
        ->Start((PayloadType)payload_header.type(),
                PayloadDirection::INCOMING_PAYLOAD);
    packet_meta_data.Reset();
    RunOnStatusUpdateThread(
        "process-data-packet", [to_client, from_endpoint_id, payload_header,
                                this]() RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD() {
          // This is the first chunk of a new incoming
          // payload. Start the analysis.
          to_client->GetAnalyticsRecorder().OnIncomingPayloadStarted(
              from_endpoint_id, payload_header.id(),
              FramePayloadTypeToPayloadType(payload_header.type()),
              payload_header.total_size());
        });

    pending_payload =
        CreateIncomingPayload(payload_transfer_frame, from_endpoint_id);
    if (!pending_payload) {
      LOG(WARNING)
          << "PayloadManager failed to create InternalPayload from "
             "PayloadTransferFrame with payload_id="
          << payload_header.id() << " and type " << payload_header.type()
          << ", aborting receipt.";
      // Send the error to the remote endpoint.
      SendControlMessage({from_endpoint_id}, payload_header,
                         payload_chunk.offset(),
                         PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
      return;
    }
    // Also, let the client know of this new incoming payload.
    RunOnStatusUpdateThread(
        "process-data-packet",
        [to_client, from_endpoint_id,
         pending_payload = GetPayload(payload_id)]()
            RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD() {
              if (!pending_payload) return;
              LOG(INFO)
                  << "PayloadManager received new payload_id="
                  << pending_payload->GetInternalPayload()->GetId()
                  << " from endpoint_id=" << from_endpoint_id;
              to_client->OnPayload(
                  from_endpoint_id,
                  pending_payload->GetInternalPayload()->ReleasePayload());
            });
  } else {
    pending_payload = GetPayload(payload_header.id());
  }

  if (!pending_payload) {
    LOG(WARNING) << "ProcessDataPacket: [missing] endpoint_id="
                         << from_endpoint_id
                         << "; payload_id=" << payload_header.id();
    return;
  }
  if (pending_payload->IsLocallyCanceled()) {
    // This incoming payload was canceled by the client. Drop this frame and
    // do all the cleanup. See go/nc-cancel-payload
    LOG(INFO) << "ProcessDataPacket: [cancel] endpoint_id="
                      << from_endpoint_id
                      << "; payload_id=" << pending_payload->GetId();
    HandleFinishedIncomingPayload(to_client, from_endpoint_id, payload_header,
                                  payload_chunk.offset(),
                                  location::nearby::proto::connections::
                                      PayloadStatus::LOCAL_CANCELLATION);
    return;
  }

  // Update the offset for this payload. An endpoint disconnection might occur
  // from another thread and we would need to know the current offset to
  // report back to the client. For the sake of accuracy, we update the
  // pending payload here because it's after all payload terminating events
  // are handled, but right before we actually start attaching the next chunk.
  pending_payload->SetOffsetForEndpoint(from_endpoint_id,
                                        payload_chunk.offset());

  // Save size of packet before we move it.
  std::int64_t payload_body_size = payload_chunk.body().size();

  packet_meta_data.StartFileIo();
  if (pending_payload->GetInternalPayload()
          ->AttachNextChunk(ByteArray(std::move(*payload_chunk.mutable_body())))
          .Raised()) {
    LOG(ERROR) << "ProcessDataPacket: [data: error] endpoint_id="
                       << from_endpoint_id
                       << "; payload_id=" << pending_payload->GetId();
    HandleFinishedIncomingPayload(
        to_client, from_endpoint_id, payload_header, payload_chunk.offset(),
        location::nearby::proto::connections::PayloadStatus::LOCAL_ERROR);
    return;
  }
  packet_meta_data.StopFileIo();
  bool is_last_chunk = (payload_chunk.flags() &
                        PayloadTransferFrame::PayloadChunk::LAST_CHUNK) != 0;
  SendPayloadReceivedAck(to_client, *pending_payload, from_endpoint_id,
                         is_last_chunk);

  HandleSuccessfulIncomingChunk(to_client, from_endpoint_id, payload_header,
                                payload_chunk.flags(), payload_chunk.offset(),
                                payload_body_size);

  ThroughputRecorderContainer::GetInstance()
      .GetTPRecorder(payload_header.id(), PayloadDirection::INCOMING_PAYLOAD)
      ->OnFrameReceived(medium, packet_meta_data);
  if (is_last_chunk) {
    ThroughputRecorderContainer::GetInstance()
        .GetTPRecorder(payload_header.id(), PayloadDirection::INCOMING_PAYLOAD)
        ->MarkAsSuccess();
  }
}

// @EndpointManagerDataPool
void PayloadManager::ProcessControlPacket(
    ClientProxy* to_client, const std::string& from_endpoint_id,
    PayloadTransferFrame& payload_transfer_frame) {
  const PayloadTransferFrame::PayloadHeader& payload_header =
      payload_transfer_frame.payload_header();
  const PayloadTransferFrame::ControlMessage& control_message =
      payload_transfer_frame.control_message();
  PendingPayloadHandle pending_payload = GetPayload(payload_header.id());
  if (!pending_payload) {
    LOG(INFO) << "Got ControlMessage for unknown payload_id="
                      << payload_header.id()
                      << ", ignoring: " << control_message.event();
    return;
  }

  switch (control_message.event()) {
    case PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED:
      if (pending_payload->IsIncoming()) {
        LOG(INFO) << "Incoming PAYLOAD_CANCELED: from endpoint_id="
                          << from_endpoint_id << "; self=" << this;
        // No need to mark the pending payload as cancelled, since this is a
        // remote cancellation for an incoming payload -- we handle everything
        // inline here.
        HandleFinishedIncomingPayload(
            to_client, from_endpoint_id, payload_header,
            control_message.offset(),
            ControlMessageEventToPayloadStatus(control_message.event()));
      } else {
        LOG(INFO) << "Outgoing PAYLOAD_CANCELED: from endpoint_id="
                          << from_endpoint_id << "; self=" << this;
        // Mark the payload as canceled *for this endpoint*.
        pending_payload->SetEndpointStatusFromControlMessage(from_endpoint_id,
                                                             control_message);
      }
      NEARBY_VLOG(1)
          << "Marked "
          << (pending_payload->IsIncoming() ? "incoming" : "outgoing")
          << " payload_id=" << pending_payload->GetInternalPayload()->GetId()
          << " as canceled at request of endpoint_id=" << from_endpoint_id;
      break;
    case PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR:
      if (pending_payload->IsIncoming()) {
        HandleFinishedIncomingPayload(
            to_client, from_endpoint_id, payload_header,
            control_message.offset(),
            ControlMessageEventToPayloadStatus(control_message.event()));
      } else {
        pending_payload->SetEndpointStatusFromControlMessage(from_endpoint_id,
                                                             control_message);
      }
      break;
    default:
      LOG(INFO) << "Unhandled control message "
                        << control_message.event() << " for payload_id="
                        << pending_payload->GetInternalPayload()->GetId();
      break;
  }
}

void PayloadManager::ProcessPayloadAckPacket(
    const std::string& from_endpoint_id,
    PayloadTransferFrame& payload_transfer_frame) {
  auto payload_header = payload_transfer_frame.payload_header();
  PendingPayloadHandle pending_payload = GetPayload(payload_header.id());
  if (!pending_payload) {
    LOG(INFO) << "[safe-to-disconnect][PAYLOAD_RECEIVED_ACK] "
                         "short-circuiting got payload "
                         "ack for unknown payload "
                      << payload_header.id() << ", ignoring";
    return;
  }
  if (pending_payload->IsIncoming()) {
    LOG(INFO) << "[safe-to-disconnect][PAYLOAD_RECEIVED_ACK] "
                         "short-circuiting got Payload "
                         "ack for incoming payload "
                      << payload_header.id() << ", ignoring";
  }
  LOG(INFO)
      << "[safe-to-disconnect][PAYLOAD_RECEIVED_ACK] sender received payload "
      << payload_header.id() << " ack from " << from_endpoint_id;
  pending_payload->MarkReceivedAckFromEndpoint(from_endpoint_id);
}

// @PayloadManagerStatusUpdateThread
void PayloadManager::NotifyClientOfIncomingPayloadProgressInfo(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadProgressInfo& payload_transfer_update) {
  client->OnPayloadProgress(endpoint_id, payload_transfer_update);
}

void PayloadManager::RecordPayloadStartedAnalytics(
    ClientProxy* client, const EndpointIds& endpoint_ids,
    std::int64_t payload_id, PayloadType payload_type, std::int64_t offset,
    std::int64_t total_size) {
  client->GetAnalyticsRecorder().OnOutgoingPayloadStarted(
      endpoint_ids, payload_id, payload_type,
      total_size == -1 ? -1 : total_size - offset);
}

void PayloadManager::RecordInvalidPayloadAnalytics(
    ClientProxy* client, const EndpointIds& endpoint_ids,
    std::int64_t payload_id, PayloadType payload_type, std::int64_t offset,
    std::int64_t total_size) {
  RecordPayloadStartedAnalytics(client, endpoint_ids, payload_id, payload_type,
                                offset, total_size);

  for (const auto& endpoint_id : endpoint_ids) {
    client->GetAnalyticsRecorder().OnOutgoingPayloadDone(
        endpoint_id, payload_id,
        location::nearby::proto::connections::LOCAL_ERROR);
  }
}

PayloadType PayloadManager::FramePayloadTypeToPayloadType(
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

void PayloadManager::SetCustomSavePath(ClientProxy* client,
                                       const std::string& path) {
  custom_save_path_ = path;
}

///////////////////////////////// EndpointInfo
////////////////////////////////////

PayloadManager::EndpointInfo::Status
PayloadManager::EndpointInfo::ControlMessageEventToEndpointInfoStatus(
    PayloadTransferFrame::ControlMessage::EventType event) {
  switch (event) {
    case PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR:
      return Status::kError;
    case PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED:
      return Status::kCanceled;
    default:
      LOG(INFO)
          << "Unknown EndpointInfo.Status for ControlMessage.EventType "
          << event;
      return Status::kUnknown;
  }
}

void PayloadManager::EndpointInfo::SetStatusFromControlMessage(
    const PayloadTransferFrame::ControlMessage& control_message) {
  status.Set(ControlMessageEventToEndpointInfoStatus(control_message.event()));
  NEARBY_VLOG(1) << "Marked endpoint " << id << " with status "
                 << ToString(status.Get()) << " based on OOB ControlMessage";
}

void PayloadManager::EndpointInfo::MarkReceivedAckFromEndpoint() {
  MutexLock lock(&payload_received_ack_mutex);
  is_payload_received_ack = true;
  payload_received_ack_cond.Notify();
}

bool PayloadManager::EndpointInfo::IsEndpointAvailable(
    ClientProxy* clientProxy, EndpointInfo::Status status) {
  // Pending endpointIds would be removed from the payload after
  // onPayloadTransferUpdate, but there is the racing problem that gets the
  // available endpoints before update. Here force to remove those endpoints
  // (b/227419433).
  bool is_pending_endpoint = false;
  if (clientProxy->HasPendingConnectionToEndpoint(id)) {
    is_pending_endpoint = true;
  }
  return (status == EndpointInfo::Status::kAvailable) && !is_pending_endpoint;
}

//////////////////////////////// PendingPayload ////////////////////////////////

PayloadManager::PendingPayload::PendingPayload(
    std::unique_ptr<InternalPayload> internal_payload,
    const EndpointIds& endpoint_ids, bool is_incoming,
    DestroyCallback destroy_callback)
    : is_incoming_(is_incoming),
      internal_payload_(std::move(internal_payload)),
      destroy_callback_(std::move(destroy_callback)) {
  // Initially we mark all endpoints as available.
  // Later on some may become canceled, some may experience data transfer
  // failures. Any of these situations will cause endpoint to be marked as
  // unavailable.
  for (const auto& id : endpoint_ids) {
    EndpointInfo endpoint_info;
    endpoint_info.id = id;
    endpoint_info.status.Set(EndpointInfo::Status::kAvailable);

    endpoints_.emplace(id, std::move(endpoint_info));
  }
}

Payload::Id PayloadManager::PendingPayload::GetId() const {
  return internal_payload_->GetId();
}

InternalPayload* PayloadManager::PendingPayload::GetInternalPayload() {
  return internal_payload_.get();
}

bool PayloadManager::PendingPayload::IsLocallyCanceled() const {
  return is_locally_canceled_.Get();
}

void PayloadManager::PendingPayload::MarkLocallyCanceled() {
  is_locally_canceled_.Set(true);
}

void PayloadManager::PendingPayload::MarkReceivedAckFromEndpoint(
    const std::string& from_endpoint_id) {
  auto info = GetEndpoint(from_endpoint_id);
  if (!info) return;
  info->MarkReceivedAckFromEndpoint();
}

bool PayloadManager::PendingPayload::IsIncoming() const { return is_incoming_; }

std::vector<const PayloadManager::EndpointInfo*>
PayloadManager::PendingPayload::GetEndpoints() const {
  MutexLock lock(&mutex_);

  std::vector<const EndpointInfo*> result;
  for (const auto& item : endpoints_) {
    result.push_back(&item.second);
  }
  return result;
}

PayloadManager::EndpointInfo* PayloadManager::PendingPayload::GetEndpoint(
    const std::string& endpoint_id) {
  MutexLock lock(&mutex_);

  auto it = endpoints_.find(endpoint_id);
  if (it == endpoints_.end()) {
    return {};
  }

  return &it->second;
}

void PayloadManager::PendingPayload::RemoveEndpoints(
    const EndpointIds& endpoint_ids) {
  MutexLock lock(&mutex_);

  for (const auto& id : endpoint_ids) {
    endpoints_.erase(id);
  }
}

void PayloadManager::PendingPayload::SetEndpointStatusFromControlMessage(
    const std::string& endpoint_id,
    const PayloadTransferFrame::ControlMessage& control_message) {
  MutexLock lock(&mutex_);

  auto item = endpoints_.find(endpoint_id);
  if (item != endpoints_.end()) {
    item->second.SetStatusFromControlMessage(control_message);
  }
}

void PayloadManager::PendingPayload::SetOffsetForEndpoint(
    const std::string& endpoint_id, std::int64_t offset) {
  MutexLock lock(&mutex_);

  auto item = endpoints_.find(endpoint_id);
  if (item != endpoints_.end()) {
    item->second.offset = offset;
  }
}

void PayloadManager::PendingPayload::Close() {
  bool was_closed = is_closed_.Set(true);
  if (was_closed) return;
  if (internal_payload_) internal_payload_->Close();
}

void PayloadManager::RunOnStatusUpdateThread(
    const std::string& name, absl::AnyInvocable<void()> runnable) {
  payload_status_update_executor_.Execute(name, std::move(runnable));
}

/////////////////////////////// PendingPayloads
//////////////////////////////////

void PayloadManager::PendingPayloads::StartTrackingPayload(
    Payload::Id payload_id, std::unique_ptr<PendingPayload> pending_payload) {
  MutexLock lock(&mutex_);

  // If the |payload_id| is being re-used, always prefer the newer payload.
  Remove(pending_payloads_.find(payload_id));
  LOG(INFO) << "StartTrackingPayload: " << pending_payload->ToString();
  pending_payload->IncRefCount();
  pending_payloads_[payload_id] = std::move(pending_payload);
}

void PayloadManager::PendingPayloads::StopTrackingPayload(
    Payload::Id payload_id) {
  MutexLock lock(&mutex_);
  LOG(INFO) << "StopTrackingPayload " << payload_id;
  Remove(pending_payloads_.find(payload_id));
}

void PayloadManager::PendingPayloads::Remove(
    absl::flat_hash_map<Payload::Id, std::unique_ptr<PendingPayload>>::iterator
        it) {
  if (it != pending_payloads_.end()) {
    int refcount = it->second->DecRefCount();
    if (refcount == 0) {
      // Nobody is using the payload, we can remove it.
      NEARBY_VLOG(1) << "Erase payload " << it->second->ToString();
      pending_payloads_.erase(it);
    } else {
      // Someone is still using the payload. Move it to the garbage bin. The
      // payload will be removed when they release it.
      NEARBY_VLOG(1) << "Bin payload " << it->second->ToString();
      payload_garbage_bin_.push_back(
          std::move(pending_payloads_.extract(it).mapped()));
    }
  }
}

PayloadManager::PendingPayloadHandle
PayloadManager::PendingPayloads::GetPayload(Payload::Id payload_id) const {
  MutexLock lock(&mutex_);

  auto item = pending_payloads_.find(payload_id);
  if (item == pending_payloads_.end()) {
    return PendingPayloadHandle();
  }
  PendingPayload* payload = item->second.get();
  payload->IncRefCount();
  return PendingPayloadHandle(
      payload, absl::bind_front(&PendingPayloads::Release,
                                const_cast<PendingPayloads*>(this)));
}

void PayloadManager::PendingPayloads::StopTrackingAllPayloads() {
  MutexLock lock(&mutex_);

  for (auto it = pending_payloads_.begin(); it != pending_payloads_.end();) {
    Remove(it++);
  }
}

void PayloadManager::PendingPayloads::ForEachPayload(
    absl::AnyInvocable<void(PendingPayload*)> callback) {
  MutexLock lock(&mutex_);

  for (const auto& item : pending_payloads_) {
    callback(item.second.get());
  }
}

void PayloadManager::PendingPayloads::Release(PendingPayload* payload) {
  // Called when `PendingPayloadHandle` is destroyed.
  MutexLock lock(&mutex_);
  NEARBY_VLOG(1) << __func__ << " " << payload->ToString();
  auto it = pending_payloads_.find(payload->GetId());
  if (it != pending_payloads_.end() && it->second.get() == payload) {
    // The payload is still tracked.
    payload->DecRefCount();
    return;
  }
  auto bin_it =
      std::find_if(payload_garbage_bin_.begin(), payload_garbage_bin_.end(),
                   [payload](auto& item) { return item.get() == payload; });
  if (bin_it != payload_garbage_bin_.end()) {
    int refcount = payload->DecRefCount();
    if (refcount == 0) {
      // The payload is not tracked and it was the last reference.
      payload_garbage_bin_.erase(bin_it);
    }
  }
}

PayloadManager::PendingPayloadHandle::PendingPayloadHandle(
    PendingPayload* payload, DestroyCallback destroy_callback)
    : payload_(payload), destroy_callback_(std::move(destroy_callback)) {}

PayloadManager::PendingPayloadHandle::~PendingPayloadHandle() {
  if (destroy_callback_) {
    std::move(destroy_callback_)(payload_);
  }
}

std::string PayloadManager::PendingPayload::ToString() const {
  return absl::StrFormat("Payload(%s, %d)",
                         IsIncoming() ? "incoming" : "outgoing", GetId());
}

}  // namespace connections
}  // namespace nearby
