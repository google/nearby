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

#ifndef CORE_INTERNAL_OUTGOING_PAYLOAD_MANAGER_H_
#define CORE_INTERNAL_OUTGOING_PAYLOAD_MANAGER_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/internal_payload.h"
#include "connections/implementation/pending_payload.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/payload.h"
#include "connections/payload_type.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/single_thread_executor.h"
#include "proto/connections_enums.pb.h"

namespace nearby::connections {

class OutgoingPayloadManager {
 public:
  OutgoingPayloadManager(EndpointManager& endpoint_manager,
                         SingleThreadExecutor& status_update_executor,
                         std::atomic<bool>& shutdown);
  ~OutgoingPayloadManager() = default;

  OutgoingPayloadManager(const OutgoingPayloadManager&) = delete;
  OutgoingPayloadManager& operator=(const OutgoingPayloadManager&) = delete;

  void SendPayload(ClientProxy* client,
                   const std::vector<std::string>& endpoint_ids,
                   Payload payload);

  // Returns true if `payload_id` was tracked as an outgoing payload and
  // marked canceled.
  bool CancelPayload(Payload::Id payload_id);
  void CancelAllPayloadsAndWait() ABSL_LOCKS_EXCLUDED(mutex_);
  void ShutdownExecutors();
  void StopTrackingAllPayloads();

  void OnEndpointDisconnect(
      ClientProxy* client, const std::string& endpoint_id,
      location::nearby::proto::connections::PayloadStatus payload_status,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code);

  // @EndpointManagerDataPool
  // Returns true if `payload_transfer_frame` matched a tracked outgoing
  // payload.
  bool ProcessControlPacket(ClientProxy* to_client,
                            const std::string& from_endpoint_id,
                            location::nearby::connections::PayloadTransferFrame&
                                payload_transfer_frame);

 private:
  // Information about an endpoint for a particular payload.
  struct EndpointInfo {
    // Status set for the endpoint out-of-band via a ControlMessage.
    enum class Status {
      kUnknown,
      kAvailable,
      kCanceled,
      kError,
    };

    void SetStatusFromControlMessage(
        const location::nearby::connections::PayloadTransferFrame::
            ControlMessage& control_message);

    static Status ControlMessageEventToEndpointInfoStatus(
        location::nearby::connections::PayloadTransferFrame::ControlMessage::
            EventType event);

    EndpointInfo() = default;
    EndpointInfo(EndpointInfo&& other) noexcept
        : id(std::move(other.id)),
          status(other.status.load()),
          offset(other.offset) {}
    EndpointInfo& operator=(EndpointInfo&& other) noexcept {
      id = std::move(other.id);
      status.store(other.status.load());
      offset = other.offset;
      return *this;
    }

    std::string id;
    std::atomic<Status> status{Status::kUnknown};
    int64_t offset = 0;
  };

  class OutgoingPendingPayload : public PendingPayload {
   public:
    OutgoingPendingPayload(
        std::unique_ptr<OutgoingInternalPayload> outgoing_payload,
        const std::vector<std::string>& endpoint_ids,
        absl::AnyInvocable<void() &&> destroy_callback);

    ~OutgoingPendingPayload() override {
      Close();
      if (destroy_callback_) {
        std::move(destroy_callback_)();
      }
    }

    Payload::Id GetId() const { return outgoing_payload_->GetId(); }
    OutgoingInternalPayload* GetOutgoingInternalPayload() const {
      return outgoing_payload_.get();
    }
    void Close() override {
      if (outgoing_payload_ != nullptr) {
        outgoing_payload_->Close();
      }
    }
    std::string ToString() const;

    // Returns a list of endpoint ids and their status.
    std::vector<std::pair<std::string, EndpointInfo::Status>>
    GetEndpointStatuses() const;

    bool IsEndpointsEmpty() const ABSL_LOCKS_EXCLUDED(mutex_);

    bool HasEndpoint(absl::string_view endpoint_id) ABSL_LOCKS_EXCLUDED(mutex_);

    std::optional<EndpointInfo> RemoveEndpoint(absl::string_view endpoint_id)
        ABSL_LOCKS_EXCLUDED(mutex_);

    // Removes the given endpoints, e.g. on error.
    void RemoveEndpoints(const std::vector<std::string>& endpoint_ids_to_remove)
        ABSL_LOCKS_EXCLUDED(mutex_);

    // Sets the status for a particular endpoint.
    void SetEndpointStatusFromControlMessage(
        const std::string& endpoint_id,
        const location::nearby::connections::PayloadTransferFrame::
            ControlMessage& control_message) ABSL_LOCKS_EXCLUDED(mutex_);

    // Sets the offset for a particular endpoint.
    void SetOffsetForEndpoint(const std::string& endpoint_id, int64_t offset)
        ABSL_LOCKS_EXCLUDED(mutex_);

   private:
    mutable absl::Mutex mutex_;
    const std::unique_ptr<OutgoingInternalPayload> outgoing_payload_;
    absl::flat_hash_map<std::string, EndpointInfo> endpoints_
        ABSL_GUARDED_BY(mutex_);
    absl::AnyInvocable<void() &&> destroy_callback_;
  };

  // Tracks and manages OutgoingPendingPayload objects in a synchronized
  // manner.
  class OutgoingPendingPayloads {
   public:
    OutgoingPendingPayloads() = default;
    ~OutgoingPendingPayloads() = default;

    void StartTrackingPayload(
        Payload::Id payload_id,
        std::unique_ptr<OutgoingPendingPayload> pending_payload)
        ABSL_LOCKS_EXCLUDED(mutex_);
    void StopTrackingPayload(Payload::Id payload_id)
        ABSL_LOCKS_EXCLUDED(mutex_);
    void StopTrackingAllPayloads() ABSL_LOCKS_EXCLUDED(mutex_);
    std::shared_ptr<OutgoingPendingPayload> GetPayload(
        Payload::Id payload_id) const ABSL_LOCKS_EXCLUDED(mutex_);
    // Calls `callback` for each tracked payload. The callback must not call
    // other `OutgoingPendingPayloads` methods.
    void ForEachPayload(
        absl::AnyInvocable<void(OutgoingPendingPayload*)> callback)
        ABSL_LOCKS_EXCLUDED(mutex_);

   private:
    void Remove(
        absl::flat_hash_map<
            Payload::Id, std::shared_ptr<OutgoingPendingPayload>>::iterator it)
        ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
    mutable absl::Mutex mutex_;
    absl::flat_hash_map<Payload::Id, std::shared_ptr<OutgoingPendingPayload>>
        pending_payloads_ ABSL_GUARDED_BY(mutex_);
  };

  static std::string ToString(EndpointInfo::Status status);
  // Converts the status of an endpoint that's been set out-of-band via a
  // remote ControlMessage to the PayloadStatus for handling of that
  // endpoint-payload pair.
  static location::nearby::proto::connections::PayloadStatus
  EndpointInfoStatusToPayloadStatus(EndpointInfo::Status status);
  static location::nearby::proto::connections::OperationResultCode
  EndpointInfoStatusToOperationResultCode(EndpointInfo::Status status);

  // Returns the number of bytes sent.  0 bytes sent indicates end of payload.
  // Returns -1 on error.
  int SendPayloadLoop(
      ClientProxy* client, OutgoingPendingPayload& pending_payload,
      location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int64_t next_chunk_offset, size_t resume_offset, int index);

  int GetOptimalChunkSize(const std::vector<std::string>& endpoint_ids);

  location::nearby::connections::PayloadTransferFrame::PayloadHeader
  CreatePayloadHeader(const OutgoingInternalPayload& outgoing_payload,
                      size_t offset);

  location::nearby::connections::PayloadTransferFrame::PayloadChunk
  CreatePayloadChunk(int64_t offset, ByteArray body, int index);

  Payload::Id CreateOutgoingPayload(
      Payload payload, const std::vector<std::string>& endpoint_ids)
      ABSL_LOCKS_EXCLUDED(mutex_);

  std::shared_ptr<OutgoingPendingPayload> GetOutgoingPayload(
      Payload::Id payload_id) const;

  void SendClientCallbacksForFinishedOutgoingPayload(
      ClientProxy* client,
      const std::vector<std::string>& finished_endpoint_ids,
      const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int64_t num_bytes_successfully_transferred,
      location::nearby::proto::connections::PayloadStatus status,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code);

  // Handles a finished outgoing payload for the given endpointIds. All
  // statuses except for SUCCESS are handled here.
  void HandleFinishedOutgoingPayload(
      ClientProxy* client,
      const std::vector<std::string>& finished_endpoint_ids,
      const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int64_t num_bytes_successfully_transferred,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code,
      location::nearby::proto::connections::PayloadStatus status = location::
          nearby::proto::connections::PayloadStatus::UNKNOWN_PAYLOAD_STATUS);

  void HandleSuccessfulOutgoingChunk(
      ClientProxy* client, const std::string& endpoint_id,
      const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int32_t payload_chunk_flags, int64_t payload_chunk_offset,
      int64_t payload_chunk_body_size);

  SingleThreadExecutor* GetOutgoingPayloadExecutor(PayloadType payload_type);

  void RunOnStatusUpdateThread(const std::string& name,
                               absl::AnyInvocable<void()> runnable);
  bool NotifyShutdown() ABSL_LOCKS_EXCLUDED(mutex_);

  void RecordPayloadStartedAnalytics(
      ClientProxy* client, const std::vector<std::string>& endpoint_ids,
      int64_t payload_id, PayloadType payload_type, int64_t offset,
      int64_t total_size);
  void RecordInvalidPayloadAnalytics(
      ClientProxy* client, const std::vector<std::string>& endpoint_ids,
      int64_t payload_id, PayloadType payload_type, int64_t offset,
      int64_t total_size,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code);

  mutable absl::Mutex mutex_;
  std::unique_ptr<CountDownLatch> shutdown_barrier_;
  SingleThreadExecutor bytes_payload_executor_;
  SingleThreadExecutor file_payload_executor_;
  SingleThreadExecutor stream_payload_executor_;
  OutgoingPendingPayloads outgoing_pending_payloads_;
  EndpointManager* endpoint_manager_;
  SingleThreadExecutor* status_update_executor_;
  std::atomic<bool>* shutdown_;

  mutable absl::Mutex chunk_update_mutex_;
  int outgoing_chunk_update_count_ ABSL_GUARDED_BY(chunk_update_mutex_) = 0;
  absl::Time last_outgoing_chunk_update_time_
      ABSL_GUARDED_BY(chunk_update_mutex_) = absl::InfinitePast();
};

}  // namespace nearby::connections

#endif  // CORE_INTERNAL_OUTGOING_PAYLOAD_MANAGER_H_
