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

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
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
                            CountDownLatch barrier) override;

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

    std::string id;
    AtomicReference<Status> status{Status::kUnknown};
    std::int64_t offset = 0;
  };

  // Tracks state for an InternalPayload and the endpoints associated with it.
  class PendingPayload {
   public:
    PendingPayload(std::unique_ptr<InternalPayload> internal_payload,
                   const EndpointIds& endpoint_ids, bool is_incoming);
    PendingPayload(PendingPayload&&) = default;
    PendingPayload& operator=(PendingPayload&&) = default;

    ~PendingPayload() { Close(); }

    Payload::Id GetId() const;

    InternalPayload* GetInternalPayload();

    bool IsLocallyCanceled() const;
    void MarkLocallyCanceled();
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

    // Closes internal_payload_ and triggers close_event_.
    // Close is called when a pending peyload does not have associated
    // endpoints.
    void Close();

    // Waits for close_event_  or for timeout to happen.
    // Returns true, if event happened, false otherwise.
    bool WaitForClose();
    bool IsClosed();

   private:
    mutable Mutex mutex_;
    bool is_incoming_;
    AtomicBoolean is_locally_canceled_{false};
    CountDownLatch close_event_{1};
    std::unique_ptr<InternalPayload> internal_payload_;
    absl::flat_hash_map<std::string, EndpointInfo> endpoints_
        ABSL_GUARDED_BY(mutex_);
  };

  // Tracks and manages PendingPayload objects in a synchronized manner.
  class PendingPayloads {
   public:
    PendingPayloads() = default;
    ~PendingPayloads() = default;

    void StartTrackingPayload(Payload::Id payload_id,
                              std::unique_ptr<PendingPayload> pending_payload)
        ABSL_LOCKS_EXCLUDED(mutex_);
    std::unique_ptr<PendingPayload> StopTrackingPayload(Payload::Id payload_id)
        ABSL_LOCKS_EXCLUDED(mutex_);
    PendingPayload* GetPayload(Payload::Id payload_id) const
        ABSL_LOCKS_EXCLUDED(mutex_);
    std::vector<Payload::Id> GetAllPayloads() ABSL_LOCKS_EXCLUDED(mutex_);

   private:
    mutable Mutex mutex_;
    absl::flat_hash_map<Payload::Id, std::unique_ptr<PendingPayload>>
        pending_payloads_ ABSL_GUARDED_BY(mutex_);
  };

  using Endpoints = std::vector<const EndpointInfo*>;
  static std::string ToString(const EndpointIds& endpoint_ids);
  static std::string ToString(const Endpoints& endpoints);
  static std::string ToString(PayloadType type);
  static std::string ToString(EndpointInfo::Status status);

  // Splits the endpoints for this payload by availability.
  // Returns a pair of lists of EndpointInfo*, with the first being the list of
  // still-available endpoints, and the second for unavailable endpoints.
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

  // Converts the status of an endpoint that's been set out-of-band via a remote
  // ControlMessage to the PayloadStatus for handling of that endpoint-payload
  // pair.
  static location::nearby::proto::connections::PayloadStatus
  EndpointInfoStatusToPayloadStatus(EndpointInfo::Status status);
  // Converts a ControlMessage::EventType for a particular payload to a
  // PayloadStatus. Called when we've received a ControlMessage with this event
  // from a remote endpoint; thus the PayloadStatuses are REMOTE_*.
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

  PendingPayload* CreateIncomingPayload(const PayloadTransferFrame& frame,
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

  // Handles a finished outgoing payload for the given endpointIds. All statuses
  // except for SUCCESS are handled here.
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
                               std::function<void()> runnable);
  bool NotifyShutdown() ABSL_LOCKS_EXCLUDED(mutex_);
  void DestroyPendingPayload(Payload::Id payload_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  PendingPayload* GetPayload(Payload::Id payload_id) const
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

  mutable Mutex mutex_;
  std::string custom_save_path_;
  AtomicBoolean shutdown_{false};
  std::unique_ptr<CountDownLatch> shutdown_barrier_;
  int send_payload_count_ = 0;
  PendingPayloads pending_payloads_ ABSL_GUARDED_BY(mutex_);
  SingleThreadExecutor bytes_payload_executor_;
  SingleThreadExecutor file_payload_executor_;
  SingleThreadExecutor stream_payload_executor_;
  SingleThreadExecutor payload_status_update_executor_;

  EndpointManager* endpoint_manager_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_PAYLOAD_MANAGER_H_
