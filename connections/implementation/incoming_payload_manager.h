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

#ifndef CORE_INTERNAL_INCOMING_PAYLOAD_MANAGER_H_
#define CORE_INTERNAL_INCOMING_PAYLOAD_MANAGER_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

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
#include "connections/listeners.h"
#include "connections/payload.h"
#include "internal/platform/expected.h"
#include "internal/platform/single_thread_executor.h"
#include "proto/connections_enums.pb.h"

namespace nearby::connections {

class IncomingPayloadManager {
 public:
  IncomingPayloadManager(EndpointManager& endpoint_manager,
                         SingleThreadExecutor& status_update_executor,
                         std::atomic<bool>& shutdown);
  ~IncomingPayloadManager() = default;

  IncomingPayloadManager(const IncomingPayloadManager&) = delete;
  IncomingPayloadManager& operator=(const IncomingPayloadManager&) = delete;

  void SetCustomSavePath(ClientProxy* client, const std::string& path);

  // Returns true if `payload_id` was tracked as an incoming payload and
  // marked canceled.
  bool CancelPayload(Payload::Id payload_id) ABSL_LOCKS_EXCLUDED(mutex_);
  void CancelAllPayloads() ABSL_LOCKS_EXCLUDED(mutex_);
  void StopTrackingAllPayloads() ABSL_LOCKS_EXCLUDED(mutex_);

  void OnEndpointDisconnect(
      ClientProxy* client, const std::string& endpoint_id,
      location::nearby::proto::connections::PayloadStatus payload_status,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code) ABSL_LOCKS_EXCLUDED(mutex_);

  // @EndpointManagerDataPool
  void ProcessDataPacket(ClientProxy* to_client,
                         const std::string& from_endpoint_id,
                         location::nearby::connections::PayloadTransferFrame&
                             payload_transfer_frame,
                         location::nearby::proto::connections::Medium medium)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // @EndpointManagerDataPool
  // Returns true if `payload_transfer_frame` matched a tracked incoming
  // payload.
  bool ProcessControlPacket(ClientProxy* to_client,
                            const std::string& from_endpoint_id,
                            location::nearby::connections::PayloadTransferFrame&
                                payload_transfer_frame)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  class IncomingPendingPayload : public PendingPayload {
   public:
    IncomingPendingPayload(
        std::unique_ptr<IncomingInternalPayload> incoming_payload,
        absl::string_view endpoint_id);

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

  void StartTrackingPayload(
      Payload::Id payload_id,
      std::unique_ptr<IncomingPendingPayload> pending_payload)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void StopTrackingPayload(Payload::Id payload_id) ABSL_LOCKS_EXCLUDED(mutex_);
  void Remove(
      absl::flat_hash_map<
          Payload::Id, std::shared_ptr<IncomingPendingPayload>>::iterator it)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Creates an incoming payload and returns a handle to it.
  // If `save_path` is empty, the payload will be saved to the default save
  // path set in `SetCustomSavePath()`.
  ErrorOr<std::shared_ptr<IncomingPendingPayload>> CreateIncomingPayload(
      const location::nearby::connections::PayloadTransferFrame& frame,
      const std::string& endpoint_id, const std::string& save_path)
      ABSL_LOCKS_EXCLUDED(mutex_);

  std::shared_ptr<IncomingPendingPayload> GetIncomingPayload(
      Payload::Id payload_id) const ABSL_LOCKS_EXCLUDED(mutex_);

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
          operation_result_code)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(status_update_executor_)
          ABSL_LOCKS_EXCLUDED(mutex_);

  void HandleSuccessfulIncomingChunk(
      ClientProxy* client, const std::string& endpoint_id,
      const location::nearby::connections::PayloadTransferFrame::PayloadHeader&
          payload_header,
      int32_t payload_chunk_flags, int64_t payload_chunk_offset,
      int64_t payload_chunk_body_size);

  void NotifyClientOfIncomingPayloadProgressInfo(
      ClientProxy* client, const std::string& endpoint_id,
      const PayloadProgressInfo& payload_transfer_update)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(status_update_executor_);

  void RunOnStatusUpdateThread(const std::string& name,
                               absl::AnyInvocable<void()> runnable);

  mutable absl::Mutex mutex_;
  std::string custom_save_path_;
  absl::flat_hash_map<Payload::Id, std::shared_ptr<IncomingPendingPayload>>
      pending_payloads_ ABSL_GUARDED_BY(mutex_);
  EndpointManager* endpoint_manager_;
  SingleThreadExecutor* status_update_executor_;
  std::atomic<bool>* shutdown_;

  mutable absl::Mutex chunk_update_mutex_;
  int incoming_chunk_update_count_ ABSL_GUARDED_BY(chunk_update_mutex_) = 0;
  absl::Time last_incoming_chunk_update_time_
      ABSL_GUARDED_BY(chunk_update_mutex_) = absl::InfinitePast();
};

}  // namespace nearby::connections

#endif  // CORE_INTERNAL_INCOMING_PAYLOAD_MANAGER_H_
