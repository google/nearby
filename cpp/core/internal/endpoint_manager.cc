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

#include <utility>

#include "core/internal/offline_frames.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

namespace endpoint_manager {

// A Runnable that continuously grabs the most recent EndpointChannel available
// for an endpoint. Override
// EndpointChannelLoopRunnable.execute(EndpointChannel) to interact with the
// EndpointChannel.
template <typename Platform>
class EndpointChannelLoopRunnable : public Runnable {
 public:
  EndpointChannelLoopRunnable(Ptr<EndpointManager<Platform>> endpoint_manager,
                              const string& runnable_name,
                              Ptr<ClientProxy<Platform>> client_proxy,
                              const string& endpoint_id)
      : endpoint_manager_(endpoint_manager),
        runnable_name_(runnable_name),
        client_proxy_(client_proxy),
        endpoint_id_(endpoint_id) {}
  ~EndpointChannelLoopRunnable() override {}

  void run() override {
    // The implication of using the EndpointChannel's medium to identify it is
    // that this loop will break if we ever allow creating multiple
    // EndpointChannels to the same endpoint over the same medium.
    proto::connections::Medium last_failed_endpoint_channel_medium =
        proto::connections::UNKNOWN_MEDIUM;
    while (true) {
      // It's important to keep re-fetching the EndpointChannel for an endpoint
      // because it can be changed out from under us (for example, when we
      // upgrade from Bluetooth to Wifi).
      ScopedPtr<Ptr<EndpointChannel>> scoped_endpoint_channel(
          endpoint_manager_->endpoint_channel_manager_->getChannelForEndpoint(
              endpoint_id_));
      if (scoped_endpoint_channel.isNull()) {
        // TODO(tracyzhou): Add logging.
        break;
      }

      // If we're looping back around after a failure, and there's not a new
      // EndpointChannel for this endpoint, there's nothing more to do here.
      if ((last_failed_endpoint_channel_medium !=
           proto::connections::UNKNOWN_MEDIUM) &&
          (scoped_endpoint_channel->getMedium() ==
           last_failed_endpoint_channel_medium)) {
        // TODO(tracyzhou): Add logging.
        break;
      }

      ExceptionOr<bool> keep_using_channel =
          useHealthyEndpointChannel(scoped_endpoint_channel.get());

      if (!keep_using_channel.ok()) {
        Exception::Value exception = keep_using_channel.exception();
        if (Exception::IO == exception) {
          last_failed_endpoint_channel_medium =
              scoped_endpoint_channel->getMedium();
          // TODO(tracyzhou): Add logging.
          continue;
        }
        if (Exception::INTERRUPTED == exception) {
          // Thread.currentThread().interrupt();
          // TODO(tracyzhou): Add logging.
          break;
        }
      }

      if (!keep_using_channel.result()) {
        // TODO(tracyzhou): Add logging.
        break;
      }
    }

    // Always clear out all state related to this endpoint before terminating
    // this thread.
    endpoint_manager_->discardEndpoint(client_proxy_, endpoint_id_);
  }

  // Called whenever an EndpointChannel is available for endpointId.
  // Implementations are expected to read/write freely to the EndpointChannel
  // until an Exception::IO is thrown. Once an Exception::IO occurs, a check
  // will be performed to see if another EndpointChannel is available for the
  // given endpoint and, if so, useHealthyEndpointChannel(EndpointChannel) will
  // be called again.
  //
  // <p>Return false to exit the loop.
  virtual ExceptionOr<bool> useHealthyEndpointChannel(
      Ptr<EndpointChannel> endpoint_channel) = 0;  // throws Exception::IO,
                                                   // Exception::INTERRUPTED

 protected:
  Ptr<EndpointManager<Platform>> endpoint_manager_;
  const string runnable_name_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  const string endpoint_id_;
};

template <typename Platform>
class ReaderRunnable : public EndpointChannelLoopRunnable<Platform> {
 public:
  ReaderRunnable(Ptr<EndpointManager<Platform>> endpoint_manager,
                 Ptr<ClientProxy<Platform>> client_proxy,
                 const string& endpoint_id)
      : EndpointChannelLoopRunnable<Platform>(endpoint_manager, "Read",
                                              client_proxy, endpoint_id) {}

  // @EndpointManagerReaderThread
  ExceptionOr<bool> useHealthyEndpointChannel(
      Ptr<EndpointChannel> endpoint_channel) override {
    // Read as much as we can from the healthy EndpointChannel - when it is no
    // longer in good shape (i.e. our read from it throws an Exception), our
    // super class will loop back around and try our luck in case there's been
    // a replacement for this endpoint since we last checked with the
    // EndpointChannelManager.
    while (true) {
      ExceptionOr<ConstPtr<ByteArray>> read_bytes = endpoint_channel->read();
      if (!read_bytes.ok()) {
        if (Exception::INVALID_PROTOCOL_BUFFER == read_bytes.exception()) {
          // TODO(reznor): logger.atDebug().withCause(e).log("EndpointManager
          // failed to decode message from endpoint %s on channel %s,
          // discarding.", endpointId, endpointChannel.getType());
          continue;
        } else if (Exception::IO == read_bytes.exception()) {
          return ExceptionOr<bool>(read_bytes.exception());
        }
      }
      ScopedPtr<ConstPtr<ByteArray>> scoped_read_bytes(read_bytes.result());

      ExceptionOr<ConstPtr<OfflineFrame>> offline_frame =
          OfflineFrames::fromBytes(scoped_read_bytes.get());
      if (!offline_frame.ok()) {
        if (Exception::INVALID_PROTOCOL_BUFFER == offline_frame.exception()) {
          // TODO(reznor): logger.atDebug().withCause(e).log("EndpointManager
          // received an invalid OfflineFrame from endpoint %s on channel %s,
          // discarding.", endpointId, endpointChannel.getType());
          continue;
        }
      }
      ScopedPtr<ConstPtr<OfflineFrame>> scoped_offline_frame(
          offline_frame.result());

      // Route the incoming offlineFrame to its registered processor.
      V1Frame::FrameType frame_type =
          OfflineFrames::getFrameType(scoped_offline_frame.get());
      Ptr<typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>
          incoming_offline_frame_processor =
              this->endpoint_manager_->getOfflineFrameProcessor(frame_type);
      if (incoming_offline_frame_processor.isNull()) {
        // TODO(tracyzhou): Add logging.
        continue;
      }

      incoming_offline_frame_processor->processIncomingOfflineFrame(
          scoped_offline_frame.release(), this->endpoint_id_,
          this->client_proxy_, endpoint_channel->getMedium());
    }
  }
};

template <typename Platform>
class KeepAliveManagerRunnable : public EndpointChannelLoopRunnable<Platform> {
 public:
  KeepAliveManagerRunnable(Ptr<EndpointManager<Platform>> endpoint_manager,
                           Ptr<ClientProxy<Platform>> client_proxy,
                           const string& endpoint_id)
      : EndpointChannelLoopRunnable<Platform>(
            endpoint_manager, "KeepAliveManager", client_proxy, endpoint_id) {}

  // @EndpointManagerKeepAliveThread
  ExceptionOr<bool> useHealthyEndpointChannel(
      Ptr<EndpointChannel> endpoint_channel) override {
    // Check if it has been too long since we received a frame from our
    // endpoint.
    if ((endpoint_channel->getLastReadTimestamp() != -1) &&
        ((endpoint_channel->getLastReadTimestamp() +
          EndpointManager<Platform>::kKeepAliveReadTimeoutMillis) <
         this->endpoint_manager_->system_clock_->elapsedRealtime())) {
      // TODO(tracyzhou): Add logging.
      return ExceptionOr<bool>(false);
    }

    // Attempt to send the KeepAlive frame over the endpoint channel - if the
    // write fails, our super class will loop back around and try our luck again
    // in case there's been a replacement for this endpoint.
    Exception::Value write_exception =
        endpoint_channel->write(OfflineFrames::forKeepAlive());
    if (Exception::NONE != write_exception) {
      if (Exception::IO == write_exception) {
        return ExceptionOr<bool>(write_exception);
      }
    }

    // We sleep as the very last step because we want to minimize the caching of
    // the EndpointChannel. If we do hold on to the EndpointChannel, and it's
    // switched out from under us in BandwidthUpgradeManager, our write will
    // trigger an erroneous write to the encryption context that will cascade
    // into all our remote endpoint's future reads failing.
    Exception::Value sleep_exception =
        this->endpoint_manager_->thread_utils_->sleep(
            EndpointManager<Platform>::kKeepAliveWriteIntervalMillis);
    if (Exception::NONE != sleep_exception) {
      if (Exception::INTERRUPTED == sleep_exception) {
        return ExceptionOr<bool>(sleep_exception);
      }
    }

    return ExceptionOr<bool>(true);
  }
};

template <typename Platform>
class RegisterIncomingOfflineFrameProcessorRunnable : public Runnable {
 public:
  RegisterIncomingOfflineFrameProcessorRunnable(
      Ptr<EndpointManager<Platform>> endpoint_manager,
      V1Frame::FrameType frame_type,
      Ptr<typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>
          processor)
      : endpoint_manager_(endpoint_manager),
        frame_type_(frame_type),
        processor_(processor) {}

  void run() override {
    typename EndpointManager<
        Platform>::IncomingOfflineFrameProcessorsMap::iterator it =
        endpoint_manager_->incoming_offline_frame_processors_.find(frame_type_);
    if (it != endpoint_manager_->incoming_offline_frame_processors_.end()) {
      // TODO(tracyzhou): Add logging.
      it->second = processor_;
    } else {
      endpoint_manager_->incoming_offline_frame_processors_.insert(
          std::make_pair(frame_type_, processor_));
    }
  }

 private:
  Ptr<EndpointManager<Platform>> endpoint_manager_;
  const V1Frame::FrameType frame_type_;
  Ptr<typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>
      processor_;
};

template <typename Platform>
class UnregisterIncomingOfflineFrameProcessorRunnable : public Runnable {
 public:
  UnregisterIncomingOfflineFrameProcessorRunnable(
      Ptr<EndpointManager<Platform>> endpoint_manager,
      V1Frame::FrameType frame_type,
      Ptr<typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>
          processor)
      : endpoint_manager_(endpoint_manager),
        frame_type_(frame_type),
        processor_(processor) {}

  void run() override {
    typename EndpointManager<
        Platform>::IncomingOfflineFrameProcessorsMap::iterator it =
        endpoint_manager_->incoming_offline_frame_processors_.find(frame_type_);
    if (it != endpoint_manager_->incoming_offline_frame_processors_.end()) {
      if (it->second != processor_) {
        // TODO(tracyzhou): Add logging.
        return;
      }

      endpoint_manager_->incoming_offline_frame_processors_.erase(it);
    }
  }

 private:
  Ptr<EndpointManager<Platform>> endpoint_manager_;
  const V1Frame::FrameType frame_type_;
  Ptr<typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>
      processor_;
};

template <typename Platform>
class RegisterEndpointRunnable : public Runnable {
 public:
  RegisterEndpointRunnable(
      Ptr<EndpointManager<Platform>> endpoint_manager,
      Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id,
      const string& endpoint_name, const string& authentication_token,
      ConstPtr<ByteArray> raw_authentication_token, bool is_incoming,
      Ptr<EndpointChannel> endpoint_channel,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener,
      Ptr<CountDownLatch> latch)
      : endpoint_manager_(endpoint_manager),
        client_proxy_(client_proxy),
        endpoint_id_(endpoint_id),
        endpoint_name_(endpoint_name),
        authentication_token_(authentication_token),
        raw_authentication_token_(raw_authentication_token),
        is_incoming_(is_incoming),
        endpoint_channel_(endpoint_channel),
        connection_lifecycle_listener_(connection_lifecycle_listener),
        latch_(latch) {}

  void run() override {
    endpoint_manager_->endpoint_channel_manager_->registerChannelForEndpoint(
        client_proxy_, endpoint_id_, endpoint_channel_);

    // For every endpoint, there's one Reader instance running on the
    // EndpointManagerReaderThread. This instance reads from the endpoint and
    // delegates incoming frames to various IncomingOfflineFrameProcessors.
    // Once the frame has been properly handled, it starts reading again for the
    // next frame. If the Reader fails its read and no other EndpointChannels
    // are available for this endpoint, a disconnection will be initiated.
    endpoint_manager_->startEndpointReader(MakePtr(new ReaderRunnable<Platform>(
        endpoint_manager_, client_proxy_, endpoint_id_)));

    // For every endpoint, there's one KeepAliveManager instance running on the
    // EndpointManagerKeepAliveThread. This instance will periodically
    // send out a ping* to the endpoint while listening for an incoming pong**.
    // If it fails to send the ping, or if no pong is heard within
    // kKeepAliveReadTimeoutMillis milliseconds, it initiates a
    // disconnection.
    //
    // (*) Bluetooth requires a constant outgoing stream of messages. If there's
    // silence, Android will break the socket. This is why we ping.
    // (**) Wifi Hotspots can fail to notice a connection has been lost, and
    // they will happily keep writing to /dev/null. This is why we listen for
    // the pong.
    endpoint_manager_->startEndpointKeepAliveManager(
        MakePtr(new KeepAliveManagerRunnable<Platform>(
            endpoint_manager_, client_proxy_, endpoint_id_)));
    // TODO(tracyzhou): Add logging.

    // It's now time to let the client know of this new connection so that they
    // can accept or reject it.
    client_proxy_->onConnectionInitiated(
        endpoint_id_, endpoint_name_, authentication_token_,
        raw_authentication_token_.release(), is_incoming_,
        connection_lifecycle_listener_.release());
    latch_->countDown();
  }

 private:
  Ptr<EndpointManager<Platform>> endpoint_manager_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  const string endpoint_id_;
  const string endpoint_name_;
  const string authentication_token_;
  ScopedPtr<ConstPtr<ByteArray>> raw_authentication_token_;
  const bool is_incoming_;
  Ptr<EndpointChannel> endpoint_channel_;
  ScopedPtr<Ptr<ConnectionLifecycleListener>> connection_lifecycle_listener_;
  Ptr<CountDownLatch> latch_;
};

template <typename Platform>
class UnregisterEndpointRunnable : public Runnable {
 public:
  UnregisterEndpointRunnable(Ptr<EndpointManager<Platform>> endpoint_manager,
                             Ptr<ClientProxy<Platform>> client_proxy,
                             const string& endpoint_id,
                             Ptr<CountDownLatch> latch)
      : endpoint_manager_(endpoint_manager),
        client_proxy_(client_proxy),
        endpoint_id_(endpoint_id),
        latch_(latch) {}

  void run() override {
    endpoint_manager_->removeEndpoint(
        client_proxy_, endpoint_id_, /*send_disconnection_notification=*/false);

    latch_->countDown();
  }

 private:
  Ptr<EndpointManager<Platform>> endpoint_manager_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  const string endpoint_id_;
  Ptr<CountDownLatch> latch_;
};

template <typename Platform>
class DiscardEndpointRunnable : public Runnable {
 public:
  DiscardEndpointRunnable(Ptr<EndpointManager<Platform>> endpoint_manager,
                          Ptr<ClientProxy<Platform>> client_proxy,
                          const string& endpoint_id)
      : endpoint_manager_(endpoint_manager),
        client_proxy_(client_proxy),
        endpoint_id_(endpoint_id) {}

  void run() override {
    endpoint_manager_->removeEndpoint(
        client_proxy_, endpoint_id_,
        /*send_disconnection_notification=*/
        client_proxy_->isConnectedToEndpoint(endpoint_id_));
  }

 private:
  Ptr<EndpointManager<Platform>> endpoint_manager_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  const string endpoint_id_;
};

template <typename Platform>
class GetOfflineFrameProcessorCallable
    : public Callable<Ptr<
          typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>> {
 public:
  typedef Ptr<typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>
      ReturnType;

  GetOfflineFrameProcessorCallable(
      Ptr<EndpointManager<Platform>> endpoint_manager,
      V1Frame::FrameType frame_type)
      : endpoint_manager_(endpoint_manager), frame_type_(frame_type) {}

  ExceptionOr<ReturnType> call() override {
    typename EndpointManager<
        Platform>::IncomingOfflineFrameProcessorsMap::iterator it =
        endpoint_manager_->incoming_offline_frame_processors_.find(frame_type_);
    if (it == endpoint_manager_->incoming_offline_frame_processors_.end()) {
      return ExceptionOr<ReturnType>(ReturnType());
    }
    return ExceptionOr<ReturnType>(it->second);
  }

 private:
  Ptr<EndpointManager<Platform>> endpoint_manager_;
  const V1Frame::FrameType frame_type_;
};

}  // namespace endpoint_manager

template <typename Platform>
bool EndpointManager<Platform>::IncomingOfflineFrameProcessor::operator==(
    const EndpointManager<Platform>::IncomingOfflineFrameProcessor& rhs) {
  // We're comparing addresses because these objects are callbacks which need to
  // be matched by exact instances.
  return this == &rhs;
}

template <typename Platform>
bool EndpointManager<Platform>::IncomingOfflineFrameProcessor::operator<(
    const EndpointManager<Platform>::IncomingOfflineFrameProcessor& rhs) {
  // We're comparing addresses because these objects are callbacks which need to
  // be matched by exact instances.
  return this < &rhs;
}

template <typename Platform>
const std::int32_t EndpointManager<Platform>::kKeepAliveWriteIntervalMillis =
    5000;
template <typename Platform>
const std::int32_t EndpointManager<Platform>::kKeepAliveReadTimeoutMillis =
    30000;
template <typename Platform>
const std::int32_t
    EndpointManager<Platform>::kProcessEndpointDisconnectionTimeoutMillis =
        2000;
template <typename Platform>
const std::int32_t EndpointManager<Platform>::kMaxConcurrentEndpoints = 50;

template <typename Platform>
EndpointManager<Platform>::EndpointManager(
    Ptr<EndpointChannelManager> endpoint_channel_manager)
    : thread_utils_(Platform::createThreadUtils()),
      system_clock_(Platform::createSystemClock()),
      endpoint_channel_manager_(endpoint_channel_manager),
      incoming_offline_frame_processors_(),
      endpoint_keep_alive_manager_thread_pool_(
          Platform::createMultiThreadExecutor(kMaxConcurrentEndpoints)),
      endpoint_readers_thread_pool_(
          Platform::createMultiThreadExecutor(kMaxConcurrentEndpoints)),
      serial_executor_(Platform::createSingleThreadExecutor()) {}

template <typename Platform>
EndpointManager<Platform>::~EndpointManager() {
  // TODO(tracyzhou): Add logging.
  // Stop all the ongoing Runnables (as gracefully as possible).
  serial_executor_->shutdown();
  endpoint_readers_thread_pool_->shutdown();
  endpoint_keep_alive_manager_thread_pool_->shutdown();

  // 'incoming_offline_frame_processors' does not own the processors.
  incoming_offline_frame_processors_.clear();
  // TODO(tracyzhou): Add logging.
}

template <typename Platform>
void EndpointManager<Platform>::registerIncomingOfflineFrameProcessor(
    V1Frame::FrameType frame_type,
    Ptr<typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>
        processor) {
  runOnEndpointManagerThread(MakePtr(
      new endpoint_manager::RegisterIncomingOfflineFrameProcessorRunnable<
          Platform>(self_, frame_type, processor)));
}

template <typename Platform>
void EndpointManager<Platform>::unregisterIncomingOfflineFrameProcessor(
    V1Frame::FrameType frame_type,
    Ptr<typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>
        processor) {
  runOnEndpointManagerThread(MakePtr(
      new endpoint_manager::UnregisterIncomingOfflineFrameProcessorRunnable<
          Platform>(self_, frame_type, processor)));
}

template <typename Platform>
Ptr<typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>
EndpointManager<Platform>::getOfflineFrameProcessor(
    V1Frame::FrameType frame_type) {
  typedef Ptr<typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>
      PtrIncomingOfflineFrameProcessor;
  typedef Ptr<Future<PtrIncomingOfflineFrameProcessor>> ResultType;

  ScopedPtr<ResultType> future_result(
      runOnEndpointManagerThread<PtrIncomingOfflineFrameProcessor>(MakePtr(
          new endpoint_manager::GetOfflineFrameProcessorCallable<Platform>(
              self_, frame_type))));

  return waitForResult("getOfflineFrameProcessor", future_result.get());
}

template <typename Platform>
void EndpointManager<Platform>::registerEndpoint(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id,
    const string& endpoint_name, const string& authentication_token,
    ConstPtr<ByteArray> raw_authentication_token, bool is_incoming,
    Ptr<EndpointChannel> endpoint_channel,
    Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) {
  ScopedPtr<Ptr<CountDownLatch>> latch(Platform::createCountDownLatch(1));
  runOnEndpointManagerThread(
      MakePtr(new endpoint_manager::RegisterEndpointRunnable<Platform>(
          self_, client_proxy, endpoint_id, endpoint_name,
          authentication_token, raw_authentication_token, is_incoming,
          endpoint_channel, connection_lifecycle_listener, latch.get())));
  waitForLatch("registerEndpoint", latch.get());
}

template <typename Platform>
void EndpointManager<Platform>::unregisterEndpoint(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id) {
  ScopedPtr<Ptr<CountDownLatch>> latch(Platform::createCountDownLatch(1));
  runOnEndpointManagerThread(
      MakePtr(new endpoint_manager::UnregisterEndpointRunnable<Platform>(
          self_, client_proxy, endpoint_id, latch.get())));
  waitForLatch("unregisterEndpoint", latch.get());
}

template <typename Platform>
void EndpointManager<Platform>::discardEndpoint(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id) {
  runOnEndpointManagerThread(
      MakePtr(new endpoint_manager::DiscardEndpointRunnable<Platform>(
          self_, client_proxy, endpoint_id)));
}

template <typename Platform>
std::vector<string> EndpointManager<Platform>::sendPayloadChunk(
    const PayloadTransferFrame::PayloadHeader& payload_header,
    const PayloadTransferFrame::PayloadChunk& payload_chunk,
    const std::vector<string>& endpoint_ids) {
  ConstPtr<ByteArray> payload_transfer_frame_bytes =
      OfflineFrames::forDataPayloadTransferFrame(payload_header, payload_chunk);

  return sendTransferFrameBytes(endpoint_ids, payload_transfer_frame_bytes,
                                payload_header.id(),
                                /*offset=*/payload_chunk.offset(),
                                /*packet_type=*/"DATA");
}

template <typename Platform>
void EndpointManager<Platform>::sendControlMessage(
    const PayloadTransferFrame::PayloadHeader& payload_header,
    const PayloadTransferFrame::ControlMessage& control_message,
    const std::vector<string>& endpoint_ids) {
  ConstPtr<ByteArray> payload_transfer_frame_bytes =
      OfflineFrames::forControlPayloadTransferFrame(payload_header,
                                                    control_message);

  sendTransferFrameBytes(endpoint_ids, payload_transfer_frame_bytes,
                         payload_header.id(),
                         /*offset=*/control_message.offset(),
                         /*packet_type=*/"CONTROL");
}

template <typename Platform>
void EndpointManager<Platform>::waitForLatch(const string& method_name,
                                             Ptr<CountDownLatch> latch) {
  Exception::Value await_exception = latch->await();
  if (Exception::NONE != await_exception) {
    if (Exception::INTERRUPTED == await_exception) {
      // TODO(tracyzhou): Add logging.
      // Thread.currentThread().interrupt();
    }
  }
}

template <typename Platform>
void EndpointManager<Platform>::waitForLatch(const string& method_name,
                                             Ptr<CountDownLatch> latch,
                                             std::int32_t timeout_millis) {
  ExceptionOr<bool> await_succeeded = latch->await(timeout_millis);

  if (!await_succeeded.ok()) {
    // TODO(tracyzhou): Add logging.
    if (Exception::INTERRUPTED == await_succeeded.exception()) {
      // TODO(tracyzhou): Add logging.
      // Thread.currentThread().interrupt();
      return;
    }
  }

  if (!await_succeeded.result()) {
    // TODO(tracyzhou): Add logging.
  }
}

template <typename Platform>
template <typename T>
T EndpointManager<Platform>::waitForResult(const string& method_name,
                                           Ptr<Future<T>> result_future) {
  ExceptionOr<T> result = result_future->get();

  if (!result.ok()) {
    Exception::Value exception = result.exception();
    if (Exception::INTERRUPTED == exception ||
        Exception::EXECUTION == exception) {
      // TODO(tracyzhou): Add logging.
      if (Exception::INTERRUPTED == exception) {
        // Thread.currentThread().interrupt();
      }
      return T();
    }
  }

  return result.result();
}

// @EndpointManagerThread
template <typename Platform>
void EndpointManager<Platform>::removeEndpoint(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id,
    bool send_disconnection_notification) {
  // Unregistering from endpoint_channel_manager_ will also serve to terminate
  // the dedicated reader and KeepAlive threads we started when we registered
  // this endpoint.
  if (endpoint_channel_manager_->unregisterChannelForEndpoint(endpoint_id)) {
    // Notify all frame processors of the disconnection immediately and wait
    // for them to clean up state. Only once all processors are done cleaning
    // up, we can remove the endpoint from ClientProxy after which there
    // should be no further interactions with the endpoint.
    // (See b/37352254 for history)
    waitForEndpointDisconnectionProcessing(client_proxy, endpoint_id);

    client_proxy->onDisconnected(endpoint_id, send_disconnection_notification);
    // TODO(tracyzhou): Add logging.
  }
}

// @EndpointManagerThread
template <typename Platform>
void EndpointManager<Platform>::waitForEndpointDisconnectionProcessing(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id) {
  ScopedPtr<Ptr<CountDownLatch>> process_disconnection_barrier(
      Platform::createCountDownLatch(static_cast<std::int32_t>(
          incoming_offline_frame_processors_.size())));

  for (typename IncomingOfflineFrameProcessorsMap::iterator it =
           incoming_offline_frame_processors_.begin();
       it != incoming_offline_frame_processors_.end(); it++) {
    it->second->processEndpointDisconnection(
        client_proxy, endpoint_id, process_disconnection_barrier.get());
  }

  waitForLatch("waitForEndpointDisconnectionProcessing",
               process_disconnection_barrier.get(),
               kProcessEndpointDisconnectionTimeoutMillis);
}

template <typename Platform>
std::vector<string> EndpointManager<Platform>::sendTransferFrameBytes(
    const std::vector<string>& endpoint_ids,
    ConstPtr<ByteArray> payload_transfer_frame_bytes, std::int64_t payload_id,
    std::int64_t offset, const string& packet_type) {
  ScopedPtr<ConstPtr<ByteArray>> scoped_payload_transfer_frame_bytes(
      payload_transfer_frame_bytes);
  std::vector<string> failed_endpoint_ids;
  for (std::vector<string>::const_iterator it = endpoint_ids.begin();
       it != endpoint_ids.end(); it++) {
    const string& endpoint_id = *it;

    ScopedPtr<Ptr<EndpointChannel>> scoped_endpoint_channel(
        endpoint_channel_manager_->getChannelForEndpoint(endpoint_id));

    if (scoped_endpoint_channel.isNull()) {
      // We no longer know about this endpoint (it was either explicitly
      // unregistered, or a read/write error made us unregister it internally).
      // TODO(tracyzhou): Add logging.
      failed_endpoint_ids.push_back(endpoint_id);
      continue;
    }

    Exception::Value write_exception = scoped_endpoint_channel->write(
        scoped_payload_transfer_frame_bytes.release());
    if (Exception::NONE != write_exception) {
      if (Exception::IO == write_exception) {
        // TODO(tracyzhou): Add logging.
        failed_endpoint_ids.push_back(endpoint_id);
        continue;
      }
    }
  }

  return failed_endpoint_ids;
}

template <typename Platform>
void EndpointManager<Platform>::startEndpointReader(Ptr<Runnable> runnable) {
  endpoint_readers_thread_pool_->execute(runnable);
}

template <typename Platform>
void EndpointManager<Platform>::startEndpointKeepAliveManager(
    Ptr<Runnable> runnable) {
  endpoint_keep_alive_manager_thread_pool_->execute(runnable);
}

template <typename Platform>
void EndpointManager<Platform>::runOnEndpointManagerThread(
    Ptr<Runnable> runnable) {
  serial_executor_->execute(runnable);
}

template <typename Platform>
template <typename T>
Ptr<Future<T>> EndpointManager<Platform>::runOnEndpointManagerThread(
    Ptr<Callable<T>> callable) {
  return serial_executor_->submit(callable);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
