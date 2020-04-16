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

#include "core/internal/payload_manager.h"

#include <algorithm>
#include <utility>

#include "platform/synchronized.h"

namespace location {
namespace nearby {
namespace connections {

namespace payload_manager {

template <typename K, typename V>
void eraseOwnedPtrFromMap(std::map<K, Ptr<V> >& m, const K& k) {
  typename std::map<K, Ptr<V> >::iterator it = m.find(k);
  if (it != m.end()) {
    it->second.destroy();
    m.erase(it);
  }
}

template <typename Platform>
class SendPayloadRunnable : public Runnable {
 public:
  SendPayloadRunnable(Ptr<PayloadManager<Platform> > payload_manager,
                      Ptr<ClientProxy<Platform> > client_proxy,
                      const std::vector<string>& endpoint_ids,
                      ConstPtr<Payload> payload)
      : payload_manager_(payload_manager),
        client_proxy_(client_proxy),
        endpoint_ids_(endpoint_ids),
        payload_(payload) {}

  void run() override {
    // If successfully created, pending_payload is owned by
    // PayloadManager::pending_payloads_ until
    // PayloadManager::PendingPayloads::stopTrackingPayload() is invoked.
    Ptr<typename PayloadManager<Platform>::PendingPayload> pending_payload(
        createOutgoingPayload(payload_.release(), endpoint_ids_));
    if (pending_payload.isNull()) {
      // TODO(tracyzhou): Add logging.
      return;
    }

    ScopedPtr<ConstPtr<PayloadTransferFrame::PayloadHeader> > payload_header(
        payload_manager_->createPayloadHeader(
            ConstifyPtr(pending_payload->getInternalPayload())));

    payload_manager_->send_payload_loop_runner_->loop(
        MakePtr(new LoopCallable(payload_manager_, client_proxy_,
                                 pending_payload, payload_header.get())));
  }

 private:
  class LoopCallable : public Callable<bool> {
   public:
    LoopCallable(
        Ptr<PayloadManager<Platform> > payload_manager,
        Ptr<ClientProxy<Platform> > client_proxy,
        Ptr<typename PayloadManager<Platform>::PendingPayload> pending_payload,
        ConstPtr<PayloadTransferFrame::PayloadHeader> payload_header)
        : next_chunk_offset_(0),
          payload_manager_(payload_manager),
          client_proxy_(client_proxy),
          pending_payload_(pending_payload),
          payload_header_(payload_header) {}

    ExceptionOr<bool> call() override {
      AvailableAndUnavailableEndpoints available_and_unavailable_endpoints =
          getAvailableAndUnavailableEndpoints(ConstifyPtr(pending_payload_));
      const UnavailableEndpoints& unavailable_endpoints =
          available_and_unavailable_endpoints.second;

      // First, handle any non-available endpoints.
      for (typename UnavailableEndpoints::const_iterator it =
               unavailable_endpoints.begin();
           it != unavailable_endpoints.end(); it++) {
        Ptr<typename PayloadManager<Platform>::EndpointInfo> endpoint_info =
            *it;
        payload_manager_->handleFinishedOutgoingPayload(
            client_proxy_, std::vector<string>(1, endpoint_info->getId()),
            *payload_header_, next_chunk_offset_,
            PayloadManager<Platform>::endpointInfoStatusToPayloadStatus(
                endpoint_info->getStatus()));
      }

      // Update the still-active recipients of this payload.
      const AvailableEndpointIds& available_endpoint_ids =
          available_and_unavailable_endpoints.first;
      if (available_endpoint_ids.empty()) {
        // TODO(tracyzhou): Add logging.
        return ExceptionOr<bool>(false);
      }

      // Check if the payload has been cancelled by the client and, if so,
      // notify the remaining recipients.
      if (pending_payload_->isLocallyCanceled()) {
        // TODO(tracyzhou): Add logging.
        payload_manager_->handleFinishedOutgoingPayload(
            client_proxy_, available_endpoint_ids, *payload_header_,
            next_chunk_offset_,
            proto::connections::PayloadStatus::LOCAL_CANCELLATION);
        return ExceptionOr<bool>(false);
      }

      // Update the current offsets for all endpoints still active for this
      // payload. For the sake of accuracy, we update the pending payload here
      // because it's after all payload terminating events are handled, but
      // right before we actually start detaching the next chunk.
      for (AvailableEndpointIds::const_iterator it =
               available_endpoint_ids.begin();
           it != available_endpoint_ids.end(); it++) {
        const string& endpoint_id = *it;
        pending_payload_->setOffsetForEndpoint(endpoint_id, next_chunk_offset_);
      }

      ExceptionOr<ConstPtr<ByteArray> > next_chunk =
          pending_payload_->getInternalPayload()->detachNextChunk();
      if (!next_chunk.ok()) {
        if (Exception::IO == next_chunk.exception()) {
          // TODO(tracyzhou): Add logging.
          payload_manager_->handleFinishedOutgoingPayload(
              client_proxy_, available_endpoint_ids, *payload_header_,
              next_chunk_offset_,
              proto::connections::PayloadStatus::LOCAL_ERROR);
          return ExceptionOr<bool>(false);
        }
      }

      ScopedPtr<ConstPtr<ByteArray> > scoped_next_chunk(next_chunk.result());
      ScopedPtr<ConstPtr<PayloadTransferFrame::PayloadChunk> > payload_chunk(
          payload_manager_->createPayloadChunk(next_chunk_offset_,
                                               scoped_next_chunk.get()));
      std::vector<string> failed_endpoint_ids =
          payload_manager_->endpoint_manager_->sendPayloadChunk(
              *payload_header_, *payload_chunk, available_endpoint_ids);

      // Check whether at least one endpoint failed.
      if (!failed_endpoint_ids.empty()) {
        payload_manager_->handleFinishedOutgoingPayload(
            client_proxy_, failed_endpoint_ids, *payload_header_,
            next_chunk_offset_,
            proto::connections::PayloadStatus::ENDPOINT_IO_ERROR);
      }

      // Check whether at least one endpoint succeeded -- if they all failed,
      // we'll just go right back to the top of the loop and break out when
      // availableEndpointIds is re-synced and found to be empty at that point.
      if (failed_endpoint_ids.size() < available_endpoint_ids.size()) {
        for (std::vector<string>::const_iterator it =
                 available_endpoint_ids.begin();
             it != available_endpoint_ids.end(); it++) {
          const string& endpoint_id = *it;
          if (std::find(failed_endpoint_ids.begin(), failed_endpoint_ids.end(),
                        endpoint_id) == failed_endpoint_ids.end()) {
            payload_manager_->handleSuccessfulOutgoingChunk(
                client_proxy_, endpoint_id, *payload_header_,
                payload_chunk->flags(), payload_chunk->offset(),
                payload_chunk->body().size());
          }
        }

        // TODO(tracyzhou): Add logging.
        if (scoped_next_chunk.isNull()) {
          // That was the last chunk, we're outta here.
          return ExceptionOr<bool>(false);
        }

        next_chunk_offset_ += scoped_next_chunk->size();
      }
      return ExceptionOr<bool>(true);
    }

   private:
    typedef std::vector<string> AvailableEndpointIds;
    typedef std::vector<Ptr<typename PayloadManager<Platform>::EndpointInfo> >
        UnavailableEndpoints;
    typedef std::pair<AvailableEndpointIds, UnavailableEndpoints>
        AvailableAndUnavailableEndpoints;

    // Splits the endpoints for this payload by availability. Returns a pair of
    // lists, with the first being the list of still-available endpoint IDs and
    // the second the list of EndpointInfos for unavailable endpoints.
    static AvailableAndUnavailableEndpoints getAvailableAndUnavailableEndpoints(
        ConstPtr<typename PayloadManager<Platform>::PendingPayload>
            pending_payload) {
      AvailableEndpointIds available_endpoint_ids;
      UnavailableEndpoints unavailable_endpoints;
      std::vector<Ptr<typename PayloadManager<Platform>::EndpointInfo> >
          endpoints = pending_payload->getEndpoints();
      for (typename std::vector<Ptr<typename PayloadManager<
               Platform>::EndpointInfo> >::const_iterator it =
               endpoints.begin();
           it != endpoints.end(); it++) {
        Ptr<typename PayloadManager<Platform>::EndpointInfo> endpoint_info =
            *it;
        if (PayloadManager<Platform>::EndpointInfo::Status::AVAILABLE ==
            endpoint_info->getStatus()) {
          available_endpoint_ids.push_back(endpoint_info->getId());
        } else {
          unavailable_endpoints.push_back(endpoint_info);
        }
      }
      return std::make_pair(available_endpoint_ids, unavailable_endpoints);
    }

    // Keep track of the chunk offset across iterations.
    std::int64_t next_chunk_offset_;
    Ptr<PayloadManager<Platform> > payload_manager_;
    Ptr<ClientProxy<Platform> > client_proxy_;
    Ptr<typename PayloadManager<Platform>::PendingPayload> pending_payload_;
    ConstPtr<PayloadTransferFrame::PayloadHeader> payload_header_;
  };

  // Creates and starts tracking a PendingPayload for this Payload. Returns null
  // if unable to create the InternalPayload.
  Ptr<typename PayloadManager<Platform>::PendingPayload> createOutgoingPayload(
      ConstPtr<Payload> payload, const std::vector<string>& endpoint_ids) {
    ScopedPtr<ConstPtr<Payload> > scoped_payload(payload);

    ScopedPtr<Ptr<InternalPayload> > internal_payload(
        payload_manager_->internal_payload_factory_->createOutgoing(
            scoped_payload.release()));
    if (internal_payload.isNull()) {
      return Ptr<typename PayloadManager<Platform>::PendingPayload>();
    }

    std::int64_t payload_id = internal_payload->getId();
    ScopedPtr<Ptr<typename PayloadManager<Platform>::PendingPayload> >
        pending_payload(
            PayloadManager<Platform>::PendingPayload::createOutgoing(
                internal_payload.release(), endpoint_ids));
    payload_manager_->pending_payloads_->startTrackingPayload(
        payload_id, pending_payload.release());

    return payload_manager_->pending_payloads_->getPayload(payload_id);
  }

  Ptr<PayloadManager<Platform> > payload_manager_;
  Ptr<ClientProxy<Platform> > client_proxy_;
  std::vector<string> endpoint_ids_;
  ScopedPtr<ConstPtr<Payload> > payload_;
};

template <typename Platform>
class ProcessEndpointDisconnectionRunnable : public Runnable {
 public:
  ProcessEndpointDisconnectionRunnable(
      Ptr<PayloadManager<Platform> > payload_manager,
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      Ptr<CountDownLatch> process_disconnection_barrier)
      : payload_manager_(payload_manager),
        client_proxy_(client_proxy),
        endpoint_id_(endpoint_id),
        process_disconnection_barrier_(process_disconnection_barrier) {}

  void run() override {
    std::vector<string> endpoints_to_remove(1, endpoint_id_);

    // Iterate through all our payloads and look for payloads associated with
    // this endpoint.
    std::vector<Ptr<typename PayloadManager<Platform>::PendingPayload> >
        pending = payload_manager_->pending_payloads_->getAllPayloads();
    for (typename std::vector<Ptr<typename PayloadManager<
             Platform>::PendingPayload> >::const_iterator it = pending.begin();
         it != pending.end(); it++) {
      Ptr<typename PayloadManager<Platform>::PendingPayload> pending_payload =
          *it;
      Ptr<typename PayloadManager<Platform>::EndpointInfo> endpoint_info =
          pending_payload->getEndpoint(endpoint_id_);
      if (endpoint_info.isNull()) {
        continue;
      }

      // Stop tracking the endpoint for this payload.
      pending_payload->removeEndpoints(endpoints_to_remove);

      std::int64_t payload_id = pending_payload->getId();
      std::int64_t payload_total_size =
          pending_payload->getInternalPayload()->getTotalSize();

      // If no endpoints are left for this payload, stop tracking it and close
      // it.
      if (pending_payload->getEndpoints().empty()) {
        pending_payload =
            payload_manager_->pending_payloads_->stopTrackingPayload(
                pending_payload->getId());
        pending_payload->close();
        pending_payload.destroy();
      }

      // Create the payload transfer update.
      PayloadTransferUpdate update(
          payload_id, PayloadTransferUpdate::Status::FAILURE,
          payload_total_size, endpoint_info->getOffset());

      // Send a client notification of a payload transfer failure.
      client_proxy_->onPayloadTransferUpdate(endpoint_id_, update);
    }

    process_disconnection_barrier_->countDown();
  }

 private:
  Ptr<PayloadManager<Platform> > payload_manager_;
  Ptr<ClientProxy<Platform> > client_proxy_;
  const string endpoint_id_;
  Ptr<CountDownLatch> process_disconnection_barrier_;
};

template <typename Platform>
class SendClientCallbacksForFinishedOutgoingPayloadRunnable : public Runnable {
 public:
  SendClientCallbacksForFinishedOutgoingPayloadRunnable(
      Ptr<PayloadManager<Platform> > payload_manager,
      Ptr<ClientProxy<Platform> > client_proxy,
      const std::vector<string>& finished_endpoint_ids,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t num_bytes_successfully_transferred,
      proto::connections::PayloadStatus status)
      : payload_manager_(payload_manager),
        client_proxy_(client_proxy),
        finished_endpoint_ids_(finished_endpoint_ids),
        payload_header_(payload_header),
        num_bytes_successfully_transferred_(num_bytes_successfully_transferred),
        status_(status) {}

  void run() override {
    // Make sure we're still tracking this payload.
    Ptr<typename PayloadManager<Platform>::PendingPayload> pending_payload =
        payload_manager_->pending_payloads_->getPayload(payload_header_.id());
    if (pending_payload.isNull()) {
      return;
    }

    PayloadTransferUpdate update(
        payload_header_.id(),
        PayloadManager<Platform>::payloadStatusToTransferUpdateStatus(status_),
        payload_header_.total_size(), num_bytes_successfully_transferred_);
    for (std::vector<string>::const_iterator it =
             finished_endpoint_ids_.begin();
         it != finished_endpoint_ids_.end(); it++) {
      const string& endpoint_id = *it;

      // Skip sending notifications if we have stopped tracking this endpoint.
      if (pending_payload->getEndpoint(endpoint_id).isNull()) {
        continue;
      }

      // Notify the client.
      client_proxy_->onPayloadTransferUpdate(endpoint_id, update);
    }

    // Remove these endpoints from our tracking list for this payload.
    pending_payload->removeEndpoints(finished_endpoint_ids_);

    // Close the payload and stop tracking it if no endpoints remain.
    if (pending_payload->getEndpoints().empty()) {
      pending_payload =
          payload_manager_->pending_payloads_->stopTrackingPayload(
              payload_header_.id());
      pending_payload->close();
      pending_payload.destroy();
    }
  }

 private:
  Ptr<PayloadManager<Platform> > payload_manager_;
  Ptr<ClientProxy<Platform> > client_proxy_;
  const std::vector<string> finished_endpoint_ids_;
  const PayloadTransferFrame::PayloadHeader payload_header_;
  const std::int64_t num_bytes_successfully_transferred_;
  const proto::connections::PayloadStatus status_;
};

template <typename Platform>
class SendClientCallbacksForFinishedIncomingPayloadRunnable : public Runnable {
 public:
  SendClientCallbacksForFinishedIncomingPayloadRunnable(
      Ptr<PayloadManager<Platform> > payload_manager,
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int64_t offset_bytes, proto::connections::PayloadStatus status)
      : payload_manager_(payload_manager),
        client_proxy_(client_proxy),
        endpoint_id_(endpoint_id),
        payload_header_(payload_header),
        offset_bytes_(offset_bytes),
        status_(status) {}

  void run() override {
    // Make sure we're still tracking this payload.
    Ptr<typename PayloadManager<Platform>::PendingPayload> pending_payload =
        payload_manager_->pending_payloads_->getPayload(payload_header_.id());
    if (pending_payload.isNull()) {
      return;
    }

    // Unless we never started tracking this payload (meaning we failed to even
    // create the InternalPayload), notify the client (and close it).
    PayloadTransferUpdate update(
        payload_header_.id(),
        PayloadManager<Platform>::payloadStatusToTransferUpdateStatus(status_),
        payload_header_.total_size(), offset_bytes_);
    payload_manager_->notifyClientOfIncomingPayloadTransferUpdate(
        client_proxy_, endpoint_id_, update, /*done_with_payload=*/true);
  }

 private:
  Ptr<PayloadManager<Platform> > payload_manager_;
  Ptr<ClientProxy<Platform> > client_proxy_;
  const string endpoint_id_;
  const PayloadTransferFrame::PayloadHeader payload_header_;
  const std::int64_t offset_bytes_;
  const proto::connections::PayloadStatus status_;
};

template <typename Platform>
class HandleSuccessfulOutgoingChunkRunnable : public Runnable {
 public:
  HandleSuccessfulOutgoingChunkRunnable(
      Ptr<PayloadManager<Platform> > payload_manager,
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int32_t payload_chunk_flags, std::int64_t payload_chunk_offset,
      std::int64_t payload_chunk_body_size)
      : payload_manager_(payload_manager),
        client_proxy_(client_proxy),
        endpoint_id_(endpoint_id),
        payload_header_(payload_header),
        payload_chunk_flags_(payload_chunk_flags),
        payload_chunk_offset_(payload_chunk_offset),
        payload_chunk_body_size_(payload_chunk_body_size) {}

  void run() override {
    // Make sure we're still tracking this payload and its associated endpoint.
    Ptr<typename PayloadManager<Platform>::PendingPayload> pending_payload =
        payload_manager_->pending_payloads_->getPayload(payload_header_.id());
    if (pending_payload.isNull() ||
        pending_payload->getEndpoint(endpoint_id_).isNull()) {
      return;
    }

    bool is_last_chunk = (payload_chunk_flags_ &
                          PayloadTransferFrame::PayloadChunk::LAST_CHUNK) != 0;
    PayloadTransferUpdate update(
        payload_header_.id(),
        is_last_chunk ? PayloadTransferUpdate::Status::SUCCESS
                      : PayloadTransferUpdate::Status::IN_PROGRESS,
        payload_header_.total_size(),
        is_last_chunk ? payload_chunk_offset_
                      : payload_chunk_offset_ + payload_chunk_body_size_);

    // Notify the client.
    client_proxy_->onPayloadTransferUpdate(endpoint_id_, update);

    if (is_last_chunk) {
      // Stop tracking this endpoint.
      pending_payload->removeEndpoints(std::vector<string>(1, endpoint_id_));

      // Close the payload and stop tracking it if no endpoints remain.
      if (pending_payload->getEndpoints().empty()) {
        pending_payload =
            payload_manager_->pending_payloads_->stopTrackingPayload(
                payload_header_.id());
        pending_payload->close();
        pending_payload.destroy();
      }
    }
  }

 private:
  Ptr<PayloadManager<Platform> > payload_manager_;
  Ptr<ClientProxy<Platform> > client_proxy_;
  const string endpoint_id_;
  const PayloadTransferFrame::PayloadHeader payload_header_;
  const std::int32_t payload_chunk_flags_;
  const std::int64_t payload_chunk_offset_;
  const std::int64_t payload_chunk_body_size_;
};

template <typename Platform>
class HandleSuccessfulIncomingChunkRunnable : public Runnable {
 public:
  HandleSuccessfulIncomingChunkRunnable(
      Ptr<PayloadManager<Platform> > payload_manager,
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      const PayloadTransferFrame::PayloadHeader& payload_header,
      std::int32_t payload_chunk_flags, std::int64_t payload_chunk_offset,
      std::int64_t payload_chunk_body_size)
      : payload_manager_(payload_manager),
        client_proxy_(client_proxy),
        endpoint_id_(endpoint_id),
        payload_header_(payload_header),
        payload_chunk_flags_(payload_chunk_flags),
        payload_chunk_offset_(payload_chunk_offset),
        payload_chunk_body_size_(payload_chunk_body_size) {}

  void run() override {
    // Make sure we're still tracking this payload.
    Ptr<typename PayloadManager<Platform>::PendingPayload> pending_payload =
        payload_manager_->pending_payloads_->getPayload(payload_header_.id());
    if (pending_payload.isNull()) {
      return;
    }

    bool is_last_chunk = (payload_chunk_flags_ &
                          PayloadTransferFrame::PayloadChunk::LAST_CHUNK) != 0;
    PayloadTransferUpdate update(
        payload_header_.id(),
        is_last_chunk ? PayloadTransferUpdate::Status::SUCCESS
                      : PayloadTransferUpdate::Status::IN_PROGRESS,
        payload_header_.total_size(),
        is_last_chunk ? payload_chunk_offset_
                      : payload_chunk_offset_ + payload_chunk_body_size_);

    // Notify the client of this update.
    payload_manager_->notifyClientOfIncomingPayloadTransferUpdate(
        client_proxy_, endpoint_id_, update, is_last_chunk);
  }

 private:
  Ptr<PayloadManager<Platform> > payload_manager_;
  Ptr<ClientProxy<Platform> > client_proxy_;
  const string endpoint_id_;
  const PayloadTransferFrame::PayloadHeader payload_header_;
  const std::int32_t payload_chunk_flags_;
  const std::int64_t payload_chunk_offset_;
  const std::int64_t payload_chunk_body_size_;
};

template <typename Platform>
class ProcessDataPacketRunnable : public Runnable {
 public:
  ProcessDataPacketRunnable(Ptr<ClientProxy<Platform> > to_client_proxy,
                            const string& from_endpoint_id,
                            ConstPtr<Payload> payload)
      : to_client_proxy_(to_client_proxy),
        from_endpoint_id_(from_endpoint_id),
        payload_(payload) {}

  void run() override {
    to_client_proxy_->onPayloadReceived(from_endpoint_id_, payload_.release());
  }

 private:
  Ptr<ClientProxy<Platform> > to_client_proxy_;
  const string from_endpoint_id_;
  ScopedPtr<ConstPtr<Payload> > payload_;
};

}  // namespace payload_manager

template <typename Platform>
PayloadManager<Platform>::PayloadManager(
    Ptr<EndpointManager<Platform> > endpoint_manager)
    : internal_payload_factory_(new InternalPayloadFactory<Platform>()),
      send_payload_loop_runner_(new LoopRunner("sendPayload")),
      pending_payloads_(new PendingPayloads()),
      bytes_payload_executor_(Platform::createSingleThreadExecutor()),
      file_payload_executor_(Platform::createSingleThreadExecutor()),
      stream_payload_executor_(Platform::createSingleThreadExecutor()),
      payload_status_update_executor_(Platform::createSingleThreadExecutor()),
      endpoint_manager_(endpoint_manager) {
  endpoint_manager_->registerIncomingOfflineFrameProcessor(
      V1Frame::PAYLOAD_TRANSFER, std::static_pointer_cast<
          typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>(
          self_));
}

template <typename Platform>
PayloadManager<Platform>::~PayloadManager() {
  // TODO(reznor):
  // logger.atDebug().log("Initiating shutdown of PayloadManager");

  // Unregister ourselves from the IncomingOfflineFrameProcessors.
  endpoint_manager_->unregisterIncomingOfflineFrameProcessor(
      V1Frame::CONNECTION_RESPONSE, std::static_pointer_cast<
          typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>(
          self_));

  // Stop all the ongoing Runnables (as gracefully as possible).
  payload_status_update_executor_->shutdown();
  bytes_payload_executor_->shutdown();
  file_payload_executor_->shutdown();
  stream_payload_executor_->shutdown();

  typedef Ptr<typename PayloadManager<Platform>::PendingPayload>
      PtrPendingPayload;

  // Clear our tracked pending payloads.
  std::vector<PtrPendingPayload> pending = pending_payloads_->getAllPayloads();
  for (typename std::vector<PtrPendingPayload>::const_iterator it =
           pending.begin();
       it != pending.end(); it++) {
    PtrPendingPayload pending_payload =
        pending_payloads_->stopTrackingPayload((*it)->getId());
    pending_payload->close();
    pending_payload.destroy();
  }

  // TODO(reznor):
  // logger.atVerbose().log("PayloadManager has shut down.");
}

template <typename Platform>
void PayloadManager<Platform>::sendPayload(
    Ptr<ClientProxy<Platform> > client_proxy,
    const std::vector<string>& endpoint_ids, ConstPtr<Payload> payload) {
  Ptr<typename Platform::SingleThreadExecutorType> send_payload_executor =
      getOutgoingPayloadExecutor(payload->getType());
  // The send_payload_executor will be null if the payload is of a type
  // we cannot work with. This should never be reached since the
  // ServiceControllerRouter has already checked whether or not we can work with
  // this Payload type.
  ScopedPtr<ConstPtr<Payload> > scoped_payload(payload);
  if (send_payload_executor.isNull()) {
    // TODO(tracyzhou): Add logging.
    return;
  }

  // Each payload is sent in FCFS order within each Payload type, blocking any
  // other payload of the same type from even starting until this one is
  // completely done with. If we ever want to provide isolation across
  // ClientProxy objects this will need to be significantly re-architected.
  enqueueOutgoingPayload(
      send_payload_executor,
      MakePtr(new payload_manager::SendPayloadRunnable<Platform>(
          self_, client_proxy, endpoint_ids,
          scoped_payload.release())));
  // TODO(tracyzhou): Add logging.
}

template <typename Platform>
Status::Value PayloadManager<Platform>::cancelPayload(
    Ptr<ClientProxy<Platform> > client_proxy, std::int64_t payload_id) {
  Ptr<typename PayloadManager<Platform>::PendingPayload> canceled_payload =
      pending_payloads_->getPayload(payload_id);
  if (canceled_payload.isNull()) {
    // TODO(tracyzhou): Add logging.
    return Status::PAYLOAD_UNKNOWN;
  }

  // Mark the payload as canceled.
  canceled_payload->markLocallyCanceled();
  // TODO(tracyzhou): Add logging.

  // Return SUCCESS immediately. Remaining cleanup and updates will be sent in
  // sendPayload() or processIncomingOfflineFrame()
  return Status::SUCCESS;
}

template <typename Platform>
void PayloadManager<Platform>::processIncomingOfflineFrame(
    ConstPtr<OfflineFrame> offline_frame, const string& from_endpoint_id,
    Ptr<ClientProxy<Platform> > to_client_proxy,
    proto::connections::Medium current_medium) {
  ScopedPtr<ConstPtr<OfflineFrame> > scoped_offline_frame(offline_frame);
  const PayloadTransferFrame& payload_transfer_frame =
      scoped_offline_frame->v1().payload_transfer();

  switch (payload_transfer_frame.packet_type()) {
    case PayloadTransferFrame::CONTROL:
      processControlPacket(to_client_proxy, from_endpoint_id,
                           payload_transfer_frame);
      break;
    case PayloadTransferFrame::DATA:
      processDataPacket(to_client_proxy, from_endpoint_id,
                        payload_transfer_frame);
      break;
    default:
      // TODO(tracyzhou): Add logging.
      break;
  }
}

template <typename Platform>
void PayloadManager<Platform>::processEndpointDisconnection(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    Ptr<CountDownLatch> process_disconnection_barrier) {
  payload_status_update_executor_->execute(MakePtr(
      new payload_manager::ProcessEndpointDisconnectionRunnable<Platform>(
          self_, client_proxy, endpoint_id,
          process_disconnection_barrier)));
}

template <typename Platform>
proto::connections::PayloadStatus
PayloadManager<Platform>::endpointInfoStatusToPayloadStatus(
    typename EndpointInfo::Status::Value status) {
  switch (status) {
    case EndpointInfo::Status::CANCELED:
      return proto::connections::PayloadStatus::REMOTE_CANCELLATION;
    case EndpointInfo::Status::ERROR:
      return proto::connections::PayloadStatus::REMOTE_ERROR;
    case EndpointInfo::Status::AVAILABLE:
      return proto::connections::PayloadStatus::SUCCESS;
    default:
      // TODO(tracyzhou): Add logging.
      return proto::connections::PayloadStatus::UNKNOWN_PAYLOAD_STATUS;
  }
}

template <typename Platform>
proto::connections::PayloadStatus
PayloadManager<Platform>::controlMessageEventToPayloadStatus(
    PayloadTransferFrame::ControlMessage::EventType event) {
  switch (event) {
    case PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR:
      return proto::connections::PayloadStatus::REMOTE_ERROR;
    case PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED:
      return proto::connections::PayloadStatus::REMOTE_CANCELLATION;
    default:
      // TODO(tracyzhou): Add logging.
      return proto::connections::PayloadStatus::UNKNOWN_PAYLOAD_STATUS;
  }
}

template <typename Platform>
PayloadTransferUpdate::Status::Value
PayloadManager<Platform>::payloadStatusToTransferUpdateStatus(
    proto::connections::PayloadStatus status) {
  switch (status) {
    case proto::connections::LOCAL_CANCELLATION:
    case proto::connections::REMOTE_CANCELLATION:
      return PayloadTransferUpdate::Status::CANCELED;
    case proto::connections::SUCCESS:
      return PayloadTransferUpdate::Status::SUCCESS;
    default:
      return PayloadTransferUpdate::Status::FAILURE;
  }
}

template <typename Platform>
Ptr<typename Platform::SingleThreadExecutorType>
PayloadManager<Platform>::getOutgoingPayloadExecutor(
    Payload::Type::Value payload_type) {
  switch (payload_type) {
    case Payload::Type::BYTES:
      return bytes_payload_executor_.get();
    case Payload::Type::FILE:
      return file_payload_executor_.get();
    case Payload::Type::STREAM:
      return stream_payload_executor_.get();
    default:
      return Ptr<typename Platform::SingleThreadExecutorType>();
  }
}

template <typename Platform>
ConstPtr<PayloadTransferFrame::PayloadHeader>
PayloadManager<Platform>::createPayloadHeader(
    ConstPtr<InternalPayload> internal_payload) {
  ScopedPtr<Ptr<PayloadTransferFrame::PayloadHeader> > payload_header(
      new PayloadTransferFrame::PayloadHeader());

  payload_header->set_id(internal_payload->getId());
  payload_header->set_type(internal_payload->getType());
  payload_header->set_total_size(internal_payload->getTotalSize());

  return ConstifyPtr(payload_header.release());
}

template <typename Platform>
ConstPtr<PayloadTransferFrame::PayloadChunk>
PayloadManager<Platform>::createPayloadChunk(
    std::int64_t payload_chunk_offset, ConstPtr<ByteArray> payload_chunk_body) {
  ScopedPtr<Ptr<PayloadTransferFrame::PayloadChunk> > payload_chunk(
      new PayloadTransferFrame::PayloadChunk());

  payload_chunk->set_offset(payload_chunk_offset);
  if (!payload_chunk_body.isNull()) {
    payload_chunk->set_body(payload_chunk_body->getData(),
                            payload_chunk_body->size());
  }

  // This is a null-initialized Integer, so it needs to be initialized to avoid
  // inadvertent NPEs.
  payload_chunk->set_flags(0);
  if (payload_chunk_body.isNull()) {
    payload_chunk->set_flags(payload_chunk->flags() |
                             PayloadTransferFrame::PayloadChunk::LAST_CHUNK);
  }

  return ConstifyPtr(payload_chunk.release());
}

template <typename Platform>
Ptr<typename PayloadManager<Platform>::PendingPayload>
PayloadManager<Platform>::createIncomingPayload(
    const PayloadTransferFrame& payload_transfer_frame,
    const string& endpoint_id) {
  ScopedPtr<Ptr<InternalPayload> > internal_payload(
      internal_payload_factory_->createIncoming(payload_transfer_frame));
  if (internal_payload.isNull()) {
    return Ptr<typename PayloadManager<Platform>::PendingPayload>();
  }

  std::int64_t payload_id = internal_payload->getId();
  ScopedPtr<Ptr<typename PayloadManager<Platform>::PendingPayload> >
      pending_payload(PendingPayload::createIncoming(internal_payload.release(),
                                                     endpoint_id));
  pending_payloads_->startTrackingPayload(payload_id,
                                          pending_payload.release());

  return pending_payloads_->getPayload(payload_id);
}

template <typename Platform>
void PayloadManager<Platform>::sendClientCallbacksForFinishedOutgoingPayload(
    Ptr<ClientProxy<Platform> > client_proxy,
    const std::vector<string>& finished_endpoint_ids,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int64_t num_bytes_successfully_transferred,
    proto::connections::PayloadStatus status) {
  payload_status_update_executor_->execute(MakePtr(
      new payload_manager::
          SendClientCallbacksForFinishedOutgoingPayloadRunnable<Platform>(
              self_, client_proxy, finished_endpoint_ids,
              payload_header, num_bytes_successfully_transferred, status)));
}

template <typename Platform>
void PayloadManager<Platform>::sendClientCallbacksForFinishedIncomingPayload(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int64_t offset_bytes, proto::connections::PayloadStatus status) {
  payload_status_update_executor_->execute(MakePtr(
      new payload_manager::
          SendClientCallbacksForFinishedIncomingPayloadRunnable<Platform>(
              self_, client_proxy, endpoint_id, payload_header,
              offset_bytes, status)));
}

template <typename Platform>
void PayloadManager<Platform>::sendControlMessage(
    const std::vector<string>& endpoint_ids,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int64_t num_bytes_successfully_transferred,
    PayloadTransferFrame::ControlMessage::EventType event_type) {
  PayloadTransferFrame::ControlMessage control_message;
  control_message.set_event(event_type);
  control_message.set_offset(num_bytes_successfully_transferred);

  endpoint_manager_->sendControlMessage(payload_header, control_message,
                                        endpoint_ids);
}

template <typename Platform>
void PayloadManager<Platform>::handleFinishedOutgoingPayload(
    Ptr<ClientProxy<Platform> > client_proxy,
    const std::vector<string>& finished_endpoint_ids,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int64_t num_bytes_successfully_transferred,
    proto::connections::PayloadStatus status) {
  sendClientCallbacksForFinishedOutgoingPayload(
      client_proxy, finished_endpoint_ids, payload_header,
      num_bytes_successfully_transferred, status);

  switch (status) {
    case proto::connections::PayloadStatus::LOCAL_ERROR:
      sendControlMessage(finished_endpoint_ids, payload_header,
                         num_bytes_successfully_transferred,
                         PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
      break;
    case proto::connections::PayloadStatus::LOCAL_CANCELLATION:
      sendControlMessage(
          finished_endpoint_ids, payload_header,
          num_bytes_successfully_transferred,
          PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
      break;
    case proto::connections::PayloadStatus::ENDPOINT_IO_ERROR:
      // Unregister these endpoints, since we had an IO error on the physical
      // connection.
      for (std::vector<string>::const_iterator it =
               finished_endpoint_ids.begin();
           it != finished_endpoint_ids.end(); it++) {
        endpoint_manager_->discardEndpoint(client_proxy, *it);
      }
      break;
    case proto::connections::PayloadStatus::REMOTE_ERROR:
    case proto::connections::PayloadStatus::REMOTE_CANCELLATION:
      // No special handling needed for these.
      break;
    default:
      // TODO(tracyzhou): Add logging.
      break;
  }
}

template <typename Platform>
void PayloadManager<Platform>::handleFinishedIncomingPayload(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int64_t offset_bytes, proto::connections::PayloadStatus status) {
  sendClientCallbacksForFinishedIncomingPayload(
      client_proxy, endpoint_id, payload_header, offset_bytes, status);

  switch (status) {
    case proto::connections::PayloadStatus::LOCAL_ERROR:
      sendControlMessage(std::vector<string>(1, endpoint_id), payload_header,
                         offset_bytes,
                         PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
      break;
    case proto::connections::PayloadStatus::LOCAL_CANCELLATION:
      sendControlMessage(
          std::vector<string>(1, endpoint_id), payload_header, offset_bytes,
          PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
      break;
    default:
      // TODO(tracyzhou): Add logging.
      break;
  }
}

template <typename Platform>
void PayloadManager<Platform>::handleSuccessfulOutgoingChunk(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int32_t payload_chunk_flags, std::int64_t payload_chunk_offset,
    std::int64_t payload_chunk_body_size) {
  payload_status_update_executor_->execute(MakePtr(
      new payload_manager::HandleSuccessfulOutgoingChunkRunnable<Platform>(
          self_, client_proxy, endpoint_id, payload_header,
          payload_chunk_flags, payload_chunk_offset, payload_chunk_body_size)));
}

template <typename Platform>
void PayloadManager<Platform>::handleSuccessfulIncomingChunk(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    const PayloadTransferFrame::PayloadHeader& payload_header,
    std::int32_t payload_chunk_flags, std::int64_t payload_chunk_offset,
    std::int64_t payload_chunk_body_size) {
  payload_status_update_executor_->execute(MakePtr(
      new payload_manager::HandleSuccessfulIncomingChunkRunnable<Platform>(
          self_, client_proxy, endpoint_id, payload_header,
          payload_chunk_flags, payload_chunk_offset, payload_chunk_body_size)));
}

template <typename Platform>
void PayloadManager<Platform>::processDataPacket(
    Ptr<ClientProxy<Platform> > to_client_proxy, const string& from_endpoint_id,
    const PayloadTransferFrame& payload_transfer_frame) {
  const PayloadTransferFrame::PayloadHeader& payload_header =
      payload_transfer_frame.payload_header();
  const PayloadTransferFrame::PayloadChunk& payload_chunk =
      payload_transfer_frame.payload_chunk();
  // TODO(tracyzhou): Add logging.

  Ptr<typename PayloadManager<Platform>::PendingPayload> pending_payload;
  if (payload_chunk.offset() == 0) {
    pending_payload =
        createIncomingPayload(payload_transfer_frame, from_endpoint_id);
    if (pending_payload.isNull()) {
      // TODO(tracyzhou): Add logging.
      // Send the error to the remote endpoint.
      sendControlMessage(std::vector<string>(1, from_endpoint_id),
                         payload_header, payload_chunk.offset(),
                         PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR);
      return;
    }

    // Also, let the client know of this new incoming payload.
    payload_status_update_executor_->execute(
        MakePtr(new payload_manager::ProcessDataPacketRunnable<Platform>(
            to_client_proxy, from_endpoint_id,
            pending_payload->getInternalPayload()->releasePayload())));
    // TODO(tracyzhou): Add logging.
  } else {
    pending_payload = pending_payloads_->getPayload(payload_header.id());
    if (pending_payload.isNull()) {
      // TODO(tracyzhou): Add logging.
      return;
    }
  }

  if (pending_payload->isLocallyCanceled()) {
    // This incoming payload was canceled by the client. Drop this frame and do
    // all the cleanup. See go/nc-cancel-payload
    handleFinishedIncomingPayload(
        to_client_proxy, from_endpoint_id, payload_header,
        payload_chunk.offset(),
        proto::connections::PayloadStatus::LOCAL_CANCELLATION);
    return;
  }

  // Update the offset for this payload. An endpoint disconnection might occur
  // from another thread and we would need to know the current offset to report
  // back to the client. For the sake of accuracy, we update the pending payload
  // here because it's after all payload terminating events are handled, but
  // right before we actually start attaching the next chunk.
  pending_payload->setOffsetForEndpoint(from_endpoint_id,
                                        payload_chunk.offset());

  Exception::Value attach_next_chunk_exception =
      pending_payload->getInternalPayload()->attachNextChunk(
          MakeConstPtr(new ByteArray(payload_chunk.body().data(),
                                     payload_chunk.body().size())));
  if (Exception::NONE != attach_next_chunk_exception) {
    if (Exception::IO == attach_next_chunk_exception) {
      // TODO(tracyzhou): Add logging.
      handleFinishedIncomingPayload(
          to_client_proxy, from_endpoint_id, payload_header,
          payload_chunk.offset(),
          proto::connections::PayloadStatus::LOCAL_ERROR);
      return;
    }
  }

  handleSuccessfulIncomingChunk(
      to_client_proxy, from_endpoint_id, payload_header, payload_chunk.flags(),
      payload_chunk.offset(), payload_chunk.body().size());
}

template <typename Platform>
void PayloadManager<Platform>::processControlPacket(
    Ptr<ClientProxy<Platform> > to_client_proxy, const string& from_endpoint_id,
    const PayloadTransferFrame& payload_transfer_frame) {
  const PayloadTransferFrame::PayloadHeader& payload_header =
      payload_transfer_frame.payload_header();
  const PayloadTransferFrame::ControlMessage& control_message =
      payload_transfer_frame.control_message();
  Ptr<PayloadManager<Platform>::PendingPayload> pending_payload =
      pending_payloads_->getPayload(payload_header.id());
  if (pending_payload.isNull()) {
    // TODO(tracyzhou): Add logging.
    return;
  }

  switch (control_message.event()) {
    case PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED:
      if (pending_payload->isIncoming()) {
        // No need to mark the pending payload as cancelled, since this is a
        // remote cancellation for an incoming payload -- we handle everything
        // inline here.
        handleFinishedIncomingPayload(
            to_client_proxy, from_endpoint_id, payload_header,
            control_message.offset(),
            controlMessageEventToPayloadStatus(control_message.event()));
      } else {
        // Mark the payload as canceled *for this endpoint*.
        pending_payload->setEndpointStatusFromControlMessage(from_endpoint_id,
                                                             control_message);
      }
      // TODO(tracyzhou): Add logging.
      break;
    case PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR:
      if (pending_payload->isIncoming()) {
        handleFinishedIncomingPayload(
            to_client_proxy, from_endpoint_id, payload_header,
            control_message.offset(),
            controlMessageEventToPayloadStatus(control_message.event()));
      } else {
        pending_payload->setEndpointStatusFromControlMessage(from_endpoint_id,
                                                             control_message);
      }
      break;
    default:
      // TODO(tracyzhou): Add logging.
      break;
  }
}

template <typename Platform>
void PayloadManager<Platform>::notifyClientOfIncomingPayloadTransferUpdate(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    const PayloadTransferUpdate& payload_transfer_update,
    bool done_with_payload) {
  client_proxy->onPayloadTransferUpdate(endpoint_id, payload_transfer_update);
  if (done_with_payload) {
    // We're done with this payload (either received the last chunk, or had a
    // failure), so remove it from the incoming payloads that we're tracking.
    Ptr<typename PayloadManager<Platform>::PendingPayload> pending_payload =
        pending_payloads_->stopTrackingPayload(
            payload_transfer_update.payload_id);
    pending_payload->close();
    pending_payload.destroy();
  }
}

template <typename Platform>
void PayloadManager<Platform>::enqueueOutgoingPayload(
    Ptr<typename Platform::SingleThreadExecutorType> executor,
    Ptr<Runnable> runnable) {
  executor->execute(runnable);
}

///////////////////////////////// EndpointInfo /////////////////////////////////

template <typename Platform>
PayloadManager<Platform>::EndpointInfo::EndpointInfo(string id)
    : id_(id), status_(Status::AVAILABLE), offset_(0) {}

template <typename Platform>
typename PayloadManager<Platform>::EndpointInfo::Status::Value
PayloadManager<Platform>::EndpointInfo::controlMessageEventToEndpointInfoStatus(
    PayloadTransferFrame::ControlMessage::EventType event) {
  switch (event) {
    case PayloadTransferFrame::ControlMessage::PAYLOAD_ERROR:
      return Status::ERROR;
    case PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED:
      return Status::CANCELED;
    default:
      // TODO(tracyzhou): Add logging.
      return Status::UNKNOWN;
  }
}

template <typename Platform>
string PayloadManager<Platform>::EndpointInfo::getId() const {
  return id_;
}

template <typename Platform>
typename PayloadManager<Platform>::EndpointInfo::Status::Value
PayloadManager<Platform>::EndpointInfo::getStatus() const {
  return status_;
}

template <typename Platform>
std::int64_t PayloadManager<Platform>::EndpointInfo::getOffset() const {
  return offset_;
}

template <typename Platform>
void PayloadManager<Platform>::EndpointInfo::setStatus(
    const PayloadTransferFrame::ControlMessage& control_message) {
  status_ = controlMessageEventToEndpointInfoStatus(control_message.event());
}

template <typename Platform>
void PayloadManager<Platform>::EndpointInfo::setOffset(std::int64_t offset) {
  offset_ = offset;
}

//////////////////////////////// PendingPayload ////////////////////////////////

template <typename Platform>
Ptr<typename PayloadManager<Platform>::PendingPayload>
PayloadManager<Platform>::PendingPayload::createIncoming(
    Ptr<InternalPayload> internal_payload, const string& endpoint_id) {
  return MakeRefCountedPtr(new PendingPayload(
      internal_payload, std::vector<string>(1, endpoint_id), true));
}

template <typename Platform>
Ptr<typename PayloadManager<Platform>::PendingPayload>
PayloadManager<Platform>::PendingPayload::createOutgoing(
    Ptr<InternalPayload> internal_payload,
    const std::vector<string>& endpoint_ids) {
  return MakeRefCountedPtr(
      new PendingPayload(internal_payload, endpoint_ids, false));
}

template <typename Platform>
PayloadManager<Platform>::PendingPayload::PendingPayload(
    Ptr<InternalPayload> internal_payload,
    const std::vector<string>& endpoint_ids, bool is_incoming)
    : lock_(Platform::createLock()),
      internal_payload_(internal_payload),
      is_incoming_(is_incoming),
      is_locally_cancelled_(Platform::createAtomicBoolean(false)),
      endpoints_() {
  for (std::vector<string>::const_iterator it = endpoint_ids.begin();
       it != endpoint_ids.end(); it++) {
    endpoints_.insert(std::make_pair(*it, MakePtr(new EndpointInfo(*it))));
  }
}

template <typename Platform>
PayloadManager<Platform>::PendingPayload::~PendingPayload() {
  for (typename EndpointsMap::iterator it = endpoints_.begin();
       it != endpoints_.end(); it++) {
    it->second.destroy();
  }
  endpoints_.clear();
}

template <typename Platform>
std::int64_t PayloadManager<Platform>::PendingPayload::getId() {
  return internal_payload_->getId();
}

template <typename Platform>
Ptr<InternalPayload>
PayloadManager<Platform>::PendingPayload::getInternalPayload() {
  return internal_payload_.get();
}

template <typename Platform>
bool PayloadManager<Platform>::PendingPayload::isLocallyCanceled() {
  return is_locally_cancelled_->get();
}

template <typename Platform>
void PayloadManager<Platform>::PendingPayload::markLocallyCanceled() {
  is_locally_cancelled_->set(true);
}

template <typename Platform>
bool PayloadManager<Platform>::PendingPayload::isIncoming() {
  return is_incoming_;
}

template <typename Platform>
std::vector<Ptr<typename PayloadManager<Platform>::EndpointInfo> >
PayloadManager<Platform>::PendingPayload::getEndpoints() const {
  Synchronized s(lock_.get());

  std::vector<Ptr<typename PayloadManager<Platform>::EndpointInfo> > result;
  for (typename EndpointsMap::const_iterator it = endpoints_.begin();
       it != endpoints_.end(); it++) {
    result.push_back(it->second);
  }
  return result;
}

template <typename Platform>
Ptr<typename PayloadManager<Platform>::EndpointInfo>
PayloadManager<Platform>::PendingPayload::getEndpoint(
    const string& endpoint_id) {
  Synchronized s(lock_.get());

  typename EndpointsMap::iterator it = endpoints_.find(endpoint_id);
  if (it == endpoints_.end()) {
    return Ptr<typename PayloadManager<Platform>::EndpointInfo>();
  }

  return it->second;
}

template <typename Platform>
void PayloadManager<Platform>::PendingPayload::removeEndpoints(
    const std::vector<string>& endpoint_ids_to_remove) {
  Synchronized s(lock_.get());

  for (std::vector<string>::const_iterator it = endpoint_ids_to_remove.begin();
       it != endpoint_ids_to_remove.end(); it++) {
    payload_manager::eraseOwnedPtrFromMap(endpoints_, *it);
  }
}

template <typename Platform>
void PayloadManager<Platform>::PendingPayload::
    setEndpointStatusFromControlMessage(
        const string& endpoint_id,
        const PayloadTransferFrame::ControlMessage& control_message) {
  Synchronized s(lock_.get());

  typename EndpointsMap::iterator it = endpoints_.find(endpoint_id);
  if (it != endpoints_.end()) {
    it->second->setStatus(control_message);
  }
}

template <typename Platform>
void PayloadManager<Platform>::PendingPayload::setOffsetForEndpoint(
    const string& endpoint_id, std::int64_t offset) {
  Synchronized s(lock_.get());

  typename EndpointsMap::iterator it = endpoints_.find(endpoint_id);
  if (it != endpoints_.end()) {
    it->second->setOffset(offset);
  }
}

template <typename Platform>
void PayloadManager<Platform>::PendingPayload::close() {
  internal_payload_->close();
}

/////////////////////////////// PendingPayloads ///////////////////////////////

template <typename Platform>
PayloadManager<Platform>::PendingPayloads::PendingPayloads()
    : lock_(Platform::createLock()), pending_payloads_() {}

template <typename Platform>
PayloadManager<Platform>::PendingPayloads::~PendingPayloads() {
  for (typename PendingPayloadsMap::iterator it = pending_payloads_.begin();
       it != pending_payloads_.end(); it++) {
    it->second.destroy();
  }
  pending_payloads_.clear();
}

template <typename Platform>
void PayloadManager<Platform>::PendingPayloads::startTrackingPayload(
    std::int64_t payload_id,
    Ptr<typename PayloadManager<Platform>::PendingPayload> pending_payload) {
  Synchronized s(lock_.get());

  pending_payloads_.insert(std::make_pair(payload_id, pending_payload));
}

template <typename Platform>
Ptr<typename PayloadManager<Platform>::PendingPayload>
PayloadManager<Platform>::PendingPayloads::stopTrackingPayload(
    std::int64_t payload_id) {
  Synchronized s(lock_.get());

  typename PendingPayloadsMap::iterator it = pending_payloads_.find(payload_id);
  if (it == pending_payloads_.end()) {
    return Ptr<typename PayloadManager<Platform>::PendingPayload>();
  }

  Ptr<typename PayloadManager<Platform>::PendingPayload> pending_payload =
      it->second;
  pending_payloads_.erase(it);

  return pending_payload;
}

template <typename Platform>
Ptr<typename PayloadManager<Platform>::PendingPayload>
PayloadManager<Platform>::PendingPayloads::getPayload(std::int64_t payload_id) {
  Synchronized s(lock_.get());

  typename PendingPayloadsMap::iterator it = pending_payloads_.find(payload_id);
  if (it == pending_payloads_.end()) {
    return Ptr<typename PayloadManager<Platform>::PendingPayload>();
  }
  return it->second;
}

template <typename Platform>
std::vector<Ptr<typename PayloadManager<Platform>::PendingPayload> >
PayloadManager<Platform>::PendingPayloads::getAllPayloads() {
  Synchronized s(lock_.get());

  std::vector<Ptr<typename PayloadManager<Platform>::PendingPayload> > result;
  for (typename PendingPayloadsMap::iterator it = pending_payloads_.begin();
       it != pending_payloads_.end(); it++) {
    result.push_back(it->second);
  }
  return result;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
