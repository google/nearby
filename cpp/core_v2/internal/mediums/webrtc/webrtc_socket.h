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

#ifndef CORE_V2_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_SOCKET_H_
#define CORE_V2_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_SOCKET_H_

#include <memory>

#include "core_v2/listeners.h"
#include "platform_v2/base/input_stream.h"
#include "platform_v2/base/output_stream.h"
#include "platform_v2/base/socket.h"
#include "platform_v2/public/atomic_boolean.h"
#include "platform_v2/public/condition_variable.h"
#include "platform_v2/public/mutex.h"
#include "platform_v2/public/pipe.h"
#include "webrtc/api/data_channel_interface.h"
namespace location {
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
class WebRtcSocket : public Socket {
 public:
  WebRtcSocket(const std::string& name,
               rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);
  ~WebRtcSocket() override = default;

  WebRtcSocket(const WebRtcSocket& other) = delete;
  WebRtcSocket& operator=(const WebRtcSocket& other) = delete;

  // Overrides for location::nearby::Socket:
  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  void Close() override;

  // Callback from WebRTC data channel when new message has been received from
  // the remote.
  void NotifyDataChannelMsgReceived(const ByteArray& message);

  // Callback from WebRTC data channel that the buffered data amount has
  // changed.
  void NotifyDataChannelBufferedAmountChanged();

  // Listener class the gets called when the socket is closed.
  struct SocketClosedListener {
    std::function<void()> socket_closed_cb = DefaultCallback<>();
  };

  void SetOnSocketClosedListener(SocketClosedListener&& listener);

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
  bool SendMessage(const ByteArray& data);
  void BlockUntilSufficientSpaceInBuffer(int length);

  std::string name_;
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;

  Pipe pipe_;

  OutputStreamImpl output_stream_{this};

  AtomicBoolean closed_{false};

  SocketClosedListener socket_closed_listener_;

  mutable Mutex backpressure_mutex_;
  ConditionVariable buffer_variable_{&backpressure_mutex_};
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_SOCKET_H_
