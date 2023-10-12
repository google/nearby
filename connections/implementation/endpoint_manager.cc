// Copyright 2023 Google LLC
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

#include "connections/implementation/endpoint_manager.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "connections/implementation/analytics/throughput_recorder.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/payload_manager.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/implementation/service_id_constants.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/proto/analytics/connections_log.pb.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::analytics::proto::ConnectionsLog;
using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::V1Frame;
using ::nearby::analytics::PacketMetaData;
using DisconnectionReason =
    ::location::nearby::proto::connections::DisconnectionReason;

// We set this to 11s to provide sufficient time for an in-progress WebRTC
// bandwidth upgrade to resolve. This is chosen to be slightly longer than the
// 10s timeout in WebRtc::AttemptToConnect().
constexpr absl::Duration kProcessEndpointDisconnectionTimeout =
    absl::Seconds(11);
constexpr absl::Time kInvalidTimestamp = absl::InfinitePast();
// The maximum time we will wait for the encryption setup during negotiating a
// connection.
constexpr absl::Duration kDecryptRetryTimeout = absl::Seconds(3);
}  // namespace

class EndpointManager::LockedFrameProcessor {
 public:
  explicit LockedFrameProcessor(FrameProcessorWithMutex* fp)
      : lock_{std::make_unique<MutexLock>(&fp->mutex_)},
        frame_processor_with_mutex_{fp} {}

  // Constructor of a no-op object.
  LockedFrameProcessor() = default;

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
//           handler(EndpointChannel) will be called again.
void EndpointManager::EndpointChannelLoopRunnable(
    const std::string& runnable_name, ClientProxy* client,
    const std::string& endpoint_id,
    absl::AnyInvocable<ExceptionOr<bool>(EndpointChannel*)> handler) {
  // EndpointChannelManager will not let multiple channels exist simultaneously
  // for the same endpoint_id; it will be closing "old" channels as new ones
  // come.
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
        NEARBY_LOGS(INFO)
            << "Received invalid protobuf message, re-fetching endpoint "
               "channel; last_failed_medium="
            << location::nearby::proto::connections::Medium_Name(
                   last_failed_medium);
        continue;
      }
      if (exception.Raised(Exception::kIo)) {
        last_failed_medium = channel->GetMedium();
        NEARBY_LOGS(INFO)
            << "Endpoint channel IO exception; last_failed_medium="
            << location::nearby::proto::connections::Medium_Name(
                   last_failed_medium);
        continue;
      }
      if (exception.Raised(Exception::kInterrupted)) {
        break;
      }
    }

    if (!keep_using_channel.result()) {
      NEARBY_LOGS(INFO) << "Dropping current channel: last medium="
                        << location::nearby::proto::connections::Medium_Name(
                               last_failed_medium);
      if (client->IsSafeToDisconnectEnabled(endpoint_id)) {
        channel_manager_->MarkEndpointStopWaitToDisconnect(
            endpoint_id, /* is_safe_to_disconnect */ false,
            /* notify_stop_waiting */ true);
      }
      break;
    }
  }
  // Indicate we're out of the loop and it is ok to schedule another instance
  // if needed.
  NEARBY_LOGS(INFO) << "Worker going down; worker name=" << runnable_name
                    << "; endpoint_id=" << endpoint_id;
  // Always clear out all state related to this endpoint before terminating
  // this thread.
  DiscardEndpoint(client, endpoint_id, DisconnectionReason::IO_ERROR);
  NEARBY_LOGS(INFO) << "Worker done; worker name=" << runnable_name
                    << "; endpoint_id=" << endpoint_id;
}

ExceptionOr<OfflineFrame> EndpointManager::TryDecryptFrame(
    const ByteArray& data, EndpointChannel* endpoint_channel) {
  auto start_time = SystemClock::ElapsedRealtime();
  while (true) {
    ExceptionOr<ByteArray> decrypted = endpoint_channel->TryDecrypt(data);
    if (decrypted.ok()) {
      NEARBY_LOGS(VERBOSE) << "Message decrypted after "
                           << SystemClock::ElapsedRealtime() - start_time;
      return parser::FromBytes(decrypted.result());
    }
    if (decrypted.exception() == Exception::kExecution) {
      return decrypted.exception();
    }
    auto elapsed = SystemClock::ElapsedRealtime() - start_time;
    if (elapsed > kDecryptRetryTimeout) {
      NEARBY_LOGS(WARNING) << "Can't decrypt the message. Timeout after "
                           << elapsed;
      return Exception::kTimeout;
    }
    SystemClock::Sleep(absl::Milliseconds(1));
  }
}

ExceptionOr<bool> EndpointManager::HandleData(
    const std::string& endpoint_id, ClientProxy* client,
    EndpointChannel* endpoint_channel) {
  bool try_decrypting = !endpoint_channel->IsEncrypted();
  // Read as much as we can from the healthy EndpointChannel - when it is no
  // longer in good shape (i.e. our read from it throws an Exception), our
  // super class will loop back around and try our luck in case there's been
  // a replacement for this endpoint since we last checked with the
  // EndpointChannelManager.
  while (true) {
    PacketMetaData packet_meta_data;
    ExceptionOr<ByteArray> bytes = endpoint_channel->Read(packet_meta_data);
    if (!bytes.ok()) {
      NEARBY_LOG(INFO, "Stop reading on read-time exception: %d",
                 bytes.exception());
      return ExceptionOr<bool>(bytes.exception());
    }
    ExceptionOr<OfflineFrame> wrapped_frame = parser::FromBytes(bytes.result());
    if (!wrapped_frame.ok() && try_decrypting) {
      // Workaround for a race condition where the remote party has sent an
      // encrypted message but our end was still configured as unencrypted when
      // the message was received. The workaround is to wait until the
      // encryption set-up has completed on another thread. We run this
      // workaround if:
      // - the connection was unencrypted when we started reading from the
      // channel
      // - the received frame looks wrong (corrupted)
      // - it's the first invalid frame.
      try_decrypting = false;
      ExceptionOr<OfflineFrame> decrypted =
          TryDecryptFrame(bytes.result(), endpoint_channel);
      if (decrypted.ok()) {
        wrapped_frame = std::move(decrypted);
      }
    }
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
        ProcessDisconnectionFrame(client, endpoint_id, endpoint_channel, frame);
      } else {
        NEARBY_LOGS(ERROR) << "Unhandled message: endpoint_id=" << endpoint_id
                           << ", frame type="
                           << V1Frame::FrameType_Name(frame_type);
      }
      continue;
    }

    frame_processor->OnIncomingFrame(frame, endpoint_id, client,
                                     endpoint_channel->GetMedium(),
                                     packet_meta_data);
  }
}

void EndpointManager::ProcessDisconnectionFrame(
    ClientProxy* client, const std::string& endpoint_id,
    EndpointChannel* endpoint_channel, OfflineFrame& frame) {
  if (!client->IsSafeToDisconnectEnabled(endpoint_id)) {
    NEARBY_LOGS(INFO)
        << "EndpointManager received a DISCONNECTION frame from endpoint "
        << endpoint_id << " on channel " << endpoint_channel->GetType()
        << ", disconnecting...";
    endpoint_channel->Close(DisconnectionReason::REMOTE_DISCONNECTION);
    return;
  }

  if (!frame.v1().has_disconnection() ||
      !frame.v1().disconnection().has_request_safe_to_disconnect() ||
      !frame.v1().disconnection().request_safe_to_disconnect()) {
    NEARBY_LOGS(INFO) << "[safe-to-disconnect] no need to apply "
                         "safe-to-disconnect protocol for endpoint "
                      << endpoint_id << " on channel "
                      << endpoint_channel->GetType() << ", disconnecting...";
    endpoint_channel->Close(DisconnectionReason::REMOTE_DISCONNECTION);
    return;
  }
  NEARBY_LOGS(INFO)
      << "[safe-to-disconnect] received a "
         "DISCONNECTION frame with request safe to disconnect = true and ack = "
      << frame.v1().disconnection().ack_safe_to_disconnect()
      << " from endpoint " << endpoint_id << " on channel "
      << endpoint_channel->GetType()
      << ", disconnecting with safe-to-disconnect protocol ...";
  if (frame.v1().disconnection().ack_safe_to_disconnect()) {
    channel_manager_->MarkEndpointStopWaitToDisconnect(
        endpoint_id, /* is_safe_to_disconnect */ true,
        /* notify_stop_waiting */ true);
  } else {
    channel_manager_->MarkEndpointStopWaitToDisconnect(
        endpoint_id, /* is_safe_to_disconnect */ true,
        /* notify_stop_waiting */ false);
    RunOnEndpointManagerThread(
        "safe-to-disconnect", [this, client, &endpoint_id]() {
          RemoveEndpoint(client, endpoint_id, /*notify=*/true,
                         DisconnectionReason::REMOTE_DISCONNECTION);
        });
    endpoint_channel->Resume();
    NEARBY_LOGS(INFO) << "[safe-to-disconnect] Sending "
                         "DISCONNECTION frame with request 1, ack 1";
    Exception write_exception = endpoint_channel->Write(
        parser::ForDisconnection(/* request_safe_to_disconnect= */ true,
                                 /* ack_safe_to_disconnect= */ true));
    if (!write_exception.Ok()) {
      NEARBY_LOGS(INFO) << "[safe-to-disconnect] Failed to send "
                           "DISCONNECTION frame with ack to endpoint"
                        << endpoint_id;
    }
  }
}

ExceptionOr<bool> EndpointManager::HandleKeepAlive(
    EndpointChannel* endpoint_channel, absl::Duration keep_alive_interval,
    absl::Duration keep_alive_timeout, Mutex* keep_alive_waiter_mutex,
    ConditionVariable* keep_alive_waiter) {
  // Check if it has been too long since we received a frame from our endpoint.
  absl::Time last_read_time = endpoint_channel->GetLastReadTimestamp();
  absl::Duration duration_until_timeout =
      last_read_time == kInvalidTimestamp
          ? keep_alive_timeout
          : last_read_time + keep_alive_timeout -
                SystemClock::ElapsedRealtime();
  if (duration_until_timeout <= absl::ZeroDuration()) {
    return ExceptionOr<bool>(false);
  }

  // If we haven't written anything to the endpoint for a while, attempt to
  // send the KeepAlive frame over the endpoint channel. If the write fails,
  // our super class will loop back around and try our luck again in case
  // there's been a replacement for this endpoint.
  absl::Time last_write_time = endpoint_channel->GetLastWriteTimestamp();
  absl::Duration duration_until_write_keep_alive =
      last_write_time == kInvalidTimestamp
          ? keep_alive_interval
          : last_write_time + keep_alive_interval -
                SystemClock::ElapsedRealtime();
  if (duration_until_write_keep_alive <= absl::ZeroDuration()) {
    Exception write_exception = endpoint_channel->Write(parser::ForKeepAlive());
    if (!write_exception.Ok()) {
      return ExceptionOr<bool>(write_exception);
    }
    duration_until_write_keep_alive = keep_alive_interval;
  }

  absl::Duration wait_for =
      std::min(duration_until_timeout, duration_until_write_keep_alive);
  {
    MutexLock lock(keep_alive_waiter_mutex);
    Exception wait_exception = keep_alive_waiter->Wait(wait_for);
    if (!wait_exception.Ok()) {
      return ExceptionOr<bool>(wait_exception);
    }
  }

  return ExceptionOr<bool>(true);
}

bool operator==(const EndpointManager::FrameProcessor& lhs,
                const EndpointManager::FrameProcessor& rhs) {
  // We're comparing addresses because these objects are callbacks which need
  // to be matched by exact instances.
  return &lhs == &rhs;
}

bool operator<(const EndpointManager::FrameProcessor& lhs,
               const EndpointManager::FrameProcessor& rhs) {
  // We're comparing addresses because these objects are callbacks which need
  // to be matched by exact instances.
  return &lhs < &rhs;
}

EndpointManager::EndpointManager(EndpointChannelManager* manager)
    : EndpointManager(manager, std::make_unique<SingleThreadExecutor>()) {}

EndpointManager::EndpointManager(
    EndpointChannelManager* manager,
    std::unique_ptr<SingleThreadExecutor> serial_executor)
    : channel_manager_(manager), serial_executor_(std::move(serial_executor)) {}

EndpointManager::~EndpointManager() {
  NEARBY_LOG(INFO, "Initiating shutdown of EndpointManager.");
  {
    MutexLock lock(&mutex_);
    is_shutdown_ = true;
  }
  analytics::ThroughputRecorderContainer::GetInstance().Shutdown();
  CountDownLatch latch(1);
  RunOnEndpointManagerThread("bring-down-endpoints", [this, &latch]() {
    NEARBY_LOG(INFO, "Bringing down endpoints");
    endpoints_.clear();
    latch.CountDown();
  });
  latch.Await();

  NEARBY_LOG(INFO, "Bringing down control thread");
  serial_executor_->Shutdown();
  NEARBY_LOG(INFO, "EndpointManager is down");
}

void EndpointManager::RegisterFrameProcessor(
    V1Frame::FrameType frame_type, EndpointManager::FrameProcessor* processor) {
  if (auto frame_processor = GetFrameProcessor(frame_type)) {
    NEARBY_LOGS(INFO) << "EndpointManager received request to update "
                         "registration of frame processor "
                      << processor << " for frame type "
                      << V1Frame::FrameType_Name(frame_type) << ", self"
                      << this;
    frame_processor.set(processor);
  } else {
    MutexLock lock(&frame_processors_lock_);
    NEARBY_LOGS(INFO) << "EndpointManager received request to add registration "
                         "of frame processor "
                      << processor << " for frame type "
                      << V1Frame::FrameType_Name(frame_type)
                      << ", self=" << this;
    frame_processors_.emplace(frame_type, processor);
  }
}

void EndpointManager::UnregisterFrameProcessor(
    V1Frame::FrameType frame_type,
    const EndpointManager::FrameProcessor* processor) {
  NEARBY_LOGS(INFO) << "UnregisterFrameProcessor [enter]: processor ="
                    << processor;
  if (processor == nullptr) return;
  if (auto frame_processor = GetFrameProcessor(frame_type)) {
    if (frame_processor.get() == processor) {
      frame_processor.reset();
      NEARBY_LOGS(INFO) << "EndpointManager unregister frame processor "
                        << processor << " for frame type "
                        << V1Frame::FrameType_Name(frame_type)
                        << ", self=" << this;
    } else {
      NEARBY_LOGS(INFO) << "EndpointManager cannot unregister frame processor "
                        << processor
                        << " because it is not registered for frame type "
                        << V1Frame::FrameType_Name(frame_type)
                        << ", expected=" << frame_processor.get();
    }
  } else {
    NEARBY_LOGS(INFO) << "UnregisterFrameProcessor [not found]: processor="
                      << processor;
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

void EndpointManager::RemoveEndpointState(const std::string& endpoint_id) {
  NEARBY_LOGS(VERBOSE) << "EnsureWorkersTerminated for endpoint "
                       << endpoint_id;
  auto item = endpoints_.find(endpoint_id);
  if (item != endpoints_.end()) {
    NEARBY_LOGS(INFO) << "EndpointState found for endpoint " << endpoint_id;
    // If another instance of data and keep-alive handlers is running, it will
    // terminate soon. Removing EndpointState waits for workers to complete.
    endpoints_.erase(item);
    NEARBY_LOGS(VERBOSE) << "Workers terminated for endpoint " << endpoint_id;
  } else {
    NEARBY_LOGS(INFO) << "EndpointState not found for endpoint " << endpoint_id;
  }
}

void EndpointManager::RegisterEndpoint(
    ClientProxy* client, const std::string& endpoint_id,
    const ConnectionResponseInfo& info,
    const ConnectionOptions& connection_options,
    std::unique_ptr<EndpointChannel> channel,
    const ConnectionListener& listener, const std::string& connection_token) {
  CountDownLatch latch(1);

  // NOTE (unique_ptr<> capture):
  // std::unique_ptr<> is not copyable, so we can not pass it to
  // lambda capture, because lambda eventually is converted to
  // std::function<>. Instead, we release() a pointer, and pass a raw pointer,
  // which is copyalbe. We ignore the risk of job not scheduled (and an
  // associated risk of memory leak), because this may only happen during
  // service shutdown.
  RunOnEndpointManagerThread("register-endpoint", [this, client,
                                                   channel = channel.release(),
                                                   &endpoint_id, &info,
                                                   &connection_options,
                                                   &listener, &connection_token,
                                                   &latch]() {
    if (endpoints_.contains(endpoint_id)) {
      NEARBY_LOGS(WARNING) << "Registering duplicate endpoint " << endpoint_id;
      // We must remove old endpoint state before registering a new one
      // for the same endpoint_id.
      RemoveEndpointState(endpoint_id);
    }

    absl::Duration keep_alive_interval =
        absl::Milliseconds(connection_options.keep_alive_interval_millis);
    absl::Duration keep_alive_timeout =
        absl::Milliseconds(connection_options.keep_alive_timeout_millis);
    NEARBY_LOGS(INFO) << "Registering endpoint " << endpoint_id
                      << " for client " << client->GetClientId()
                      << " with keep-alive frame as interval="
                      << absl::FormatDuration(keep_alive_interval)
                      << ", timeout="
                      << absl::FormatDuration(keep_alive_timeout);

    // Pass ownership of channel to EndpointChannelManager
    NEARBY_LOGS(INFO) << "Registering endpoint with channel manager: endpoint "
                      << endpoint_id;
    channel_manager_->RegisterChannelForEndpoint(
        client, endpoint_id, std::unique_ptr<EndpointChannel>(channel));

    EndpointState& endpoint_state =
        endpoints_
            .emplace(endpoint_id, EndpointState(endpoint_id, channel_manager_))
            .first->second;

    NEARBY_LOGS(INFO) << "Starting workers: endpoint " << endpoint_id;
    // For every endpoint, there's normally only one Read handler instance
    // running on a dedicated thread. This instance reads data from the
    // endpoint and delegates incoming frames to various FrameProcessors.
    // Once the frame has been properly handled, it starts reading again
    // for the next frame. If the handler fails its read and no other
    // EndpointChannels are available for this endpoint, a disconnection
    // will be initiated.
    endpoint_state.StartEndpointReader([this, client, endpoint_id]() {
      EndpointChannelLoopRunnable(
          "Read", client, endpoint_id,
          [this, client, endpoint_id](EndpointChannel* channel) {
            return HandleData(endpoint_id, client, channel);
          });
    });

    // For every endpoint, there's only one KeepAliveManager instance
    // running on a dedicated thread. This instance will periodically send
    // out a ping* to the endpoint while listening for an incoming pong**.
    // If it fails to send the ping, or if no pong is heard within
    // keep_alive_timeout, it initiates a disconnection.
    //
    // (*) Bluetooth requires a constant outgoing stream of messages. If
    // there's silence, Android will break the socket. This is why we
    // ping.
    // (**) Wifi Hotspots can fail to notice a connection has been lost,
    // and they will happily keep writing to /dev/null. This is why we
    // listen for the pong.
    NEARBY_LOGS(VERBOSE) << "EndpointManager enabling KeepAlive for endpoint "
                         << endpoint_id;
    endpoint_state.StartEndpointKeepAliveManager(
        [this, client, endpoint_id, keep_alive_interval, keep_alive_timeout](
            Mutex* keep_alive_waiter_mutex,
            ConditionVariable* keep_alive_waiter) {
          EndpointChannelLoopRunnable(
              "KeepAliveManager", client, endpoint_id,
              [this, keep_alive_interval, keep_alive_timeout,
               keep_alive_waiter_mutex,
               keep_alive_waiter](EndpointChannel* channel) {
                return HandleKeepAlive(
                    channel, keep_alive_interval, keep_alive_timeout,
                    keep_alive_waiter_mutex, keep_alive_waiter);
              });
        });
    NEARBY_LOGS(INFO) << "Registering endpoint " << endpoint_id
                      << ", workers started and notifying client.";

    // It's now time to let the client know of this new connection so that
    // they can accept or reject it.
    client->OnConnectionInitiated(endpoint_id, info, connection_options,
                                  listener, connection_token);
    latch.CountDown();
  });
  latch.Await();
}

void EndpointManager::UnregisterEndpoint(ClientProxy* client,
                                         const std::string& endpoint_id) {
  NEARBY_LOGS(INFO) << "UnregisterEndpoint for endpoint " << endpoint_id;
  CountDownLatch latch(1);
  RunOnEndpointManagerThread(
      "unregister-endpoint", [this, client, endpoint_id, &latch]() {
        RemoveEndpoint(client, endpoint_id,
                       /*notify=*/client->IsConnectedToEndpoint(endpoint_id),
                       DisconnectionReason::LOCAL_DISCONNECTION);
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
    const std::vector<std::string>& endpoint_ids,
    PacketMetaData& packet_meta_data) {
  ByteArray bytes =
      parser::ForDataPayloadTransfer(payload_header, payload_chunk);

  return SendTransferFrameBytes(
      endpoint_ids, bytes, payload_header.id(),
      /*offset=*/payload_chunk.offset(),
      /*packet_type=*/
      PayloadTransferFrame::PacketType_Name(PayloadTransferFrame::DATA),
      packet_meta_data);
}

// Designed to run asynchronously. It is called from IO thread pools, and
// jobs in these pools may be waited for from the EndpointManager thread. If
// we allow synchronous behavior here it will cause a live lock.
void EndpointManager::DiscardEndpoint(ClientProxy* client,
                                      const std::string& endpoint_id,
                                      DisconnectionReason reason) {
  NEARBY_LOGS(INFO) << "DiscardEndpoint for endpoint " << endpoint_id;
  if (reason == DisconnectionReason::IO_ERROR) {
    channel_manager_->MarkEndpointStopWaitToDisconnect(
        endpoint_id, /* is_safe_to_disconnect */ false,
        /* notify_stop_waiting */ true);
  }
  RunOnEndpointManagerThread("discard-endpoint", [this, client, endpoint_id,
                                                  reason]() {
    // `ClientProxy` is destroyed before `EndpointManager` in
    // `~NearbyConnections`, which means "discard-endpoint" needs to check
    // if this task is being executing during `~EndpointManager` to
    // prevent accessing an invalid `ClientProxy` pointer. There are two
    // cases where "discard-endpoint" can be executed during destruction,
    // both of which can safely use `is_shutdown_` to check if this is being
    // executed during the destruction of the object:
    //
    // Case 1: "discard-endpoints" is posted to the thread before
    // destruction, but not executed yet: `~EndpointManager` blocks on
    // "bring-down-endpoints" and because the executor is a single thread
    // executor, tasks are guaranteed to execute sequentially
    // (see
    // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newSingleThreadExecutor--)
    // and this means that the "discard-endpoints" will be executed before
    // "bring-down-endpoints", blocking the destruction of `is_shutdown_`
    // and therefore `is_shutdown_` is not garbage memory.
    //
    // Case 2: "discard-endpoints" is posted to the thread during
    // destruction, after "bring-down-endpoints" is called: the executor
    // will be destructed before `is_shutdown_` because of the ordering of
    // `EndpointManager`'s member variables, and the executor's destructor
    // blocks on running all pending tasks
    // (see
    // https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:chrome/services/sharing/nearby/platform/scheduled_executor.cc;l=67;drc=e0e0d24aaa54727dc0a8bc4b159ccdf80d3f5d8d),
    // which means that "discard-endpoints" will run during the destruction
    // of `serial_executor_` and will still have access to a valid
    // `is_shutdown_`.
    //
    // TODO(b/280653613): Develop a more robost solution to prevent
    // accessing an already destroyed `ClientProxy` during destruction.
    {
      MutexLock lock(&mutex_);
      if (is_shutdown_) {
        NEARBY_LOGS(VERBOSE)
            << "DiscardEndpoint called during destruction, returning early.";
        return;
      }
    }

    RemoveEndpoint(client, endpoint_id,
                   /* notify */client->IsConnectedToEndpoint(endpoint_id),
                   reason);
  });
}

std::vector<std::string> EndpointManager::SendControlMessage(
    const PayloadTransferFrame::PayloadHeader& header,
    const PayloadTransferFrame::ControlMessage& control,
    const std::vector<std::string>& endpoint_ids) {
  ByteArray bytes = parser::ForControlPayloadTransfer(header, control);
  PacketMetaData packet_meta_data;

  return SendTransferFrameBytes(
      endpoint_ids, bytes, header.id(),
      /*offset=*/control.offset(),
      /*packet_type=*/
      PayloadTransferFrame::PacketType_Name(PayloadTransferFrame::CONTROL),
      packet_meta_data);
}

// @EndpointManagerThread
void EndpointManager::RemoveEndpoint(ClientProxy* client,
                                     const std::string& endpoint_id,
                                     bool notify, DisconnectionReason reason) {
  NEARBY_LOGS(INFO) << "RemoveEndpoint for endpoint: " << endpoint_id
                    << ", reason: " << reason;

  SafeDisconnectionResult safe_disconnect_result =
      ConnectionsLog::EstablishedConnection::SAFE_DISCONNECTION;

  // Grab the service ID before we destroy the channel.
  EndpointChannel* channel =
      channel_manager_->GetChannelForEndpoint(endpoint_id).get();
  std::string service_id =
      channel ? channel->GetServiceId() : std::string(kUnknownServiceId);

  if (client->IsSafeToDisconnectEnabled(endpoint_id)) {
    if (channel != nullptr) {
      bool is_safe_disconnection =
          ApplySafeToDisconnect(endpoint_id, channel, reason);
      safe_disconnect_result =
          is_safe_disconnection
              ? ConnectionsLog::EstablishedConnection::SAFE_DISCONNECTION
              : ConnectionsLog::EstablishedConnection::UNSAFE_DISCONNECTION;
      NEARBY_LOGS(INFO) << "[safe-to-disconnect] safe_disconnect_result:"
                        << (safe_disconnect_result? "true" : "false");
    }
  }
  if (safe_disconnect_result ==
      ConnectionsLog::EstablishedConnection::UNSAFE_DISCONNECTION) {
    // TODO(b/297259496): Autoreconnect
  }

  // Unregistering from channel_manager_ will also serve to terminate
  // the dedicated handler and KeepAlive threads we started when we registered
  // this endpoint.
  if (channel_manager_->UnregisterChannelForEndpoint(endpoint_id, reason,
                                                     safe_disconnect_result)) {
    // Notify all frame processors of the disconnection immediately and wait
    // for them to clean up state. Only once all processors are done cleaning
    // up, we can remove the endpoint from ClientProxy after which there
    // should be no further interactions with the endpoint.
    // (See b/37352254 for history)
    WaitForEndpointDisconnectionProcessing(client, service_id, endpoint_id,
                                           reason);

    client->OnDisconnected(endpoint_id, notify);
    NEARBY_LOGS(INFO) << "Removed endpoint for endpoint " << endpoint_id;
  }
  RemoveEndpointState(endpoint_id);
}

bool EndpointManager::ApplySafeToDisconnect(const std::string& endpoint_id,
                                            EndpointChannel* endpoint_channel,
                                            DisconnectionReason reason) {
  NEARBY_LOGS(INFO) << "[safe-to-disconnect] ApplySafeToDisconnect reason: "
                    << reason;
  // TODO(b/303544913): clean up the safe-to-disconnect logic
  bool is_safe_disconnection = false;
  bool send_disconnection_frame = true;
  absl::Duration timeout_millis = FeatureFlags::GetInstance()
                               .GetFlags()
                               .safe_to_disconnect_ack_delay_millis;
  bool is_wait_for_ack = true;
  switch (reason) {
    case DisconnectionReason::UPGRADED:
    case DisconnectionReason::SHUTDOWN:
    case DisconnectionReason::PREV_CHANNEL_DISCONNECTION_IN_RECONNECT:
    case DisconnectionReason::UNFINISHED:
      return true;  // safe disconnection
    case DisconnectionReason::IO_ERROR:
      return false;  // unsafe disconnection
    case DisconnectionReason::LOCAL_DISCONNECTION:
      is_safe_disconnection = true;
      send_disconnection_frame = true;
      break;
    case DisconnectionReason::REMOTE_DISCONNECTION:
      is_safe_disconnection = true;
      send_disconnection_frame = false;
      timeout_millis =
          FeatureFlags::GetInstance()
              .GetFlags()
              .safe_to_disconnect_remote_disc_delay_millis;
      is_wait_for_ack = false;
      break;
    default:
      is_safe_disconnection = false;
      send_disconnection_frame = true;
  }

  if (send_disconnection_frame) {
    // If the channel was paused (i.e. during a bandwidth upgrade negotiation)
    // we resume to ensure the thread won't hang when trying to write to it.
    endpoint_channel->Resume();
    NEARBY_LOGS(INFO) << "[safe-to-disconnect] Sending "
                         "DISCONNECTION frame with request 1, ack 0";
    Exception write_exception = endpoint_channel->Write(
        parser::ForDisconnection(/* request_safe_to_disconnect= */ true,
                                 /* ack_safe_to_disconnect= */ false));

    if (!write_exception.Ok()) {
      NEARBY_LOGS(WARNING) << "[safe-to-disconnect] Failed to send "
                              "DISCONNECTION frame to endpoint"
                           << endpoint_id << " for reason: " << reason;
      return is_safe_disconnection;
    }
  }

  NEARBY_LOGS(WARNING) << "[safe-to-disconnect] Wait for "
                       << (is_wait_for_ack ? "ack" : "disconnection")
                       << ", timeout in " << timeout_millis;
  bool state = channel_manager_->CreateNewTimeoutDisconnectedState(
      endpoint_id, timeout_millis);
  if (!state) return is_safe_disconnection;

  return is_safe_disconnection ||
         channel_manager_->IsSafeToDisconnect(endpoint_id);
}

// @EndpointManagerThread
void EndpointManager::WaitForEndpointDisconnectionProcessing(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id, DisconnectionReason reason) {
  NEARBY_LOGS(INFO) << "Wait: client=" << client
                    << "; service_id=" << service_id
                    << "; endpoint_id=" << endpoint_id;
  CountDownLatch barrier = NotifyFrameProcessorsOnEndpointDisconnect(
      client, service_id, endpoint_id, reason);

  NEARBY_LOGS(INFO)
      << "Waiting for frame processors to disconnect from endpoint "
      << endpoint_id;
  if (!barrier.Await(kProcessEndpointDisconnectionTimeout).result()) {
    NEARBY_LOGS(INFO) << "Failed to disconnect frame processors from endpoint "
                      << endpoint_id;
  } else {
    NEARBY_LOGS(INFO) << "Finished waiting for frame processors to "
                         "disconnect from endpoint "
                      << endpoint_id;
  }
}

CountDownLatch EndpointManager::NotifyFrameProcessorsOnEndpointDisconnect(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id, DisconnectionReason reason) {
  NEARBY_LOGS(INFO) << "NotifyFrameProcessorsOnEndpointDisconnect: client="
                    << client << "; service_id=" << service_id
                    << "; endpoint_id=" << endpoint_id;
  MutexLock lock(&frame_processors_lock_);
  auto total_size = frame_processors_.size();
  NEARBY_LOGS(INFO) << "Total frame processors: " << total_size;
  CountDownLatch barrier(total_size);

  int valid = 0;
  for (auto& item : frame_processors_) {
    LockedFrameProcessor processor(&item.second);
    NEARBY_LOGS(INFO) << "processor=" << processor.get()
                      << "; frame type=" << V1Frame::FrameType_Name(item.first);
    if (processor) {
      valid++;
      processor->OnEndpointDisconnect(client, service_id, endpoint_id, barrier,
                                      reason);
    } else {
      barrier.CountDown();
    }
  }

  if (!valid) {
    NEARBY_LOGS(INFO) << "No valid frame processors.";
  } else {
    NEARBY_LOGS(INFO) << "Valid frame processors: " << valid;
  }
  return barrier;
}

std::vector<std::string> EndpointManager::SendTransferFrameBytes(
    const std::vector<std::string>& endpoint_ids, const ByteArray& bytes,
    std::int64_t payload_id, std::int64_t offset,
    const std::string& packet_type, PacketMetaData& packet_meta_data) {
  std::vector<std::string> failed_endpoint_ids;
  for (const std::string& endpoint_id : endpoint_ids) {
    std::shared_ptr<EndpointChannel> channel =
        channel_manager_->GetChannelForEndpoint(endpoint_id);

    if (channel == nullptr) {
      // We no longer know about this endpoint (it was either explicitly
      // unregistered, or a read/write error made us unregister it
      // internally).
      NEARBY_LOGS(ERROR) << "EndpointManager failed to find EndpointChannel "
                            "over which to write "
                         << packet_type << " at offset " << offset
                         << " of Payload " << payload_id << " to endpoint "
                         << endpoint_id;

      failed_endpoint_ids.push_back(endpoint_id);
      continue;
    }

    Exception write_exception = channel->Write(bytes, packet_meta_data);
    if (!write_exception.Ok()) {
      failed_endpoint_ids.push_back(endpoint_id);
      NEARBY_LOGS(INFO) << "Failed to send packet; endpoint_id=" << endpoint_id;
      continue;
    }
    analytics::ThroughputRecorderContainer::GetInstance()
        .GetTPRecorder(payload_id, PayloadDirection::OUTGOING_PAYLOAD)
        ->OnFrameSent(channel->GetMedium(), packet_meta_data);
  }

  return failed_endpoint_ids;
}

EndpointManager::EndpointState::~EndpointState() {
  // We must unregister the endpoint first to signal the runnables that they
  // should exit their loops. SingleThreadExecutor destructors will wait for
  // the workers to finish. |channel_manager_| is null after moved from this
  // object (in move constructor) which prevents unregistering the channel
  // prematurely.
  if (channel_manager_) {
    NEARBY_LOG(VERBOSE, "EndpointState destructor %s", endpoint_id_.c_str());
    channel_manager_->UnregisterChannelForEndpoint(
        endpoint_id_, DisconnectionReason::SHUTDOWN,
        ConnectionsLog::EstablishedConnection::SAFE_DISCONNECTION);
  }

  // Make sure the KeepAlive thread isn't blocking shutdown.
  if (keep_alive_waiter_mutex_ && keep_alive_waiter_) {
    MutexLock lock(keep_alive_waiter_mutex_.get());
    keep_alive_waiter_->Notify();
  }
}

void EndpointManager::EndpointState::StartEndpointReader(Runnable&& runnable) {
  reader_thread_.Execute("reader", std::move(runnable));
}

void EndpointManager::EndpointState::StartEndpointKeepAliveManager(
    absl::AnyInvocable<void(Mutex*, ConditionVariable*)> runnable) {
  keep_alive_thread_.Execute(
      "keep-alive", [runnable = std::move(runnable),
                     keep_alive_waiter_mutex = keep_alive_waiter_mutex_.get(),
                     keep_alive_waiter = keep_alive_waiter_.get()]() mutable {
        runnable(keep_alive_waiter_mutex, keep_alive_waiter);
      });
}

void EndpointManager::RunOnEndpointManagerThread(const std::string& name,
                                                 Runnable runnable) {
  serial_executor_->Execute(name, std::move(runnable));
}

}  // namespace connections
}  // namespace nearby
