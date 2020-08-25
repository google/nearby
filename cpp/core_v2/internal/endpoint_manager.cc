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

#include "core_v2/internal/endpoint_manager.h"

#include <memory>
#include <utility>

#include "core_v2/internal/endpoint_channel.h"
#include "core_v2/internal/offline_frames.h"
#include "platform_v2/base/exception.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/logging.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

using ::location::nearby::proto::connections::Medium;

constexpr absl::Duration EndpointManager::kKeepAliveWriteInterval;
constexpr absl::Duration EndpointManager::kKeepAliveReadTimeout;
constexpr absl::Duration EndpointManager::kProcessEndpointDisconnectionTimeout;
constexpr absl::Time EndpointManager::kInvalidTimestamp;

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
    const std::string& endpoint_id, CountDownLatch* barrier,
    std::function<ExceptionOr<bool>(EndpointChannel*)> handler) {
  // EndpointChannelManager will not let multiple channels exist simultaneously
  // for the same endpoint_id; it will be closing "old" channels as new ones
  // come. (There will be a short overlap).
  // Closed channel will return Exception::kIo for any Read, and loop (below)
  // will retry and attempt to pick another channel.
  // If channel is deleted (no mapping), or it is still the same channel
  // (same Medium) on which we got the Exception::kIo, we terminate the loop.
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
  NEARBY_LOG(INFO, "Worker going down; name=%s; id=%s", runnable_name.c_str(),
             endpoint_id.c_str());
  barrier->CountDown();

  // Always clear out all state related to this endpoint before terminating
  // this thread.
  DiscardEndpoint(client, endpoint_id);
  NEARBY_LOG(INFO, "Worker done; name=%s; id=%s", runnable_name.c_str(),
             endpoint_id.c_str());
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
        NEARBY_LOG(INFO, "Failed to decode; endpoint=%s; channel=%s; skip",
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
    EndpointManager::FrameProcessor* frame_processor =
        GetFrameProcessor(frame_type);
    if (frame_processor == nullptr) {
      // report messages without handlers, except KEEP_ALIVE, which has
      // no explicit handler.
      if (frame_type == V1Frame::KEEP_ALIVE) {
        NEARBY_LOG(INFO, "KeepAlive message for: id=%s", endpoint_id.c_str());
      } else {
        NEARBY_LOG(ERROR, "Unhandled message: id=%s, type=%d",
                   endpoint_id.c_str(), frame_type);
      }
      continue;
    }

    frame_processor->OnIncomingFrame(frame, endpoint_id, client,
                                     endpoint_channel->GetMedium());
  }
}

ExceptionOr<bool> EndpointManager::HandleKeepAlive(
    EndpointChannel* endpoint_channel) {
  // Check if it has been too long since we received a frame from our
  // endpoint.
  auto last_read_time = endpoint_channel->GetLastReadTimestamp();
  if (last_read_time != kInvalidTimestamp &&
      SystemClock::ElapsedRealtime() >
          (last_read_time + EndpointManager::kKeepAliveReadTimeout)) {
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
  Exception sleep_exception =
      SystemClock::Sleep(EndpointManager::kKeepAliveWriteInterval);
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
  NEARBY_LOG(INFO, "EndpointManager going down");
  CountDownLatch latch(1);
  RunOnEndpointManagerThread([this, &latch]() {
    NEARBY_LOG(INFO, "Bringing down endpoints");
    for (auto& item : endpoints_) {
      const std::string& endpoint_id = item.first;
      EndpointState& state = item.second;
      // This will close the channel; all workers will sense that and
      // terminate.
      channel_manager_->UnregisterChannelForEndpoint(endpoint_id);
      state.barrier.Await();
    }
    latch.CountDown();
  });
  latch.Await();
  NEARBY_LOG(INFO, "Bringing down worker threads");

  // Stop all the ongoing Runnables (as gracefully as possible).
  // Order matters: bring worker pools down first; serial_executor_ thread
  // should go last, since workers schedule jobs there even during shutdown.
  handlers_executor_.Shutdown();
  keep_alive_executor_.Shutdown();
  NEARBY_LOG(INFO, "Bringing down control thread");
  serial_executor_.Shutdown();
  NEARBY_LOG(INFO, "EndpointManager is down");
}

EndpointManager::FrameProcessor::Handle EndpointManager::RegisterFrameProcessor(
    V1Frame::FrameType frame_type, EndpointManager::FrameProcessor* processor) {
  const FrameProcessor::Handle handle = processor;
  CountDownLatch latch(1);
  RunOnEndpointManagerThread([this, frame_type, &latch, processor]() {
    auto it = frame_processors_.find(frame_type);
    if (it != frame_processors_.end()) {
      NEARBY_LOGS(INFO) << "Frame processor found: updated; type=" << frame_type
                        << "; processor=" << processor << "; self=" << this;
      it->second = processor;
    } else {
      NEARBY_LOGS(INFO) << "Frame processor added; type=" << frame_type
                        << "; processor=" << processor << "; self=" << this;
      frame_processors_.emplace(frame_type, processor);
    }
    latch.CountDown();
  });
  latch.Await();
  return handle;
}

void EndpointManager::UnregisterFrameProcessor(V1Frame::FrameType frame_type,
                                               const void* handle, bool sync) {
  NEARBY_LOGS(INFO) << "UnregisterFrameProcessor [enter]: handle=" << handle;
  if (handle == nullptr) return;
  CountDownLatch latch(1);
  RunOnEndpointManagerThread([this, frame_type, handle, &latch, sync]() {
    auto it = frame_processors_.find(frame_type);
    if (it == frame_processors_.end()) {
      NEARBY_LOGS(INFO) << "UnregisterFrameProcessor [not found]: handle="
                        << handle;
      if (sync) latch.CountDown();
      return;
    }
    NEARBY_LOGS(INFO) << "UnregisterFrameProcessor [found]: handle=" << handle;
    if (it->second == handle) {
      frame_processors_.erase(it);
      NEARBY_LOGS(INFO) << "Unregistered: type=" << frame_type
                        << "; processor=" << handle << "; self=" << this;
    } else {
      NEARBY_LOG(INFO,
                 "Failed to unregister: type=%d; handle mismatch: passed=%p, "
                 "expected=%p",
                 frame_type, handle, it->second);
    }
    if (sync) latch.CountDown();
  });
  if (sync) {
    latch.Await();
    NEARBY_LOGS(INFO) << "Unregistered [sync done]: type=" << frame_type
                      << "; processor=" << handle << "; self=" << this;
  }
}

EndpointManager::FrameProcessor* EndpointManager::GetFrameProcessor(
    V1Frame::FrameType frame_type) {
  EndpointManager::FrameProcessor* processor = nullptr;
  CountDownLatch latch(1);
  RunOnEndpointManagerThread([this, frame_type, &processor, &latch]() {
    auto it = frame_processors_.find(frame_type);
    if (it != frame_processors_.end()) {
      processor = it->second;
    }
    latch.CountDown();
  });
  latch.Await();
  NEARBY_LOG(INFO, "GetFrameProcessor: type=%d; processor=%p", frame_type,
             processor);
  return processor;
}

void EndpointManager::EnsureWorkersTerminated(const std::string& endpoint_id) {
  auto item = endpoints_.find(endpoint_id);
  if (item != endpoints_.end()) {
    // If another instance of data and keep-alive handlers is running, it will
    // terminate soon; we should block until it happens.
    EndpointState& endpoint_state = item->second;
    NEARBY_LOGS(INFO) << "Waiting for workers to terminate for id: "
                      << endpoint_id;
    endpoint_state.barrier.Await();
    endpoints_.erase(item);
    NEARBY_LOGS(INFO) << "Workers terminated for id: " << endpoint_id;
  } else {
    NEARBY_LOGS(INFO) << "EndpointState not found for id: " << endpoint_id;
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
  RunOnEndpointManagerThread([this, client, channel = channel.release(),
                              &endpoint_id, &info, &options, &listener,
                              &latch]() {
    // Pass ownership of channel to EndpointChannelManager
    NEARBY_LOG(INFO, "Registering endpoint with channel manager: id=%s",
               endpoint_id.c_str());
    channel_manager_->RegisterChannelForEndpoint(
        client, endpoint_id, std::unique_ptr<EndpointChannel>(channel));

    EnsureWorkersTerminated(endpoint_id);
    EndpointState& endpoint_state =
        endpoints_.emplace(endpoint_id, EndpointState()).first->second;
    endpoint_state.client = client;

    NEARBY_LOG(INFO, "Starting workers: id=%s", endpoint_id.c_str());
    // For every endpoint, there's normally only one Read handler instance
    // running on the handlers_executor_ pool. This instance reads data from the
    // endpoint and delegates incoming frames to various FrameProcessors.
    // Once the frame has been properly handled, it starts reading again for
    // the next frame. If the handler fails its read and no other
    // EndpointChannels are available for this endpoint, a disconnection
    // will be initiated.
    StartEndpointReader(
        [this, client, endpoint_id, barrier = &endpoint_state.barrier]() {
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
    // within kKeepAliveReadTimeoutMillis milliseconds, it initiates a
    // disconnection.
    //
    // (*) Bluetooth requires a constant outgoing stream of messages. If
    // there's silence, Android will break the socket. This is why we ping.
    // (**) Wifi Hotspots can fail to notice a connection has been lost, and
    // they will happily keep writing to /dev/null. This is why we listen
    // for the pong.
    StartEndpointKeepAliveManager([this, client, endpoint_id,
                                   barrier = &endpoint_state.barrier]() {
      EndpointChannelLoopRunnable("KeepAliveManager", client, endpoint_id,
                                  barrier, [this](EndpointChannel* channel) {
                                    return HandleKeepAlive(channel);
                                  });
    });
    NEARBY_LOG(INFO, "Workers started, notifying client; id=%s",
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
  CountDownLatch latch(1);
  RunOnEndpointManagerThread([this, client, endpoint_id, &latch]() {
    RemoveEndpoint(client, endpoint_id,
                   client->IsConnectedToEndpoint(endpoint_id));
    latch.CountDown();
  });
  latch.Await();
}

// Designed to run asynchronously. It is called from IO thread pools, and
// jobs in these pools may be waited for from the EndpointManager thread. If we
// allow synchronous behavior here it will cause a live lock.
void EndpointManager::DiscardEndpoint(ClientProxy* client,
                                      const std::string& endpoint_id) {
  RunOnEndpointManagerThread([this, client, endpoint_id]() {
    RemoveEndpoint(client, endpoint_id,
                   /*notify=*/
                   client->IsConnectedToEndpoint(endpoint_id));
  });
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
    NEARBY_LOG(INFO, "Removed endpoint; id=%s", endpoint_id.c_str());
  }
}

// @EndpointManagerThread
void EndpointManager::WaitForEndpointDisconnectionProcessing(
    ClientProxy* client, const std::string& endpoint_id) {
  NEARBY_LOGS(INFO) << "Wait: client=" << client << "; id=" << endpoint_id;
  auto total_size = frame_processors_.size();
  NEARBY_LOGS(INFO) << "Total frame processors: " << total_size;
  if (!total_size) return;
  CountDownLatch barrier(total_size);

  int valid = 0;
  for (auto& item : frame_processors_) {
    auto* processor = item.second;
    NEARBY_LOGS(INFO) << "processor=" << processor << "; type=" << item.first;
    if (processor) {
      valid++;
      processor->OnEndpointDisconnect(client, endpoint_id, &barrier);
    } else {
      barrier.CountDown();
    }
  }

  if (!valid) {
    NEARBY_LOGS(INFO) << "No valid frame processors.";
    return;
  } else {
    NEARBY_LOGS(INFO) << "Valid frame processors: " << valid;
  }

  NEARBY_LOGS(INFO) << "Waiting for " << valid
                    << " frame processors to disconnect from: " << endpoint_id;
  if (!barrier.Await(kProcessEndpointDisconnectionTimeout).result()) {
    NEARBY_LOGS(INFO) << "Failed to disconnect frame processors from: "
                      << endpoint_id;
  } else {
    NEARBY_LOGS(INFO)
        << "Finished waiting for frame processors to disconnect from: "
        << endpoint_id;
  }
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
      NEARBY_LOG(INFO, "Channel not available; id=%s", endpoint_id.c_str());
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
  handlers_executor_.Execute(std::move(runnable));
}

void EndpointManager::StartEndpointKeepAliveManager(Runnable runnable) {
  keep_alive_executor_.Execute(std::move(runnable));
}

void EndpointManager::RunOnEndpointManagerThread(Runnable runnable) {
  serial_executor_.Execute(std::move(runnable));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
