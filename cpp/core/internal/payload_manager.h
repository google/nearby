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
#include <map>
#include <vector>

#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/internal_payload.h"
#include "core/internal/internal_payload_factory.h"
#include "core/internal/loop_runner.h"
#include "core/listeners.h"
#include "core/payload.h"
#include "core/status.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/api/count_down_latch.h"
#include "platform/api/lock.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "platform/runnable.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

namespace payload_manager {

template <typename>
class SendPayloadRunnable;
template <typename>
class ProcessEndpointDisconnectionRunnable;
template <typename>
class SendClientCallbacksForFinishedOutgoingPayloadRunnable;
template <typename>
class SendClientCallbacksForFinishedIncomingPayloadRunnable;
template <typename>
class HandleSuccessfulOutgoingChunkRunnable;
template <typename>
class HandleSuccessfulIncomingChunkRunnable;

}  // namespace payload_manager

template <typename Platform>
class PayloadManager
    : public EndpointManager<Platform>::IncomingOfflineFrameProcessor {
 public:
  explicit PayloadManager(Ptr<EndpointManager<Platform> > endpoint_manager);
  ~PayloadManager() override;

  void sendPayload(Ptr<ClientProxy<Platform> > client_proxy,
                   const std::vector<string>& endpoint_ids,
                   ConstPtr<Payload> payload);
  Status::Value cancelPayload(Ptr<ClientProxy<Platform> > client_proxy,
                              std::int64_t payload_id);

  // @EndpointManagerReaderThread
  void processIncomingOfflineFrame(
      ConstPtr<OfflineFrame> offline_frame, const string& from_endpoint_id,
      Ptr<ClientProxy<Platform> > to_client_proxy,
      proto::connections::Medium current_medium) override;

  // @EndpointManagerThread
  void processEndpointDisconnection(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      Ptr<CountDownLatch> process_disconnection_barrier) override;

 private:
  // Information about an endpoint for a particular payload.
  class EndpointInfo {
   public:
    // Status set for the endpoint out-of-band via a ControlMessage.
    struct Status {
      enum Value { UNKNOWN, AVAILABLE, CANCELED, ERROR };
    };

    explicit EndpointInfo(string id);

    string getId() const;
    typename EndpointInfo::Status::Value getStatus() const;
    std::int64_t getOffset() const;

    void setStatus(const PayloadTransferFrame::ControlMessage& control_message);
    void setOffset(std::int64_t offset);

   private:
    static typename Status::Value controlMessageEventToEndpointInfoStatus(
        PayloadTransferFrame::ControlMessage::EventType event);

    const string id_;
    typename Status::Value status_;
    std::int64_t offset_;
  };

  // Tracks state for an InternalPayload and the endpoints associated with it.
  class PendingPayload {
   public:
    static Ptr<PendingPayload> createIncoming(
        Ptr<InternalPayload> internal_payload, const string& endpoint_id);
    static Ptr<PendingPayload> createOutgoing(
        Ptr<InternalPayload> internal_payload,
        const std::vector<string>& endpoint_ids);

    ~PendingPayload();

    std::int64_t getId();

    Ptr<InternalPayload> getInternalPayload();

    bool isLocallyCanceled();
    void markLocallyCanceled();
    bool isIncoming();

    // Gets the EndpointInfo objects for the endpoints (still) associated with
    // this payload.
    std::vector<Ptr<EndpointInfo> > getEndpoints() const;
    // Returns the EndpointInfo for a given endpoint ID. Returns null if the
    // endpoint is not associated with this payload.
    Ptr<EndpointInfo> getEndpoint(const string& endpoint_id);

    // Removes the given endpoints, e.g. on error.
    void removeEndpoints(const std::vector<string>& endpoint_ids_to_remove);

    // Sets the status for a particular endpoint.
    void setEndpointStatusFromControlMessage(
        const string& endpoint_id,
        const PayloadTransferFrame::ControlMessage& control_message);

    // Sets the offset for a particular endpoint.
    void setOffsetForEndpoint(const string& endpoint_id, std::int64_t offset);

    void close();

   private:
    PendingPayload(Ptr<InternalPayload> internal_payload,
                   const std::vector<string>& endpoint_ids, bool is_incoming);

    ScopedPtr<Ptr<Lock> > lock_;

    ScopedPtr<Ptr<InternalPayload> > internal_payload_;
    const bool is_incoming_;
    ScopedPtr<Ptr<AtomicBoolean> > is_locally_cancelled_;
    typedef std::map<string, Ptr<EndpointInfo> > EndpointsMap;
    EndpointsMap endpoints_;
  };

  // Tracks and manages PendingPayload objects in a synchronized manner.
  class PendingPayloads {
   public:
    PendingPayloads();
    ~PendingPayloads();

    void startTrackingPayload(std::int64_t payload_id,
                              Ptr<PendingPayload> pending_payload);
    Ptr<PendingPayload> stopTrackingPayload(std::int64_t payload_id);
    Ptr<PendingPayload> getPayload(std::int64_t payload_id);
    std::vector<Ptr<PendingPayload> > getAllPayloads();

   private:
    ScopedPtr<Ptr<Lock> > lock_;
    typedef std::map<std::int64_t, Ptr<PendingPayload> > PendingPayloadsMap;
    PendingPayloadsMap pending_payloads_;
  };

  template <typename>
  friend class payload_manager::SendPayloadRunnable;
  template <typename>
  friend class payload_manager::ProcessEndpointDisconnectionRunnable;
  template <typename>
  friend class payload_manager::
      SendClientCallbacksForFinishedOutgoingPayloadRunnable;
  template <typename>
  friend class payload_manager::
      SendClientCallbacksForFinishedIncomingPayloadRunnable;
  template <typename>
  friend class payload_manager::HandleSuccessfulOutgoingChunkRunnable;
  template <typename>
  friend class payload_manager::HandleSuccessfulIncomingChunkRunnable;

  // Converts the status of an endpoint that's been set out-of-band via a remote
  // ControlMessage to the PayloadStatus for handling of that endpoint-payload
  // pair.
  static proto::connections::PayloadStatus endpointInfoStatusToPayloadStatus(
      typename EndpointInfo::Status::Value status);
  // Converts a ControlMessage::EventType for a particular payload to a
  // PayloadStatus. Called when we've received a ControlMessage with this event
  // from a remote endpoint; thus the PayloadStatuses are REMOTE_*.
  static proto::connections::PayloadStatus controlMessageEventToPayloadStatus(
      PayloadTransferFrame::ControlMessage::EventType event);
  static PayloadTransferUpdate::Status::Value
  payloadStatusToTransferUpdateStatus(proto::connections::PayloadStatus status);

  ConstPtr<PayloadTransferFrame::PayloadHeader> createPayloadHeader(
      ConstPtr<InternalPayload> internal_payload);
  ConstPtr<PayloadTransferFrame::PayloadChunk> createPayloadChunk(
      std::int64_t payload_chunk_offset,
      ConstPtr<ByteArray> payload_chunk_body);

  Ptr<PendingPayload> createIncomingPayload(
      const PayloadTransferFrame& payload_transfer_frame,
      const string& endpoint_id);

  void sendClientCallbacksForFinishedOutgoingPayload(
      Ptr<ClientProxy<Platform> > client_proxy,
      const std::vector<string>& finished_endpoint_ids,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t num_bytes_successfully_transferred,
      proto::connections::PayloadStatus status);
  void sendClientCallbacksForFinishedIncomingPayload(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t offset_bytes, proto::connections::PayloadStatus status);

  void sendControlMessage(
      const std::vector<string>& endpoint_ids,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t num_bytes_successfully_transferred,
      PayloadTransferFrame::ControlMessage::EventType event_type);

  // Handles a finished outgoing payload for the given endpointIds. All statuses
  // except for SUCCESS are handled here.
  void handleFinishedOutgoingPayload(
      Ptr<ClientProxy<Platform> > client_proxy,
      const std::vector<string>& finished_endpoint_ids,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t num_bytes_successfully_transferred,
      proto::connections::PayloadStatus status);
  void handleFinishedIncomingPayload(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t offset_bytes, proto::connections::PayloadStatus status);

  void handleSuccessfulOutgoingChunk(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int32_t payload_chunk_flags, std::int64_t payload_chunk_offset,
      std::int64_t payload_chunk_body_size);
  void handleSuccessfulIncomingChunk(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int32_t payload_chunk_flags, std::int64_t payload_chunk_offset,
      std::int64_t payload_chunk_body_size);

  void processDataPacket(Ptr<ClientProxy<Platform> > to_client_proxy,
                         const string& from_endpoint_id,
                         const PayloadTransferFrame& payload_transfer_frame);
  void processControlPacket(Ptr<ClientProxy<Platform> > to_client_proxy,
                            const string& from_endpoint_id,
                            const PayloadTransferFrame& payload_transfer_frame);

  // @PayloadStatusUpdateThread
  void notifyClientOfIncomingPayloadTransferUpdate(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      const PayloadTransferUpdate& payload_transfer_update,
      bool done_with_payload);

  Ptr<typename Platform::SingleThreadExecutorType> getOutgoingPayloadExecutor(
      Payload::Type::Value payload_type);

  void enqueueOutgoingPayload(
      Ptr<typename Platform::SingleThreadExecutorType> executor,
      Ptr<Runnable> runnable);

  ScopedPtr<Ptr<InternalPayloadFactory<Platform> > > internal_payload_factory_;
  ScopedPtr<Ptr<LoopRunner> > send_payload_loop_runner_;
  ScopedPtr<Ptr<PendingPayloads> > pending_payloads_;

  ScopedPtr<Ptr<typename Platform::SingleThreadExecutorType> >
      bytes_payload_executor_;
  ScopedPtr<Ptr<typename Platform::SingleThreadExecutorType> >
      file_payload_executor_;
  ScopedPtr<Ptr<typename Platform::SingleThreadExecutorType> >
      stream_payload_executor_;
  ScopedPtr<Ptr<typename Platform::SingleThreadExecutorType> >
      payload_status_update_executor_;

  Ptr<EndpointManager<Platform> > endpoint_manager_;
  std::shared_ptr<PayloadManager> self_{this, [](void*){}};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/payload_manager.cc"

#endif  // CORE_INTERNAL_PAYLOAD_MANAGER_H_
