// Copyright 2021 Google LLC
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

#ifndef CORE_INTERNAL_ENDPOINT_MANAGER_H_
#define CORE_INTERNAL_ENDPOINT_MANAGER_H_

#include <cstdint>
#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/listeners.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/runnable.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/system_clock.h"

namespace location {
namespace nearby {
namespace connections {

// Manages all operations related to the remote endpoints with which we are
// interacting.
//
// All processing of incoming and outgoing payloads is spread across this and
// the PayloadManager as described below.
//
// The sending of outgoing payloads originates in
// PayloadManager::SendPayload() before control is transferred over to
// EndpointManager::SendPayloadChunk(). This work happens on one of three
// dedicated writer threads belonging to the PayloadManager. The writer thread
// that is used depends on the PayloadType.
//
// The EndpointManager has one dedicated reader thread for each registered
// endpoint, and the receiving of every incoming payload (and its subsequent
// chunks) originates on one of those threads before control is transferred over
// to PayloadManager::ProcessFrame() (still running on that
// same dedicated reader thread).

class EndpointManager {
 public:
  class FrameProcessor {
   public:
    virtual ~FrameProcessor() = default;

    // @EndpointManagerReaderThread
    // Called for every incoming frame of registered type.
    // NOTE(OfflineFrame& frame):
    // For large payload in data phase, resources may be saved if data is moved,
    // rather than copied (if passing data by reference is not an option).
    // To achieve that, OfflineFrame needs to be either mutabe lvalue reference,
    // or rvalue reference. Rvalue references are discouraged by go/cstyle,
    // and that leaves us with mutable lvalue reference.
    virtual void OnIncomingFrame(OfflineFrame& offline_frame,
                                 const std::string& from_endpoint_id,
                                 ClientProxy* to_client,
                                 proto::connections::Medium current_medium) = 0;

    // Implementations must call barrier.CountDown() once
    // they're done. This parallelizes the disconnection event across all frame
    // processors.
    //
    // @EndpointManagerThread
    virtual void OnEndpointDisconnect(ClientProxy* client,
                                      const std::string& endpoint_id,
                                      CountDownLatch barrier) = 0;
  };

  explicit EndpointManager(EndpointChannelManager* manager);
  ~EndpointManager();

  // Invoked from the constructors of the various *Manager components that make
  // up the OfflineServiceController implementation.
  // FrameProcessor* instances are of dynamic duration and survive all sessions.
  // Blocks until registration is complete.
  void RegisterFrameProcessor(V1Frame::FrameType frame_type,
                              FrameProcessor* processor);
  void UnregisterFrameProcessor(V1Frame::FrameType frame_type,
                                const FrameProcessor* processor);

  // Invoked from the different PcpHandler implementations (of which there can
  // be only one at a time).
  // Blocks until registration is complete.
  void RegisterEndpoint(ClientProxy* client, const std::string& endpoint_id,
                        const ConnectionResponseInfo& info,
                        const ConnectionOptions& connection_options,
                        std::unique_ptr<EndpointChannel> channel,
                        const ConnectionListener& listener,
                        const std::string& connection_token);
  // Called when a client explicitly asks to disconnect from this endpoint. In
  // this case, we do not notify the client of onDisconnected().
  void UnregisterEndpoint(ClientProxy* client, const std::string& endpoint_id);

  // Returns the maximum supported transmit packet size(MTU) for the underlying
  // transport.
  int GetMaxTransmitPacketSize(const std::string& endpoint_id);

  // Returns the list of endpoints to which sending this chunk failed.
  //
  // Invoked from the PayloadManager's sendPayload() method.
  std::vector<std::string> SendPayloadChunk(
      const PayloadTransferFrame::PayloadHeader& payload_header,
      const PayloadTransferFrame::PayloadChunk& payload_chunk,
      const std::vector<std::string>& endpoint_ids);
  std::vector<std::string> SendControlMessage(
      const PayloadTransferFrame::PayloadHeader& payload_header,
      const PayloadTransferFrame::ControlMessage& control_message,
      const std::vector<std::string>& endpoint_ids);

  // Called when we internally want to get rid of the endpoint, without the
  // client directly telling us to. For example...
  //    a) We failed to read from the endpoint in its dedicated reader thread.
  //    b) We failed to write to the endpoint in PayloadManager.
  //    c) The connection was rejected in PCPHandler.
  //    d) The dedicated KeepAlive thread exceeded its period of inactivity.
  // Or in the numerous other cases where a failure occurred and we no longer
  // believe the endpoint is in a healthy state.
  //
  // Note: This must not block. Otherwise we can get into a deadlock where we
  // ask everyone who's registered an FrameProcessor to
  // processEndpointDisconnection() while the caller of DiscardEndpoint() is
  // blocked here.
  void DiscardEndpoint(ClientProxy* client, const std::string& endpoint_id);

 private:
  class EndpointState {
   public:
    EndpointState(const std::string& endpoint_id,
                  EndpointChannelManager* channel_manager)
        : endpoint_id_{endpoint_id},
          channel_manager_{channel_manager},
          keep_alive_waiter_mutex_{std::make_unique<Mutex>()},
          keep_alive_waiter_{std::make_unique<ConditionVariable>(
              keep_alive_waiter_mutex_.get())} {}

    EndpointState(const EndpointState&) = delete;
    // The default move constructor would not reset |channel_manager_|, for
    // example. This needs to be nullified so the destructor shutdown logic is
    // bypassed when objects are moved.
    EndpointState(EndpointState&& other)
        : endpoint_id_{std::move(other.endpoint_id_)},
          channel_manager_{std::exchange(other.channel_manager_, nullptr)},
          reader_thread_{std::move(other.reader_thread_)},
          keep_alive_waiter_mutex_{
              std::exchange(other.keep_alive_waiter_mutex_, nullptr)},
          keep_alive_waiter_{std::exchange(other.keep_alive_waiter_, nullptr)},
          keep_alive_thread_{std::move(other.keep_alive_thread_)} {}
    EndpointState& operator=(const EndpointState&) = delete;
    EndpointState&& operator=(EndpointState&&) = delete;
    ~EndpointState();

    void StartEndpointReader(Runnable&& runnable);
    void StartEndpointKeepAliveManager(
        std::function<void(Mutex*, ConditionVariable*)> runnable);

   private:
    const std::string endpoint_id_;
    EndpointChannelManager* channel_manager_;
    SingleThreadExecutor reader_thread_;

    // Use a condition variable so we can wait on the thread but still be able
    // to wake it up before shutting down. We don't want to just sleep and risk
    // blocking shutdown. Note: Create the mutex/condition variable on the heap
    // so raw pointers sent to HandleKeepAlive() aren't invalidated during
    // std::move operations.
    mutable std::unique_ptr<Mutex> keep_alive_waiter_mutex_;
    std::unique_ptr<ConditionVariable> keep_alive_waiter_;
    SingleThreadExecutor keep_alive_thread_;
  };

  // RAII accessor for FrameProcessor
  class LockedFrameProcessor;

  // Provides a mutex per FrameProcessor to prevent unregistering (and
  // destroying) a FrameProcessor when it's in use.
  class FrameProcessorWithMutex {
   public:
    explicit FrameProcessorWithMutex(FrameProcessor* frame_processor = nullptr)
        : frame_processor_{frame_processor} {}

   private:
    FrameProcessor* frame_processor_;
    Mutex mutex_;
    friend class LockedFrameProcessor;
  };

  LockedFrameProcessor GetFrameProcessor(V1Frame::FrameType frame_type);

  ExceptionOr<bool> HandleData(const std::string& endpoint_id,
                               ClientProxy* client_proxy,
                               EndpointChannel* endpoint_channel);

  ExceptionOr<bool> HandleKeepAlive(EndpointChannel* endpoint_channel,
                                    absl::Duration keep_alive_interval,
                                    absl::Duration keep_alive_timeout,
                                    Mutex* keep_alive_waiter_mutex,
                                    ConditionVariable* keep_alive_waiter);

  // Waits for a given endpoint EndpointChannelLoopRunnable() workers to
  // terminate.
  // Is called from RegisterEndpoint to avoid races; also called from
  // RemoveEndpoint as part of proper endpoint shutdown sequence.
  // @EndpointManagerThread
  void RemoveEndpointState(const std::string& endpoint_id);

  void EndpointChannelLoopRunnable(
      const std::string& runnable_name, ClientProxy* client_proxy,
      const std::string& endpoint_id,
      std::function<ExceptionOr<bool>(EndpointChannel*)> handler);

  static void WaitForLatch(const std::string& method_name,
                           CountDownLatch* latch);
  static void WaitForLatch(const std::string& method_name,
                           CountDownLatch* latch, std::int32_t timeout_millis);

  static constexpr absl::Duration kProcessEndpointDisconnectionTimeout =
      absl::Milliseconds(2000);
  static constexpr absl::Time kInvalidTimestamp = absl::InfinitePast();

  // It should be noted that this method may be called multiple times (because
  // invoking this method closes the endpoint channel, which causes the
  // dedicated reader and KeepAlive threads to terminate, which in turn leads to
  // this method being called), but that's alright because the implementation of
  // this method is idempotent.
  // @EndpointManagerThread
  void RemoveEndpoint(ClientProxy* client, const std::string& endpoint_id,
                      bool notify);

  void WaitForEndpointDisconnectionProcessing(ClientProxy* client,
                                              const std::string& endpoint_id);

  CountDownLatch NotifyFrameProcessorsOnEndpointDisconnect(
      ClientProxy* client, const std::string& endpoint_id);

  std::vector<std::string> SendTransferFrameBytes(
      const std::vector<std::string>& endpoint_ids,
      const ByteArray& payload_transfer_frame_bytes, std::int64_t payload_id,
      std::int64_t offset, const std::string& packet_type);

  // Executes all jobs sequentially, on a serial_executor_.
  void RunOnEndpointManagerThread(const std::string& name, Runnable runnable);

  EndpointChannelManager* channel_manager_;

  RecursiveMutex frame_processors_lock_;
  absl::flat_hash_map<V1Frame::FrameType, FrameProcessorWithMutex>
      frame_processors_ ABSL_GUARDED_BY(frame_processors_lock_);

  // We keep track of all registered channel endpoints here.
  absl::flat_hash_map<std::string, EndpointState> endpoints_;

  SingleThreadExecutor serial_executor_;
};

// Operator overloads when comparing FrameProcessor*.
bool operator==(const EndpointManager::FrameProcessor& lhs,
                const EndpointManager::FrameProcessor& rhs);
bool operator<(const EndpointManager::FrameProcessor& lhs,
               const EndpointManager::FrameProcessor& rhs);

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_ENDPOINT_MANAGER_H_
