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

#ifndef CORE_INTERNAL_ENDPOINT_MANAGER_H_
#define CORE_INTERNAL_ENDPOINT_MANAGER_H_

#include <cstdint>
#include <memory>

#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_channel.h"
#include "core/internal/endpoint_channel_manager.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/api/count_down_latch.h"
#include "platform/api/submittable_executor.h"
#include "platform/api/system_clock.h"
#include "platform/api/thread_utils.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "platform/runnable.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

namespace endpoint_manager {

template <typename>
class ReaderRunnable;
template <typename>
class KeepAliveManagerRunnable;
template <typename>
class EndpointChannelLoopRunnable;
template <typename>
class RegisterIncomingOfflineFrameProcessorRunnable;
template <typename>
class UnregisterIncomingOfflineFrameProcessorRunnable;
template <typename>
class RegisterEndpointRunnable;
template <typename>
class UnregisterEndpointRunnable;
template <typename>
class DiscardEndpointRunnable;
template <typename>
class GetOfflineFrameProcessorCallable;

}  // namespace endpoint_manager

// Manages all operations related to the remote endpoints with which we are
// interacting.
//
// <p>All processing of incoming and outgoing payloads is spread across this and
// the PayloadManager as described below.
//
// <p>The sending of outgoing payloads originates in
// PayloadManager.sendPayload() before control is transferred over to
// EndpointManager.sendPayloadChunk(). This work happens on one of three
// dedicated writer threads belonging to the PayloadManager. The writer thread
// that is used depends on the PayloadType.
//
// <p>The EndpointManager has one dedicated reader thread for each registered
// endpoint, and the receiving of every incoming payload (and its subsequent
// chunks) originates on one of those threads before control is transferred over
// to PayloadManager.processIncomingOfflineFrame() (still running on that
// same dedicated reader thread).
template <typename Platform>
class EndpointManager {
 public:
  class IncomingOfflineFrameProcessor {
   public:
    virtual ~IncomingOfflineFrameProcessor() {}

    // This function takes full ownership of offline_frame.
    // @EndpointManagerReaderThread
    virtual void processIncomingOfflineFrame(
        ConstPtr<OfflineFrame> offline_frame, const string& from_endpoint_id,
        Ptr<ClientProxy<Platform> > to_client_proxy,
        proto::connections::Medium current_medium) = 0;

    // Implementations must call process_disconnection_barrier.countDown() once
    // they're done. This parallelizes the disconnection event across all frame
    // processors.
    //
    // @EndpointManagerThread
    virtual void processEndpointDisconnection(
        Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
        Ptr<CountDownLatch> process_disconnection_barrier) = 0;

    // Operator overloads when comparing Ptr<IncomingOfflineFrameProcessor>.
    bool operator==(
        const typename EndpointManager<Platform>::IncomingOfflineFrameProcessor&
            rhs);
    bool operator<(
        const typename EndpointManager<Platform>::IncomingOfflineFrameProcessor&
            rhs);
  };

  explicit EndpointManager(
      Ptr<EndpointChannelManager> endpoint_channel_manager);
  ~EndpointManager();

  // Invoked from the constructors of the various *Manager components that make
  // up the OfflineServiceController implementation.
  void registerIncomingOfflineFrameProcessor(
      V1Frame::FrameType frame_type,
      Ptr<IncomingOfflineFrameProcessor> processor);
  void unregisterIncomingOfflineFrameProcessor(
      V1Frame::FrameType frame_type,
      Ptr<IncomingOfflineFrameProcessor> processor);

  // Invoked from the different PCPHandler implementations (of which there can
  // be only one at a time).
  void registerEndpoint(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      const string& endpoint_name, const string& authentication_token,
      ConstPtr<ByteArray> raw_authentication_token, bool is_incoming,
      Ptr<EndpointChannel> endpoint_channel,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener);
  // Called when a client explicitly asks to disconnect from this endpoint. In
  // this case, we do not notify the client of onDisconnected().
  void unregisterEndpoint(Ptr<ClientProxy<Platform> > client_proxy,
                          const string& endpoint_id);
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
  // ask everyone who's registered an IncomingOfflineFrameProcessor to
  // processEndpointDisconnection() while the caller of discardEndpoint() is
  // blocked here.
  void discardEndpoint(Ptr<ClientProxy<Platform> > client_proxy,
                       const string& endpoint_id);

  Ptr<IncomingOfflineFrameProcessor> getOfflineFrameProcessor(
      V1Frame::FrameType frame_type);

  // Returns the list of endpoints to which sending this chunk failed.
  //
  // Invoked from the PayloadManager's sendPayload() method.
  std::vector<string> sendPayloadChunk(
      const PayloadTransferFrame::PayloadHeader& payload_header,
      const PayloadTransferFrame::PayloadChunk& payload_chunk,
      const std::vector<string>& endpoint_ids);
  void sendControlMessage(
      const PayloadTransferFrame::PayloadHeader& payload_header,
      const PayloadTransferFrame::ControlMessage& control_message,
      const std::vector<string>& endpoint_ids);

 private:
  template <typename>
  friend class endpoint_manager::ReaderRunnable;
  template <typename>
  friend class endpoint_manager::KeepAliveManagerRunnable;
  template <typename>
  friend class endpoint_manager::EndpointChannelLoopRunnable;
  template <typename>
  friend class endpoint_manager::RegisterIncomingOfflineFrameProcessorRunnable;
  template <typename>
  friend class endpoint_manager::
      UnregisterIncomingOfflineFrameProcessorRunnable;
  template <typename>
  friend class endpoint_manager::RegisterEndpointRunnable;
  template <typename>
  friend class endpoint_manager::UnregisterEndpointRunnable;
  template <typename>
  friend class endpoint_manager::DiscardEndpointRunnable;
  template <typename>
  friend class endpoint_manager::GetOfflineFrameProcessorCallable;

  static void waitForLatch(const string& method_name,
                           Ptr<CountDownLatch> latch);
  static void waitForLatch(const string& method_name, Ptr<CountDownLatch> latch,
                           std::int32_t timeout_millis);
  template <typename T>
  static T waitForResult(const string& method_name,
                         Ptr<Future<T> > result_future);

  static const std::int32_t kKeepAliveWriteIntervalMillis;
  static const std::int32_t kKeepAliveReadTimeoutMillis;
  static const std::int32_t kProcessEndpointDisconnectionTimeoutMillis;
  static const std::int32_t kMaxConcurrentEndpoints;
  static const std::int32_t kEndpointIdLength;

  // It should be noted that this method may be called multiple times (because
  // invoking this method closes the endpoint channel, which causes the
  // dedicated reader and KeepAlive threads to terminate, which in turn leads to
  // this method being called), but that's alright because the implementation of
  // this method is idempotent.
  void removeEndpoint(Ptr<ClientProxy<Platform> > client_proxy,
                      const string& endpoint_id,
                      bool send_disconnection_notification);

  void waitForEndpointDisconnectionProcessing(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id);

  std::vector<string> sendTransferFrameBytes(
      const std::vector<string>& endpoint_ids,
      ConstPtr<ByteArray> payload_transfer_frame_bytes, std::int64_t payload_id,
      std::int64_t offset, const string& packet_type);

  void startEndpointReader(Ptr<Runnable> runnable);
  void startEndpointKeepAliveManager(Ptr<Runnable> runnable);
  void runOnEndpointManagerThread(Ptr<Runnable> runnable);
  template <typename T>
  Ptr<Future<T> > runOnEndpointManagerThread(Ptr<Callable<T> > callable);

  ScopedPtr<Ptr<ThreadUtils> > thread_utils_;
  ScopedPtr<Ptr<SystemClock> > system_clock_;

  Ptr<EndpointChannelManager> endpoint_channel_manager_;

  typedef std::map<V1Frame::FrameType, Ptr<IncomingOfflineFrameProcessor> >
      IncomingOfflineFrameProcessorsMap;
  IncomingOfflineFrameProcessorsMap incoming_offline_frame_processors_;

  ScopedPtr<Ptr<typename Platform::MultiThreadExecutorType> >
      endpoint_keep_alive_manager_thread_pool_;
  ScopedPtr<Ptr<typename Platform::MultiThreadExecutorType> >
      endpoint_readers_thread_pool_;
  ScopedPtr<Ptr<typename Platform::SingleThreadExecutorType> > serial_executor_;
  std::shared_ptr<EndpointManager<Platform>> self_{this, [](void*){}};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/endpoint_manager.cc"

#endif  // CORE_INTERNAL_ENDPOINT_MANAGER_H_
