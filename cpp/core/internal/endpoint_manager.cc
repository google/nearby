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

#include "core/internal/endpoint_manager.h"

#include <cinttypes>
#include <memory>
#include <utility>

#include "core/internal/endpoint_channel.h"
#include "core/internal/offline_frames.h"
#include "platform/base/exception.h"
#include "platform/public/count_down_latch.h"
#include "platform/public/logging.h"
#include "platform/public/mutex_lock.h"

namespace location {
namespace nearby {
namespace connections {

using ::location::nearby::proto::connections::Medium;

constexpr absl::Duration EndpointManager::kProcessEndpointDisconnectionTimeout;
constexpr absl::Time EndpointManager::kInvalidTimestamp;

class EndpointManager::LockedFrameProcessor {
 public:
  explicit LockedFrameProcessor(FrameProcessorWithMutex* fp)
      : lock_{std::make_unique<MutexLock>(&fp->mutex_)},
        frame_processor_with_mutex_{fp} {}

  // Constructor of a no-op object.
  LockedFrameProcessor() {}

  explicit operator bool() const { return get() != nullptr; }

  FrameProcessor* operator->() const { return get(); }

  void set(FrameProcessor* frame_processor) {
    if (frame_processor_with_mutex_)
      frame_processor_with_mutex_->frame_processor_ = frame_processor;
  }

  FrameProcessor* get() const {
    return frame_processor_with_mutex_
               ? frame_processor_with_mutex_->frame_processor_
               : nullptr;
  }

  void reset() {
    if (frame_processor_with_mutex_)
      frame_processor_with_mutex_->frame_processor_ = nullptr;
  }

 private:
  std::unique_ptr<MutexLock> lock_;
  FrameProcessorWithMutex* frame_processor_with_mutex_ = nullptr;
};

// A Runnable that continuously grabs the most recent EndpointChannel available
// for an endpoint.
//
// handler - Called whenever an EndpointChannel is available for endpointId.
//           Implementations are expected to read/write freely to the
//           EndpointChannel until an Exception::IO is thrown. Once an
//           Exception::IO occurs, a check will be performed to see if another
//           EndpointChannel is available for the given endpoint and, if so,
//           handler(EndpointChannel) will be called again. Return false to exit
//           the loop.
void EndpointManager::EndpointChannelLoopRunnable(
    const std::string& runnable_name, ClientProxy* client,
    const std::string& endpoint_id, std::weak_ptr<CountDownLatch> barrier,
    std::function<ExceptionOr<bool>(EndpointChannel*)> handler) {
  // EndpointChannelManager will not let multiple channels exist simultaneously
  // for the same endpoint_id; it will be closing "old" channels as new ones
  // come. (There will be a short overlap).
  // Closed channel will return Exception::kIo for any Read, and loop (below)
  // will retry and attempt to pick another channel.
  // If channel is deleted (no mapping), or it is still the same channel
  // (same Medium) on which we got the Exception::kIo, we terminate the loop.
  NEARBY_LOG(INFO, "Started worker loop name=%s, endpoint=%s",
             runnable_name.c_str(), endpoint_id.c_str());
  Medium last_failed_medium = Medium::UNKNOWN_MEDIUM;
  while (true) {
    // It's important to keep re-fetching the EndpointChannel for an endpoint
    // because it can be changed out from under us (for example, when we
    // upgrade from Bluetooth to Wifi).
    std::shared_ptr<EndpointChannel> channel =
        channel_manager_->GetChannelForEndpoint(endpoint_id);
    if (channel == nullptr) {
      NEARBY_LOG(INFO, "Endpoint channel is nullptr, bail out.");
      break;
    }

    // If we're looping back around after a failure, and there's not a new
    // EndpointChannel for this endpoint, there's nothing more to do here.
    if ((last_failed_medium != Medium::UNKNOWN_MEDIUM) &&
        (channel->GetMedium() == last_failed_medium)) {
      NEARBY_LOG(
          INFO, "No new endpoint channel is found after a failure, exit loop.");
      break;
    }

    ExceptionOr<bool> keep_using_channel = handler(channel.get());

    if (!keep_using_channel.ok()) {
      Exception exception = keep_using_channel.GetException();
      // An "invalid proto" may be a final payload on a channel we're about to
      // close, so we'll loop back around once. We set |last_failed_medium| to
      // ensure we don't loop indefinitely. See crbug.com/1182031 for more
      // detail.
      if (exception.Raised(Exception::kInvalidProtocolBuffer)) {
        last_failed_medium = channel->GetMedium();
        NEARBY_LOG(INFO,
                   "Received invalid protobuf message, re-fetching endpoint "
                   "channel; last_failed_medium=%d",
                   last_failed_medium);
        continue;
      }
      if (exception.Raised(Exception::kIo)) {
        last_failed_medium = channel->GetMedium();
        NEARBY_LOG(INFO, "Endpoint channel IO exception; last_failed_medium=%d",
                   last_failed_medium);
        continue;
      }
      if (exception.Raised(Exception::kInterrupted)) {
        break;
      }
    }

    if (!keep_using_channel.result()) {
      NEARBY_LOG(INFO, "Dropping current channel: last medium=%d",
                 last_failed_medium);
      break;
    }
  }
  // Indicate we're out of the loop and it is ok to schedule another instance
  // if needed.
  NEARBY_LOG(INFO, "Worker going down; worker name=%s; endpoint_id=%s",
             runnable_name.c_str(), endpoint_id.c_str());
  if (auto latch = barrier.lock()) {
    latch->CountDown();
  } else {
    NEARBY_LOG(WARNING,
               "Barrier already expired in worker name=%s, for endpoint %s",
               runnable_name.c_str(), endpoint_id.c_str());
  }

  // Always clear out all state related to this endpoint before terminating
  // this thread.
  DiscardEndpoint(client, endpoint_id);
  NEARBY_LOG(INFO, "Worker done; worker name=%s; endpoint_id=%s",
             runnable_name.c_str(), endpoint_id.c_str());
}

ExceptionOr<bool> EndpointManager::HandleData(
    const std::string& endpoint_id, ClientProxy* client,
    EndpointChannel* endpoint_channel) {
  // Read as much as we can from the healthy EndpointChannel - when it is no
  // longer in good shape (i.e. our read from it throws an Exception), our
  // super class will loop back around and try our luck in case there's been
  // a replacement for this endpoint since we last checked with the
  // EndpointChannelManager.
  while (true) {
    ExceptionOr<ByteArray> bytes = endpoint_channel->Read();
    if (!bytes.ok()) {
      NEARBY_LOG(INFO, "Stop reading on read-time exception: %d",
                 bytes.exception());
      return ExceptionOr<bool>(bytes.exception());
    }
    ExceptionOr<OfflineFrame> wrapped_frame = parser::FromBytes(bytes.result());
    if (!wrapped_frame.ok()) {
      if (wrapped_frame.GetException().Raised(
              Exception::kInvalidProtocolBuffer)) {
        NEARBY_LOG(INFO, "Failed to decode; endpoint_id=%s; channel=%s; skip",
                   endpoint_id.c_str(), endpoint_channel->GetType().c_str());
        continue;
      } else {
        NEARBY_LOG(INFO, "Stop reading on parse-time exception: %d",
                   wrapped_frame.exception());
        return ExceptionOr<bool>(wrapped_frame.exception());
      }
    }
    OfflineFrame& frame = wrapped_frame.result();

    // Route the incoming offlineFrame to its registered processor.
    V1Frame::FrameType frame_type = parser::GetFrameType(frame);
    LockedFrameProcessor frame_processor = GetFrameProcessor(frame_type);
    if (!frame_processor) {
      // report messages without handlers, except KEEP_ALIVE, which has
      // no explicit handler.
      if (frame_type == V1Frame::KEEP_ALIVE) {
        NEARBY_LOG(INFO, "KeepAlive message for endpoint %s",
                   endpoint_id.c_str());
      } else if (frame_type == V1Frame::DISCONNECTION) {
        NEARBY_LOG(INFO, "Disconnect message for endpoint %s",
                   endpoint_id.c_str());
        endpoint_channel->Close();
      } else {
        NEARBY_LOG(ERROR, "Unhandled message: endpoint_id=%s, frame_type=%d",
                   endpoint_id.c_str(), static_cast<int>(frame_type));
      }
      continue;
    }

    frame_processor->OnIncomingFrame(frame, endpoint_id, client,
                                     endpoint_channel->GetMedium());
  }
}

ExceptionOr<bool> EndpointManager::HandleKeepAlive(
    EndpointChannel* endpoint_channel, absl::Duration keep_alive_interval,
    absl::Duration keep_alive_timeout) {
  // Check if it has been too long since we received a frame from our
  // endpoint.
  auto last_read_time = endpoint_channel->GetLastReadTimestamp();
  if (last_read_time != kInvalidTimestamp &&
      SystemClock::ElapsedRealtime() > (last_read_time + keep_alive_timeout)) {
    NEARBY_LOG(INFO, "Receive timeout expired; aborting KeepAlive worker.");
    return ExceptionOr<bool>(false);
  }

  // Attempt to send the KeepAlive frame over the endpoint channel - if the
  // write fails, our super class will loop back around and try our luck again
  // in case there's been a replacement for this endpoint.
  Exception write_exception = endpoint_channel->Write(parser::ForKeepAlive());
  if (!write_exception.Ok()) {
    return ExceptionOr<bool>(write_exception);
  }

  // We sleep as the very last step because we want to minimize the caching of
  // the EndpointChannel. If we do hold on to the EndpointChannel, and it's
  // switched out from under us in BandwidthUpgradeManager, our write will
  // trigger an erroneous write to the encryption context that will cascade
  // into all our remote endpoint's future reads failing.
  Exception sleep_exception = SystemClock::Sleep(keep_alive_interval);
  if (!sleep_exception.Ok()) {
    return ExceptionOr<bool>(sleep_exception);
  }

  return ExceptionOr<bool>(true);
}

bool operator==(const EndpointManager::FrameProcessor& lhs,
                const EndpointManager::FrameProcessor& rhs) {
  // We're comparing addresses because these objects are callbacks which need to
  // be matched by exact instances.
  return &lhs == &rhs;
}

bool operator<(const EndpointManager::FrameProcessor& lhs,
               const EndpointManager::FrameProcessor& rhs) {
  // We're comparing addresses because these objects are callbacks which need to
  // be matched by exact instances.
  return &lhs < &rhs;
}

EndpointManager::EndpointManager(EndpointChannelManager* manager)
    : channel_manager_(manager) {}

EndpointManager::~EndpointManager() {
  NEARBY_LOG(INFO, "Initiating shutdown of EndpointManager.");
  CountDownLatch latch(1);
  RunOnEndpointManagerThread("bring-down-endpoints", [this, &latch]() {
    NEARBY_LOG(INFO, "Bringing down endpoints.");
    for (auto& item : endpoints_) {
      const std::string& endpoint_id = item.first;
      EndpointState& state = item.second;
      // This will close the channel; all workers will sense that and
      // terminate.
      channel_manager_->UnregisterChannelForEndpoint(endpoint_id);
      if (state.barrier) {
        state.barrier->Await();
      } else {
        NEARBY_LOG(WARNING,
                   "State barrier already freed before EM destructor for "
                   "endpoint(id=%s).",
                   endpoint_id.c_str());
      }
    }
    latch.CountDown();
  });
  latch.Await();
  NEARBY_LOG(INFO, "Bringing down worker threads.");

  // Stop all the ongoing Runnables (as gracefully as possible).
  // Order matters: bring worker pools down first; serial_executor_ thread
  // should go last, since workers schedule jobs there even during shutdown.
  handlers_executor_.Shutdown();
  keep_alive_executor_.Shutdown();
  NEARBY_LOG(INFO, "Bringing down control thread.");
  serial_executor_.Shutdown();
  NEARBY_LOG(INFO, "EndpointManager has shut down.");
}

void EndpointManager::RegisterFrameProcessor(
    V1Frame::FrameType frame_type, EndpointManager::FrameProcessor* processor) {
  if (auto frame_processor = GetFrameProcessor(frame_type)) {
    NEARBY_LOG(INFO,
               "EndpointManager received request to update registration of "
               "frame processor %p for frame type %d, self=%p",
               processor, static_cast<int>(frame_type), this);
    frame_processor.set(processor);
  } else {
    MutexLock lock(&frame_processors_lock_);
    NEARBY_LOG(INFO,
               "EndpointManager received request to add registration of "
               "frame processor %p for frame type %d, self=%p",
               processor, static_cast<int>(frame_type), this);
    frame_processors_.emplace(frame_type, processor);
  }
}

void EndpointManager::UnregisterFrameProcessor(
    V1Frame::FrameType frame_type,
    const EndpointManager::FrameProcessor* processor) {
  NEARBY_LOG(INFO, "UnregisterFrameProcessor [enter]: processor=%p", processor);
  if (processor == nullptr) return;
  if (auto frame_processor = GetFrameProcessor(frame_type)) {
    if (frame_processor.get() == processor) {
      frame_processor.reset();
      NEARBY_LOG(INFO,
                 "EndpointManager unregister frame processor %p for frame type "
                 "%d, self=%p",
                 processor, static_cast<int>(frame_type), this);
    } else {
      NEARBY_LOG(INFO,
                 "EndpointManager cannot unregister frame processor %p because "
                 " it is not registered for frame type %d, expected=%p",
                 processor, static_cast<int>(frame_type),
                 frame_processor.get());
    }
  } else {
    NEARBY_LOG(INFO, "UnregisterFrameProcessor [not found]: processor=%p",
               processor);
  }
}

EndpointManager::LockedFrameProcessor EndpointManager::GetFrameProcessor(
    V1Frame::FrameType frame_type) {
  MutexLock lock(&frame_processors_lock_);
  auto it = frame_processors_.find(frame_type);
  if (it != frame_processors_.end()) {
    return LockedFrameProcessor(&it->second);
  }
  return LockedFrameProcessor();
}

void EndpointManager::EnsureWorkersTerminated(const std::string& endpoint_id) {
  NEARBY_LOG(ERROR, "EnsureWorkersTerminated for endpoint_id=%s",
             endpoint_id.c_str());
  auto item = endpoints_.find(endpoint_id);
  if (item != endpoints_.end()) {
    NEARBY_LOG(INFO, "EndpointState found for endpoint_id=%s",
               endpoint_id.c_str());
    // If another instance of data and keep-alive handlers is running, it will
    // terminate soon; we should block until it happens.
    EndpointState& endpoint_state = item->second;
    NEARBY_LOG(INFO, "Waiting for workers to terminate for endpoint_id=%s",
               endpoint_id.c_str());
    if (endpoint_state.barrier) {
      endpoint_state.barrier->Await();
    } else {
      NEARBY_LOG(
          WARNING,
          "State barrier already freed before EnsureWorkersTerminated for "
          "endpoint(id=%s).",
          endpoint_id.c_str());
    }
    endpoints_.erase(item);
    NEARBY_LOG(INFO, "Workers terminated for endpoint_id=%s",
               endpoint_id.c_str());
  } else {
    NEARBY_LOG(INFO, "EndpointState not found for endpoint_id=%s",
               endpoint_id.c_str());
  }
}

void EndpointManager::RegisterEndpoint(ClientProxy* client,
                                       const std::string& endpoint_id,
                                       const ConnectionResponseInfo& info,
                                       const ConnectionOptions& options,
                                       std::unique_ptr<EndpointChannel> channel,
                                       const ConnectionListener& listener) {
  CountDownLatch latch(1);

  // NOTE (unique_ptr<> capture):
  // std::unique_ptr<> is not copyable, so we can not pass it to
  // lambda capture, because lambda eventually is converted to std::function<>.
  // Instead, we release() a pointer, and pass a raw pointer, which is copyalbe.
  // We ignore the risk of job not scheduled (and an associated risk of memory
  // leak), because this may only happen during service shutdown.
  RunOnEndpointManagerThread("register-endpoint", [this, client,
                                                   channel = channel.release(),
                                                   &endpoint_id, &info,
                                                   &options, &listener,
                                                   &latch]() {
    if (endpoints_.contains(endpoint_id)) {
      NEARBY_LOG(WARNING, "Registing duplicate endpoint(id=%s)",
                 endpoint_id.c_str());
      if (!FeatureFlags::GetInstance()
               .GetFlags()
               .endpoint_manager_ensure_workers_terminated_inside_remove) {
        EnsureWorkersTerminated(endpoint_id);
      }
    }

    absl::Duration keep_alive_interval =
        absl::Milliseconds(options.keep_alive_interval_millis);
    absl::Duration keep_alive_timeout =
        absl::Milliseconds(options.keep_alive_timeout_millis);
    NEARBY_LOG(INFO,
               "Registering endpoint %s for client %" PRIx64
               " with keep-alive frame as interval=%s, timeout=%s",
               endpoint_id.c_str(), client->GetClientId(),
               absl::FormatDuration(keep_alive_interval).c_str(),
               absl::FormatDuration(keep_alive_timeout).c_str());

    // Pass ownership of channel to EndpointChannelManager
    NEARBY_LOG(INFO,
               "Registering endpoint with channel manager: endpoint_id=%s",
               endpoint_id.c_str());
    channel_manager_->RegisterChannelForEndpoint(
        client, endpoint_id, std::unique_ptr<EndpointChannel>(channel));

    EndpointState& endpoint_state =
        endpoints_.emplace(endpoint_id, EndpointState()).first->second;
    endpoint_state.client = client;

    NEARBY_LOG(INFO,
               "Registering endpoint %s and starting workers.",
               endpoint_id.c_str());
    // For every endpoint, there's normally only one Read handler instance
    // running on the handlers_executor_ pool. This instance reads data from the
    // endpoint and delegates incoming frames to various FrameProcessors.
    // Once the frame has been properly handled, it starts reading again for
    // the next frame. If the handler fails its read and no other
    // EndpointChannels are available for this endpoint, a disconnection
    // will be initiated.
    //
    // Using weak_ptr just in case the barrier is freed, to save the UAF crash
    // in b/179800119.
    StartEndpointReader(
        [this, client, endpoint_id,
         barrier = std::weak_ptr<CountDownLatch>(endpoint_state.barrier)]() {
          EndpointChannelLoopRunnable(
              "Read", client, endpoint_id, barrier,
              [this, client, endpoint_id](EndpointChannel* channel) {
                return HandleData(endpoint_id, client, channel);
              });
        });

    // For every endpoint, there's only one KeepAliveManager instance
    // running on the keep_alive_executor_ pool. This instance will
    // periodically send out a ping* to the endpoint while listening for an
    // incoming pong**. If it fails to send the ping, or if no pong is heard
    // within keep_alive_interval_, it initiates a disconnection.
    //
    // (*) Bluetooth requires a constant outgoing stream of messages. If
    // there's silence, Android will break the socket. This is why we ping.
    // (**) Wifi Hotspots can fail to notice a connection has been lost, and
    // they will happily keep writing to /dev/null. This is why we listen
    // for the pong.
    //
    // Using weak_ptr just in case the barrier is freed, to save the UAF crash
    // in b/179800119.
    NEARBY_LOG(VERBOSE, "EndpointManager enabling KeepAlive for endpoint %s",
               endpoint_id.c_str());
    StartEndpointKeepAliveManager(
        [this, client, endpoint_id, keep_alive_interval, keep_alive_timeout,
         barrier = std::weak_ptr<CountDownLatch>(endpoint_state.barrier)]() {
          EndpointChannelLoopRunnable(
              "KeepAliveManager", client, endpoint_id, barrier,
              [this, keep_alive_interval,
               keep_alive_timeout](EndpointChannel* channel) {
                return HandleKeepAlive(channel, keep_alive_interval,
                                       keep_alive_timeout);
              });
        });
    NEARBY_LOG(INFO,
               "Registering endpoint %s, workers started and notifying client.",
               endpoint_id.c_str());

    // It's now time to let the client know of this new connection so that
    // they can accept or reject it.
    client->OnConnectionInitiated(endpoint_id, info, options, listener);
    latch.CountDown();
  });
  latch.Await();
}

void EndpointManager::UnregisterEndpoint(ClientProxy* client,
                                         const std::string& endpoint_id) {
  NEARBY_LOG(ERROR, "Unregister endpoint for endpoint %s", endpoint_id.c_str());
  CountDownLatch latch(1);
  RunOnEndpointManagerThread(
      "unregister-endpoint", [this, client, endpoint_id, &latch]() {
        RemoveEndpoint(client, endpoint_id,
                       client->IsConnectedToEndpoint(endpoint_id));
        latch.CountDown();
      });
  latch.Await();
}

int EndpointManager::GetMaxTransmitPacketSize(const std::string& endpoint_id) {
  std::shared_ptr<EndpointChannel> channel =
      channel_manager_->GetChannelForEndpoint(endpoint_id);
  if (channel == nullptr) {
    return 0;
  }

  return channel->GetMaxTransmitPacketSize();
}

std::vector<std::string> EndpointManager::SendPayloadChunk(
    const PayloadTransferFrame::PayloadHeader& payload_header,
    const PayloadTransferFrame::PayloadChunk& payload_chunk,
    const std::vector<std::string>& endpoint_ids) {
  ByteArray bytes =
      parser::ForDataPayloadTransfer(payload_header, payload_chunk);

  return SendTransferFrameBytes(endpoint_ids, bytes, payload_header.id(),
                                /*offset=*/payload_chunk.offset(),
                                /*packet_type=*/"DATA");
}

// Designed to run asynchronously. It is called from IO thread pools, and
// jobs in these pools may be waited for from the EndpointManager thread. If we
// allow synchronous behavior here it will cause a live lock.
void EndpointManager::DiscardEndpoint(ClientProxy* client,
                                      const std::string& endpoint_id) {
  NEARBY_LOG(ERROR, "Discard endpoint for endpoint %s", endpoint_id.c_str());
  RunOnEndpointManagerThread("discard-endpoint", [this, client, endpoint_id]() {
    RemoveEndpoint(client, endpoint_id,
                   /*notify=*/
                   client->IsConnectedToEndpoint(endpoint_id));
  });
}

std::vector<std::string> EndpointManager::SendControlMessage(
    const PayloadTransferFrame::PayloadHeader& header,
    const PayloadTransferFrame::ControlMessage& control,
    const std::vector<std::string>& endpoint_ids) {
  ByteArray bytes = parser::ForControlPayloadTransfer(header, control);

  return SendTransferFrameBytes(endpoint_ids, bytes, header.id(),
                                /*offset=*/control.offset(),
                                /*packet_type=*/"CONTROL");
}

// @EndpointManagerThread
void EndpointManager::RemoveEndpoint(ClientProxy* client,
                                     const std::string& endpoint_id,
                                     bool notify) {
  NEARBY_LOG(ERROR, "Remove endpoint for endpoint %s", endpoint_id.c_str());
  // Unregistering from channel_manager_ will also serve to terminate
  // the dedicated handler and KeepAlive threads we started when we registered
  // this endpoint.
  if (channel_manager_->UnregisterChannelForEndpoint(endpoint_id)) {
    // Notify all frame processors of the disconnection immediately and wait
    // for them to clean up state. Only once all processors are done cleaning
    // up, we can remove the endpoint from ClientProxy after which there
    // should be no further interactions with the endpoint.
    // (See b/37352254 for history)
    WaitForEndpointDisconnectionProcessing(client, endpoint_id);

    client->OnDisconnected(endpoint_id, notify);
    NEARBY_LOG(INFO, "Removed endpoint for endpoint %s", endpoint_id.c_str());
  }
  if (FeatureFlags::GetInstance()
          .GetFlags()
          .endpoint_manager_ensure_workers_terminated_inside_remove) {
    EnsureWorkersTerminated(endpoint_id);
  }
}

// @EndpointManagerThread
void EndpointManager::WaitForEndpointDisconnectionProcessing(
    ClientProxy* client, const std::string& endpoint_id) {
  NEARBY_LOG(INFO, "Wait: client=%" PRIx64 "; endpoint_id=%s",
             client->GetClientId(), endpoint_id.c_str());
  CountDownLatch barrier =
      NotifyFrameProcessorsOnEndpointDisconnect(client, endpoint_id);

  NEARBY_LOG(INFO,
             "Waiting for frame processors to disconnect from: endpoint_id=%s",
             endpoint_id.c_str());
  if (!barrier.Await(kProcessEndpointDisconnectionTimeout).result()) {
    NEARBY_LOG(INFO,
               "Failed to disconnect frame processors from: endpoint_id=%s",
               endpoint_id.c_str());
  } else {
    NEARBY_LOG(INFO,
               "Finished waiting for frame processors to disconnect from: "
               "endpoint_id=%s",
               endpoint_id.c_str());
  }
}

CountDownLatch EndpointManager::NotifyFrameProcessorsOnEndpointDisconnect(
    ClientProxy* client, const std::string& endpoint_id) {
  NEARBY_LOG(INFO,
             "NotifyFrameProcessorsOnEndpointDisconnect: client=%" PRIx64
             "; endpoint_id=%s",
             client->GetClientId(), endpoint_id.c_str());
  MutexLock lock(&frame_processors_lock_);
  auto total_size = frame_processors_.size();
  NEARBY_LOG(INFO, "Total frame processors: %" PRId64, total_size);
  CountDownLatch barrier(total_size);

  int valid = 0;
  for (auto& item : frame_processors_) {
    LockedFrameProcessor processor(&item.second);
    NEARBY_LOG(INFO, "processor=%p; frame_type=%d", processor.get(),
               static_cast<int>(item.first));
    if (processor) {
      valid++;
      processor->OnEndpointDisconnect(client, endpoint_id, barrier);
    } else {
      barrier.CountDown();
    }
  }

  if (!valid) {
    NEARBY_LOG(INFO, "No valid frame processors.");
  } else {
    NEARBY_LOG(INFO, "Valid frame processors: %d", valid);
  }
  return barrier;
}

std::vector<std::string> EndpointManager::SendTransferFrameBytes(
    const std::vector<std::string>& endpoint_ids, const ByteArray& bytes,
    std::int64_t payload_id, std::int64_t offset,
    const std::string& packet_type) {
  std::vector<std::string> failed_endpoint_ids;
  for (const std::string& endpoint_id : endpoint_ids) {
    std::shared_ptr<EndpointChannel> channel =
        channel_manager_->GetChannelForEndpoint(endpoint_id);

    if (channel == nullptr) {
      // We no longer know about this endpoint (it was either explicitly
      // unregistered, or a read/write error made us unregister it internally).
      NEARBY_LOG(ERROR,
                 "EndpointManager failed to find EndpointChannel over which to "
                 "write %s at offset %" PRId64 " of Payload %" PRIx64
                 " to endpoint %s",
                 packet_type.c_str(), offset, payload_id, endpoint_id.c_str());
      failed_endpoint_ids.push_back(endpoint_id);
      continue;
    }

    Exception write_exception = channel->Write(bytes);
    if (!write_exception.Ok()) {
      failed_endpoint_ids.push_back(endpoint_id);
      NEARBY_LOG(INFO, "Failed to send packet; endpoint_id=%s",
                 endpoint_id.c_str());
      continue;
    }
  }

  return failed_endpoint_ids;
}

void EndpointManager::StartEndpointReader(Runnable runnable) {
  handlers_executor_.Execute("reader", std::move(runnable));
}

void EndpointManager::StartEndpointKeepAliveManager(Runnable runnable) {
  keep_alive_executor_.Execute("keep-alive", std::move(runnable));
}

void EndpointManager::RunOnEndpointManagerThread(const std::string& name,
                                                 Runnable runnable) {
  serial_executor_.Execute(name, std::move(runnable));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
