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
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/listeners.h"
#include "connections/payload.h"
#include "connections/payload_type.h"
#include "connections/status.h"
#include "internal/platform/atomic_reference.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/expected.h"
#include "internal/platform/single_thread_executor.h"
#include "proto/connections_enums.pb.h"

namespace nearby::connections {

// Annotations for methods that need to run on PayloadStatusUpdateThread.
// Use only in PayloadManager
#define RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD() \
  ABSL_EXCLUSIVE_LOCKS_REQUIRED(payload_status_update_executor_)

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
      const std::string& endpoint_id, CountDownLatch barrier,
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

    std::string id;
    AtomicReference<Status> status{Status::kUnknown};
    int64_t offset = 0;
  };

  // Tracks state for an InternalPayload.
  class PendingPayload {
   public:
    PendingPayload();
    virtual ~PendingPayload() = default;

    bool IsLocallyCanceled() const;
    void MarkLocallyCanceledAndClose();

    // Records the point at which the transfer of this payload actually
    // started, used to compute the transfer duration and throughput that are
    // logged once the payload completes.
    //
    // `start_time` is when the transfer started (e.g. when the payload
    // executor begins sending an outgoing payload, rather than when it was
    // queued; or when the first chunk of an incoming payload arrived).
    // `start_offset` is the number of payload bytes that were already
    // transferred at `start_time` and should therefore be excluded from the
    // throughput computation (e.g. the resume offset of an outgoing payload,
    // or the body of the first chunk of an incoming payload, which had
    // already been received when the start time was stamped).
    //
    // If never called, the start time defaults to when this pending payload
    // was created and the start offset defaults to 0.
    void SetTransferStart(absl::Time start_time, int64_t start_offset)
        ABSL_LOCKS_EXCLUDED(mutex_);
    absl::Time GetTransferStartTime() const ABSL_LOCKS_EXCLUDED(mutex_);
    int64_t GetTransferStartOffset() const ABSL_LOCKS_EXCLUDED(mutex_);

    // Closes the underlying payload.
    // Close is called when a pending payload does not have associated
    // endpoints.
    virtual void Close() = 0;

   private:
    mutable absl::Mutex mutex_;
    std::atomic<bool> is_locally_canceled_{false};
    absl::Time transfer_start_time_ ABSL_GUARDED_BY(mutex_);
    int64_t transfer_start_offset_ ABSL_GUARDED_BY(mutex_) = 0;
  };

  class IncomingPendingPayload : public PendingPayload {
   public:
    IncomingPendingPayload(
        std::unique_ptr<IncomingInternalPayload> incoming_payload,
        absl::string_view endpoint_id);
    IncomingPendingPayload(IncomingPendingPayload&&) = default;
    IncomingPendingPayload& operator=(IncomingPendingPayload&&) = default;

    ~IncomingPendingPayload() override { Close(); }

    Payload::Id GetId() const { return incoming_payload_->GetId(); }
    IncomingInternalPayload* GetIncomingInternalPayload() const {
      return incoming_payload_.get();
    }
    void Close() override {
      if (incoming_payload_ != nullptr) {
        incoming_payload_->Close();
      }
    }

    const std::string& GetEndpointId() const { return endpoint_id_; }
    int64_t GetOffset() const { return offset_; }
    void SetOffset(int64_t offset) { offset_ = offset; }
    std::string ToString() const;

   private:
    const std::unique_ptr<IncomingInternalPayload> incoming_payload_;
    const std::string endpoint_id_;
    std::atomic<int64_t> offset_{0};
  };

  class OutgoingPendingPayload : public PendingPayload {
   public:
    OutgoingPendingPayload(
        std::unique_ptr<OutgoingInternalPayload> outgoing_payload,
        const std::vector<std::string>& endpoint_ids,
        absl::AnyInvocable<void() &&> destroy_callback);
    OutgoingPendingPayload(OutgoingPendingPayload&&) = default;
    OutgoingPendingPayload& operator=(OutgoingPendingPayload&&) = default;

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

  // Tracks and manages PendingPayload objects in a synchronized manner.
  template <typename T>
  class PendingPayloads {
   public:
    PendingPayloads() = default;
    ~PendingPayloads() = default;

    void StartTrackingPayload(Payload::Id payload_id,
                              std::unique_ptr<T> pending_payload)
        ABSL_LOCKS_EXCLUDED(mutex_);
    void StopTrackingPayload(Payload::Id payload_id)
        ABSL_LOCKS_EXCLUDED(mutex_);
    void StopTrackingAllPayloads() ABSL_LOCKS_EXCLUDED(mutex_);
    std::shared_ptr<T> GetPayload(Payload::Id payload_id) const
        ABSL_LOCKS_EXCLUDED(mutex_);
    // Calls `callback` for each tracked payload. The callback must not call
    // other `PendingPayloads` methods.
    void ForEachPayload(absl::AnyInvocable<void(T*)> callback)
        ABSL_LOCKS_EXCLUDED(mutex_);

   private:
    void Remove(typename absl::flat_hash_map<Payload::Id,
                                             std::shared_ptr<T>>::iterator it)
        ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
    mutable absl::Mutex mutex_;
    absl::flat_hash_map<Payload::Id, std::shared_ptr<T>> pending_payloads_
        ABSL_GUARDED_BY(mutex_);
  };

  using Endpoints = std::vector<const EndpointInfo*>;
  static std::string ToString(EndpointInfo::Status status);
  // Converts the status of an endpoint that's been set out-of-band via a
  // remote ControlMessage to the PayloadStatus for handling of that
  // endpoint-payload pair.
  static location::nearby::proto::connections::PayloadStatus
  EndpointInfoStatusToPayloadStatus(EndpointInfo::Status status);
  static location::nearby::proto::connections::OperationResultCode
  EndpointInfoStatusToOperationResultCode(EndpointInfo::Status status);
  static bool IsEndpointAvailable(ClientProxy* clientProxy,
                                  absl::string_view endpoint_id,
                                  EndpointInfo::Status status);

  // Returns the number of bytes sent.  0 bytes sent indicates end of payload.
  // Returns -1 on error.
  int SendPayloadLoop(
      ClientProxy* client, OutgoingPendingPayload& pending_payload,
      location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int64_t next_chunk_offset, size_t resume_offset, int index);
  void SendClientCallbacksForFinishedIncomingPayloadRunnable(
      ClientProxy* client, const std::string& endpoint_id,
      const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int64_t offset_bytes,
      location::nearby::proto::connections::PayloadStatus status,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code);

  int GetOptimalChunkSize(const std::vector<std::string>& endpoint_ids);

  location::nearby::connections::PayloadTransferFrame::PayloadHeader
  CreatePayloadHeader(const OutgoingInternalPayload& outgoing_payload,
                      size_t offset);

  location::nearby::connections::PayloadTransferFrame::PayloadChunk
  CreatePayloadChunk(int64_t offset, ByteArray body, int index);
  bool IsLastChunk(
      location::nearby::connections::PayloadTransferFrame::PayloadChunk
          payload_chunk) {
    return ((payload_chunk.flags() &
             location::nearby::connections::PayloadTransferFrame::PayloadChunk::
                 LAST_CHUNK) != 0);
  }

  // Creates an incoming payload and returns a handle to it.
  // If `save_path` is empty, the payload will be saved to the default save
  // path set in `SetCustomSavePath()`.
  ErrorOr<std::shared_ptr<IncomingPendingPayload>> CreateIncomingPayload(
      const location::nearby::connections::PayloadTransferFrame& frame,
      const std::string& endpoint_id, const std::string& save_path)
      ABSL_LOCKS_EXCLUDED(mutex_);

  Payload::Id CreateOutgoingPayload(
      Payload payload, const std::vector<std::string>& endpoint_ids)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void SendClientCallbacksForFinishedOutgoingPayload(
      ClientProxy* client,
      const std::vector<std::string>& finished_endpoint_ids,
      const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int64_t num_bytes_successfully_transferred,
      location::nearby::proto::connections::PayloadStatus status,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code);

  void SendControlMessage(
      const std::vector<std::string>& endpoint_ids,
      const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int64_t num_bytes_successfully_transferred,
      location::nearby::connections::PayloadTransferFrame::ControlMessage::
          EventType event_type);

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
  void HandleIncomingPayloadError(
      ClientProxy* client, const std::string& endpoint_id,
      const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int64_t offset_bytes,
      location::nearby::proto::connections::PayloadStatus status,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code);
  void HandleIncomingPayloadErrorOnStatusUpdateThread(
      ClientProxy* client, const std::string& endpoint_id,
      const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int64_t offset_bytes,
      location::nearby::proto::connections::PayloadStatus status,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code) RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD();

  void HandleSuccessfulOutgoingChunk(
      ClientProxy* client, const std::string& endpoint_id,
      const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int32_t payload_chunk_flags, int64_t payload_chunk_offset,
      int64_t payload_chunk_body_size);
  void HandleSuccessfulIncomingChunk(
      ClientProxy* client, const std::string& endpoint_id,
      const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int32_t payload_chunk_flags, int64_t payload_chunk_offset,
      int64_t payload_chunk_body_size);

  void ProcessDataPacket(ClientProxy* to_client,
                         const std::string& from_endpoint_id,
                         location::nearby::connections::PayloadTransferFrame&
                             payload_transfer_frame,
                         location::nearby::proto::connections::Medium medium);
  void ProcessControlPacket(ClientProxy* to_client,
                            const std::string& from_endpoint_id,
                            location::nearby::connections::PayloadTransferFrame&
                                payload_transfer_frame);

  void NotifyClientOfIncomingPayloadProgressInfo(
      ClientProxy* client, const std::string& endpoint_id,
      const PayloadProgressInfo& payload_transfer_update)
      RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD();

  SingleThreadExecutor* GetOutgoingPayloadExecutor(PayloadType payload_type);

  void RunOnStatusUpdateThread(const std::string& name,
                               absl::AnyInvocable<void()> runnable);
  bool NotifyShutdown() ABSL_LOCKS_EXCLUDED(mutex_);
  std::shared_ptr<IncomingPendingPayload> GetIncomingPayload(
      Payload::Id payload_id) const ABSL_LOCKS_EXCLUDED(mutex_);
  std::shared_ptr<OutgoingPendingPayload> GetOutgoingPayload(
      Payload::Id payload_id) const ABSL_LOCKS_EXCLUDED(mutex_);
  void CancelAllPayloads() ABSL_LOCKS_EXCLUDED(mutex_);

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

  PayloadType FramePayloadTypeToPayloadType(
      location::nearby::connections::PayloadTransferFrame::PayloadHeader::
          PayloadType type);

  mutable absl::Mutex mutex_;
  std::string custom_save_path_;
  std::atomic<bool> shutdown_{false};
  std::unique_ptr<CountDownLatch> shutdown_barrier_;
  int send_payload_count_ = 0;
  SingleThreadExecutor bytes_payload_executor_;
  SingleThreadExecutor file_payload_executor_;
  SingleThreadExecutor stream_payload_executor_;
  SingleThreadExecutor payload_status_update_executor_;
  SingleThreadExecutor send_payload_ack_executor_;
  PendingPayloads<IncomingPendingPayload> incoming_pending_payloads_;
  PendingPayloads<OutgoingPendingPayload> outgoing_pending_payloads_;
  EndpointManager* endpoint_manager_;

  // When callback processing cannot keep the speed of callback update, the
  // callback thread will be lag to the real transfer. In order to keep sync
  // between callback and sending/receiving threads, we will skip
  // non-important callbacks during file transfer.
  mutable absl::Mutex chunk_update_mutex_;
  int outgoing_chunk_update_count_ ABSL_GUARDED_BY(chunk_update_mutex_) = 0;
  absl::Time last_outgoing_chunk_update_time_
      ABSL_GUARDED_BY(chunk_update_mutex_) = absl::InfinitePast();
  int incoming_chunk_update_count_ ABSL_GUARDED_BY(chunk_update_mutex_) = 0;
  absl::Time last_incoming_chunk_update_time_
      ABSL_GUARDED_BY(chunk_update_mutex_) = absl::InfinitePast();
};

}  // namespace nearby::connections

#endif  // CORE_INTERNAL_PAYLOAD_MANAGER_H_
