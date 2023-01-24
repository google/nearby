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

#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_SOCKET_IMPL_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_SOCKET_IMPL_H_

#include <memory>

#include "connections/listeners.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/mutex.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/pipe.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/socket.h"
#include "webrtc/api/data_channel_interface.h"

namespace nearby {
namespace connections {
namespace mediums {

// Maximum data size: 1 MB
constexpr int kMaxDataSize = 1 * 1024 * 1024;

// Defines the Socket implementation specific to WebRTC, which uses the WebRTC
// data channel to send and receive messages.
//
// Messages are buffered here to prevent the data channel from overflowing,
// which could lead to data loss.
class WebRtcSocket : public Socket, public webrtc::DataChannelObserver {
 public:
  WebRtcSocket(const std::string& name,
               rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);
  ~WebRtcSocket() override;

  WebRtcSocket(const WebRtcSocket& other) = delete;
  WebRtcSocket& operator=(const WebRtcSocket& other) = delete;

  // Overrides for nearby::Socket:
  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  void Close() override;

  // webrtc::DataChannelObserver:
  void OnStateChange() override;
  void OnMessage(const webrtc::DataBuffer& buffer) override;
  void OnBufferedAmountChange(uint64_t sent_data_size) override;

  // Listener class the gets called when the socket is ready or closed
  struct SocketListener {
    absl::AnyInvocable<void(WebRtcSocket*)> socket_ready_cb =
        DefaultCallback<WebRtcSocket*>();
    absl::AnyInvocable<void(WebRtcSocket*)> socket_closed_cb =
        DefaultCallback<WebRtcSocket*>();
  };

  void SetSocketListener(SocketListener&& listener);

 private:
  class OutputStreamImpl : public OutputStream {
   public:
    explicit OutputStreamImpl(WebRtcSocket* const socket) : socket_(socket) {}
    ~OutputStreamImpl() override = default;

    OutputStreamImpl(const OutputStreamImpl& other) = delete;
    OutputStreamImpl& operator=(const OutputStreamImpl& other) = delete;

    // OutputStream:
    Exception Write(const ByteArray& data) override;
    Exception Flush() override;
    Exception Close() override;

   private:
    // |this| OutputStreamImpl is owned by |socket_|.
    WebRtcSocket* const socket_;
  };

  void WakeUpWriter();
  bool IsClosed();
  void ClosePipe();
  bool SendMessage(const ByteArray& data);
  void BlockUntilSufficientSpaceInBuffer(int length);
  void OffloadFromSignalingThread(Runnable runnable);

  std::string name_;
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;

  Pipe pipe_;

  OutputStreamImpl output_stream_{this};

  AtomicBoolean closed_{false};

  SocketListener socket_listener_;

  mutable Mutex backpressure_mutex_;
  ConditionVariable buffer_variable_{&backpressure_mutex_};

  // This should be destroyed first to ensure any remaining tasks flushed on
  // shutdown get run while the other members are still alive.
  SingleThreadExecutor single_thread_executor_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_SOCKET_IMPL_H_
