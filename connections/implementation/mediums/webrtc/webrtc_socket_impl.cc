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

#include "connections/implementation/mediums/webrtc/webrtc_socket_impl.h"

#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {
namespace connections {
namespace mediums {

// OutputStreamImpl
Exception WebRtcSocket::OutputStreamImpl::Write(const ByteArray& data) {
  if (data.size() > kMaxDataSize) {
    NEARBY_LOG(WARNING, "Sending data larger than 1MB");
    return {Exception::kIo};
  }

  socket_->BlockUntilSufficientSpaceInBuffer(data.size());

  if (socket_->IsClosed()) {
    NEARBY_LOG(WARNING, "Tried sending message while socket is closed");
    return {Exception::kIo};
  }

  if (!socket_->SendMessage(data)) {
    NEARBY_LOG(INFO, "Unable to write data to socket.");
    return {Exception::kIo};
  }
  return {Exception::kSuccess};
}

Exception WebRtcSocket::OutputStreamImpl::Flush() {
  // Java implementation is empty.
  return {Exception::kSuccess};
}

Exception WebRtcSocket::OutputStreamImpl::Close() {
  socket_->Close();
  return {Exception::kSuccess};
}

// WebRtcSocket
WebRtcSocket::WebRtcSocket(
    const std::string& name,
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
    : name_(name), data_channel_(std::move(data_channel)) {
  NEARBY_LOGS(INFO) << "WebRtcSocket::WebRtcSocket(" << name_
                    << ") this: " << this;
  data_channel_->RegisterObserver(this);
}

WebRtcSocket::~WebRtcSocket() {
  NEARBY_LOGS(INFO) << "WebRtcSocket::~WebRtcSocket(" << name_
                    << ") this: " << this;
  Close();
  NEARBY_LOGS(INFO) << "WebRtcSocket::~WebRtcSocket(" << name_
                    << ") this: " << this << " done";
}

InputStream& WebRtcSocket::GetInputStream() { return pipe_.GetInputStream(); }

OutputStream& WebRtcSocket::GetOutputStream() { return output_stream_; }

void WebRtcSocket::Close() {
  NEARBY_LOGS(INFO) << "WebRtcSocket::Close(" << name_ << ") this: " << this;
  if (closed_.Set(true)) return;

  ClosePipe();
  // NOTE: This call blocks and triggers a state change on the siginaling thread
  // to 'closing' but does not block until 'closed' is sent so the data channel
  // is not fully closed when this call is done.
  data_channel_->Close();
  NEARBY_LOGS(INFO) << "WebRtcSocket::Close(" << name_ << ") this: " << this
                    << " done";
}

void WebRtcSocket::OnStateChange() {
  // Running on the signaling thread right now.
  NEARBY_LOGS(ERROR)
      << "WebRtcSocket::OnStateChange() webrtc data channel state: "
      << webrtc::DataChannelInterface::DataStateString(data_channel_->state());
  switch (data_channel_->state()) {
    case webrtc::DataChannelInterface::DataState::kConnecting:
      break;
    case webrtc::DataChannelInterface::DataState::kOpen:
      // We implicitly depend on the |socket_listener_| to offload from
      // the signaling thread so it does not get blocked.
      socket_listener_.socket_ready_cb(this);
      break;
    case webrtc::DataChannelInterface::DataState::kClosing:
      break;
    case webrtc::DataChannelInterface::DataState::kClosed:
      NEARBY_LOG(
          ERROR,
          "WebRtcSocket::OnStateChange() unregistering data channel observer.");
      data_channel_->UnregisterObserver();
      // This will trigger a destruction of the owning connection flow
      // We implicitly depend on the |socket_listener_| to offload from
      // the signaling thread so it does not get blocked.
      socket_listener_.socket_closed_cb(this);

      if (!closed_.Set(true)) {
        OffloadFromSignalingThread([this] { ClosePipe(); });
      }
      break;
  }
}
void WebRtcSocket::OnMessage(const webrtc::DataBuffer& buffer) {
  // This is a data channel callback on the signaling thread, lets off load so
  // we don't block signaling.
  OffloadFromSignalingThread(
      [this, buffer = ByteArray(buffer.data.data<char>(), buffer.size())] {
        if (!pipe_.GetOutputStream().Write(buffer).Ok()) {
          Close();
          return;
        }

        if (!pipe_.GetOutputStream().Flush().Ok()) {
          Close();
        }
      });
}

void WebRtcSocket::OnBufferedAmountChange(uint64_t sent_data_size) {
  // This is a data channel callback on the signaling thread, lets off load so
  // we don't block signaling.
  OffloadFromSignalingThread([this] { WakeUpWriter(); });
}

bool WebRtcSocket::SendMessage(const ByteArray& data) {
  return data_channel_->Send(
      webrtc::DataBuffer(std::string(data.data(), data.size())));
}

bool WebRtcSocket::IsClosed() { return closed_.Get(); }

void WebRtcSocket::ClosePipe() {
  NEARBY_LOGS(INFO) << "WebRtcSocket::ClosePipe(" << name_
                    << ") this: " << this;
  // This is thread-safe to close these sockets even if a read or write is in
  // process on another thread, Close will wait for the exclusive mutex before
  // setting state.
  pipe_.GetInputStream().Close();
  pipe_.GetOutputStream().Close();
  WakeUpWriter();
  NEARBY_LOGS(INFO) << "WebRtcSocket::ClosePipe(" << name_ << ") this: " << this
                    << " done";
}

// Must not be called on signalling thread.
void WebRtcSocket::WakeUpWriter() {
  MutexLock lock(&backpressure_mutex_);
  buffer_variable_.Notify();
}

void WebRtcSocket::SetSocketListener(SocketListener&& listener) {
  socket_listener_ = std::move(listener);
}

void WebRtcSocket::BlockUntilSufficientSpaceInBuffer(int length) {
  MutexLock lock(&backpressure_mutex_);
  while (!IsClosed() &&
         (data_channel_->buffered_amount() + length > kMaxDataSize)) {
    // TODO(himanshujaju): Add wait with timeout.
    buffer_variable_.Wait();
  }
}

void WebRtcSocket::OffloadFromSignalingThread(Runnable runnable) {
  single_thread_executor_.Execute(std::move(runnable));
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
