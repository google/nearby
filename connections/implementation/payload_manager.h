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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "connections/implementation/analytics/packet_meta_data.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/internal_payload.h"
#include "connections/listeners.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/atomic_reference.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/mutex.h"

namespace nearby {
namespace connections {
using ::location::nearby::connections::PayloadTransferFrame;

// Annotations for methods that need to run on PayloadStatusUpdateThread.
// Use only in PayloadManager
#define RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD() \
  ABSL_EXCLUSIVE_LOCKS_REQUIRED(payload_status_update_executor_)

class PayloadManager : public EndpointManager::FrameProcessor {
 public:
  using EndpointIds = std::vector<std::string>;
  constexpr static const absl::Duration kWaitCloseTimeout =
      absl::Milliseconds(5000);

  explicit PayloadManager(EndpointManager& endpoint_manager);
  ~PayloadManager() override;

  void SendPayload(ClientProxy* client, const EndpointIds& endpoint_ids,
                   Payload payload);
  Status CancelPayload(ClientProxy* client, Payload::Id payload_id);

  // @EndpointManagerReaderThread
  void OnIncomingFrame(
      location::nearby::connections::OfflineFrame& offline_frame,
      const std::string& from_endpoint_id, ClientProxy* to_client,
      location::nearby::proto::connections::Medium current_medium,
      analytics::PacketMetaData& packet_meta_data) override;

  // @EndpointManagerThread
  void OnEndpointDisconnect(ClientProxy* client, const std::string& service_id,
                            const std::string& endpoint_id,
                            CountDownLatch barrier,
                            DisconnectionReason reason) override;

  void DisconnectFromEndpointManager();

  void SetCustomSavePath(ClientProxy* client, const std::string& path);

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
        const PayloadTransferFrame::ControlMessage& control_message);

    static Status ControlMessageEventToEndpointInfoStatus(
        PayloadTransferFrame::ControlMessage::EventType event);
    void MarkReceivedAckFromEndpoint();
    bool IsEndpointAvailable(ClientProxy* clientProxy,
                             EndpointInfo::Status status);

    std::string id;
    AtomicReference<Status> status{Status::kUnknown};
    std::int64_t offset = 0;
    mutable Mutex payload_received_ack_mutex;
    ConditionVariable payload_received_ack_cond{&payload_received_ack_mutex};
    bool is_payload_received_ack ABSL_GUARDED_BY(payload_received_ack_mutex) =
        false;
  };

  // Tracks state for an InternalPayload and the endpoints associated with it.
  class PendingPayload {
   public:
    using DestroyCallback = absl::AnyInvocable<void(PendingPayload*) &&>;
    PendingPayload(std::unique_ptr<InternalPayload> internal_payload,
                   const EndpointIds& endpoint_ids, bool is_incoming,
                   DestroyCallback destroy_callback);
    PendingPayload(PendingPayload&&) = default;
    PendingPayload& operator=(PendingPayload&&) = default;

    ~PendingPayload() {
      Close();
      if (destroy_callback_) {
        std::move(destroy_callback_)(this);
      }
    }

    Payload::Id GetId() const;

    InternalPayload* GetInternalPayload();

    bool IsLocallyCanceled() const;
    void MarkLocallyCanceled();
    void MarkReceivedAckFromEndpoint(const std::string& from_endpoint_id);
    bool IsIncoming() const;

    // Gets the EndpointInfo objects for the endpoints (still) associated with
    // this payload.
    std::vector<const EndpointInfo*> GetEndpoints() const
        ABSL_LOCKS_EXCLUDED(mutex_);
    // Returns the EndpointInfo for a given endpoint ID. Returns null if the
    // endpoint is not associated with this payload.
    EndpointInfo* GetEndpoint(const std::string& endpoint_id)
        ABSL_LOCKS_EXCLUDED(mutex_);

    // Removes the given endpoints, e.g. on error.
    void RemoveEndpoints(const EndpointIds& endpoint_ids_to_remove)
        ABSL_LOCKS_EXCLUDED(mutex_);

    // Sets the status for a particular endpoint.
    void SetEndpointStatusFromControlMessage(
        const std::string& endpoint_id,
        const PayloadTransferFrame::ControlMessage& control_message)
        ABSL_LOCKS_EXCLUDED(mutex_);

    // Sets the offset for a particular endpoint.
    void SetOffsetForEndpoint(const std::string& endpoint_id,
                              std::int64_t offset) ABSL_LOCKS_EXCLUDED(mutex_);

    // Closes internal_payload_.
    // Close is called when a pending peyload does not have associated
    // endpoints.
    void Close();

    std::string ToString() const;

    // Ref counting for `PendingPayloads` use only. `PendingPayloads` class owns
    // all instances of `PendingPayload`.
    int IncRefCount() { return ++refcount_; }
    int DecRefCount() { return --refcount_; }

   private:
    mutable Mutex mutex_;
    bool is_incoming_;
    AtomicBoolean is_locally_canceled_{false};
    AtomicBoolean is_closed_;
    std::unique_ptr<InternalPayload> internal_payload_;
    DestroyCallback destroy_callback_;
    absl::flat_hash_map<std::string, EndpointInfo> endpoints_
        ABSL_GUARDED_BY(mutex_);
    int refcount_ = 0;
  };

  // A RAII handle to `PendingPayload`. Holding a `PendingPayloadHandle`
  // guarantees that `PendingPaylaod` won't be destroyed while in use.
  // Create instances with `GetPayload(Payload::Id)`.
  class PendingPayloadHandle {
   public:
    using DestroyCallback = absl::AnyInvocable<void(PendingPayload*) &&>;
    PendingPayloadHandle() = default;
    PendingPayloadHandle(PendingPayload* payload,
                         DestroyCallback destroy_callback);
    PendingPayloadHandle(const PendingPayloadHandle&) = delete;
    PendingPayloadHandle(PendingPayloadHandle&& other) {
      payload_ = other.payload_;
      other.payload_ = nullptr;
      destroy_callback_ = std::move(other.destroy_callback_);
    }
    ~PendingPayloadHandle();
    PendingPayloadHandle& operator=(const PendingPayloadHandle&) = delete;
    PendingPayloadHandle& operator=(PendingPayloadHandle&& other) {
      if (payload_ != nullptr && destroy_callback_) {
        std::move(destroy_callback_)(payload_);
      }
      payload_ = other.payload_;
      other.payload_ = nullptr;
      destroy_callback_ = std::move(other.destroy_callback_);
      return *this;
    }
    explicit operator bool() const { return payload_ != nullptr; }

    PendingPayload* operator->() const { return payload_; }

    PendingPayload& operator*() const { return *payload_; }

   private:
    PendingPayload* payload_ = nullptr;
    DestroyCallback destroy_callback_;
  };

  // Tracks and manages PendingPayload objects in a synchronized manner.
  class PendingPayloads {
   public:
    PendingPayloads() = default;
    ~PendingPayloads() = default;

    void StartTrackingPayload(Payload::Id payload_id,
                              std::unique_ptr<PendingPayload> pending_payload)
        ABSL_LOCKS_EXCLUDED(mutex_);
    void StopTrackingPayload(Payload::Id payload_id)
        ABSL_LOCKS_EXCLUDED(mutex_);
    void StopTrackingAllPayloads() ABSL_LOCKS_EXCLUDED(mutex_);
    PendingPayloadHandle GetPayload(Payload::Id payload_id) const
        ABSL_LOCKS_EXCLUDED(mutex_);
    // Calls `callback` for each tracked payload. The callback must not call
    // other `PendingPayloads` methods.
    void ForEachPayload(absl::AnyInvocable<void(PendingPayload*)> callback)
        ABSL_LOCKS_EXCLUDED(mutex_);

   private:
    void Release(PendingPayload* payload) ABSL_LOCKS_EXCLUDED(mutex_);
    void Remove(absl::flat_hash_map<
                Payload::Id, std::unique_ptr<PendingPayload>>::iterator it)
        ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
    mutable Mutex mutex_;
    absl::flat_hash_map<Payload::Id, std::unique_ptr<PendingPayload>>
        pending_payloads_ ABSL_GUARDED_BY(mutex_);
    // When we stop tracking a payload but someone is still holding a handle to
    // the payload, we can't delete it just yet. Instead, we move it to the
    // garbage bin. When the `PendingPayloadHandle` is released, the payload
    // will be removed from the bin.
    std::vector<std::unique_ptr<PendingPayload>> payload_garbage_bin_
        ABSL_GUARDED_BY(mutex_);
  };

  using Endpoints = std::vector<const EndpointInfo*>;
  static std::string ToString(const EndpointIds& endpoint_ids);
  static std::string ToString(const Endpoints& endpoints);
  static std::string ToString(PayloadType type);
  static std::string ToString(EndpointInfo::Status status);

  // Splits the endpoints for this payload by availability.
  // Returns a pair of lists of EndpointInfo*, with the first being the list
  // of still-available endpoints, and the second for unavailable endpoints.
  static std::pair<Endpoints, Endpoints> GetAvailableAndUnavailableEndpoints(
      const PendingPayload& pending_payload);

  // Converts list of EndpointInfo to list of Endpoint ids.
  // Returns list of endpoint ids.
  static EndpointIds EndpointsToEndpointIds(const Endpoints& endpoints);

  bool SendPayloadLoop(ClientProxy* client, PendingPayload& pending_payload,
                       PayloadTransferFrame::PayloadHeader& payload_header,
                       std::int64_t& next_chunk_offset, size_t resume_offset);
  void SendClientCallbacksForFinishedIncomingPayloadRunnable(
      ClientProxy* client, const std::string& endpoint_id,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t offset_bytes,
      location::nearby::proto::connections::PayloadStatus status);

  // Converts the status of an endpoint that's been set out-of-band via a
  // remote ControlMessage to the PayloadStatus for handling of that
  // endpoint-payload pair.
  static location::nearby::proto::connections::PayloadStatus
  EndpointInfoStatusToPayloadStatus(EndpointInfo::Status status);
  // Converts a ControlMessage::EventType for a particular payload to a
  // PayloadStatus. Called when we've received a ControlMessage with this
  // event from a remote endpoint; thus the PayloadStatuses are REMOTE_*.
  static location::nearby::proto::connections::PayloadStatus
  ControlMessageEventToPayloadStatus(
      PayloadTransferFrame::ControlMessage::EventType event);
  static PayloadProgressInfo::Status PayloadStatusToTransferUpdateStatus(
      location::nearby::proto::connections::PayloadStatus status);

  int GetOptimalChunkSize(EndpointIds endpoint_ids);

  PayloadTransferFrame::PayloadHeader CreatePayloadHeader(
      const InternalPayload& internal_payload, size_t offset,
      const std::string& parent_folder, const std::string& file_name);

  PayloadTransferFrame::PayloadChunk CreatePayloadChunk(std::int64_t offset,
                                                        ByteArray body);
  bool IsLastChunk(PayloadTransferFrame::PayloadChunk payload_chunk) {
    return ((payload_chunk.flags() &
             PayloadTransferFrame::PayloadChunk::LAST_CHUNK) != 0);
  }

  PendingPayloadHandle CreateIncomingPayload(const PayloadTransferFrame& frame,
                                             const std::string& endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  Payload::Id CreateOutgoingPayload(Payload payload,
                                    const EndpointIds& endpoint_ids)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void SendClientCallbacksForFinishedOutgoingPayload(
      ClientProxy* client, const EndpointIds& finished_endpoint_ids,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t num_bytes_successfully_transferred,
      location::nearby::proto::connections::PayloadStatus status);
  void SendClientCallbacksForFinishedIncomingPayload(
      ClientProxy* client, const std::string& endpoint_id,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t offset_bytes,
      location::nearby::proto::connections::PayloadStatus status);

  void SendControlMessage(
      const EndpointIds& endpoint_ids,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t num_bytes_successfully_transferred,
      PayloadTransferFrame::ControlMessage::EventType event_type);

  void SendPayloadReceivedAck(
      ClientProxy* client, PendingPayload& pending_payload,
      const std::string& endpoint_id,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t chunk_size, bool is_last_chunk);

  bool WaitForReceivedAck(
      ClientProxy* client, const std::string& endpoint_id,
      PendingPayload& pending_payload,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t payload_chunk_offset, bool is_last_chunk);
  bool IsPayloadReceivedAckEnabled(ClientProxy* client,
                                   const std::string& endpoint_id,
                                   PendingPayload& pending_payload);

  // Handles a finished outgoing payload for the given endpointIds. All
  // statuses except for SUCCESS are handled here.
  void HandleFinishedOutgoingPayload(
      ClientProxy* client, const EndpointIds& finished_endpoint_ids,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t num_bytes_successfully_transferred,
      location::nearby::proto::connections::PayloadStatus status = location::
          nearby::proto::connections::PayloadStatus::UNKNOWN_PAYLOAD_STATUS);
  void HandleFinishedIncomingPayload(
      ClientProxy* client, const std::string& endpoint_id,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t offset_bytes,
      location::nearby::proto::connections::PayloadStatus status);

  void HandleSuccessfulOutgoingChunk(
      ClientProxy* client, const std::string& endpoint_id,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int32_t payload_chunk_flags, std::int64_t payload_chunk_offset,
      std::int64_t payload_chunk_body_size);
  void HandleSuccessfulIncomingChunk(
      ClientProxy* client, const std::string& endpoint_id,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int32_t payload_chunk_flags, std::int64_t payload_chunk_offset,
      std::int64_t payload_chunk_body_size);

  void ProcessDataPacket(ClientProxy* to_client,
                         const std::string& from_endpoint_id,
                         PayloadTransferFrame& payload_transfer_frame,
                         Medium medium,
                         analytics::PacketMetaData& packet_meta_data);
  void ProcessControlPacket(ClientProxy* to_client,
                            const std::string& from_endpoint_id,
                            PayloadTransferFrame& payload_transfer_frame);

  void NotifyClientOfIncomingPayloadProgressInfo(
      ClientProxy* client, const std::string& endpoint_id,
      const PayloadProgressInfo& payload_transfer_update)
      RUN_ON_PAYLOAD_STATUS_UPDATE_THREAD();

  SingleThreadExecutor* GetOutgoingPayloadExecutor(PayloadType payload_type);

  void RunOnStatusUpdateThread(const std::string& name,
                               absl::AnyInvocable<void()> runnable);
  bool NotifyShutdown() ABSL_LOCKS_EXCLUDED(mutex_);
  void DestroyPendingPayload(Payload::Id payload_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  PendingPayloadHandle GetPayload(Payload::Id payload_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);
  void CancelAllPayloads() ABSL_LOCKS_EXCLUDED(mutex_);

  void RecordPayloadStartedAnalytics(ClientProxy* client,
                                     const EndpointIds& endpoint_ids,
                                     std::int64_t payload_id,
                                     PayloadType payload_type,
                                     std::int64_t offset,
                                     std::int64_t total_size);
  void RecordInvalidPayloadAnalytics(ClientProxy* client,
                                     const EndpointIds& endpoint_ids,
                                     std::int64_t payload_id,
                                     PayloadType payload_type,
                                     std::int64_t offset,
                                     std::int64_t total_size);

  PayloadType FramePayloadTypeToPayloadType(
      PayloadTransferFrame::PayloadHeader::PayloadType type);

  void OnPendingPayloadDestroy(const PendingPayload* payload);
  mutable Mutex mutex_;
  std::string custom_save_path_;
  AtomicBoolean shutdown_{false};
  std::unique_ptr<CountDownLatch> shutdown_barrier_;
  int send_payload_count_ = 0;
  SingleThreadExecutor bytes_payload_executor_;
  SingleThreadExecutor file_payload_executor_;
  SingleThreadExecutor stream_payload_executor_;
  SingleThreadExecutor payload_status_update_executor_;
  PendingPayloads pending_payloads_;
  EndpointManager* endpoint_manager_;

  // When callback processing cannot keep the speed of callback update, the
  // callback thread will be lag to the real transfer. In order to keep sync
  // between callback and sending/receiving threads, we will skip
  // non-important callbacks during file transfer.
  mutable Mutex chunk_update_mutex_;
  int outgoing_chunk_update_count_ ABSL_GUARDED_BY(chunk_update_mutex_) = 0;
  int incoming_chunk_update_count_ ABSL_GUARDED_BY(chunk_update_mutex_) = 0;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_PAYLOAD_MANAGER_H_
