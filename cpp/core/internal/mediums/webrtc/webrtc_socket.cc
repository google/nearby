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

#include "core/internal/mediums/webrtc/webrtc_socket.h"

#include "platform/synchronized.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// OutputStreamImpl
template <typename Platform>
Exception::Value WebRtcSocket<Platform>::OutputStreamImpl::write(
    ConstPtr<ByteArray> data) {
  ScopedPtr<ConstPtr<ByteArray>> scoped_data(data);

  if (scoped_data->size() > kMaxDataSize) {
    NEARBY_LOG(WARNING, "Sending data larger than 1MB");
    return Exception::IO;
  }

  socket_->BlockUntilSufficientSpaceInBuffer(scoped_data->size());

  if (socket_->IsClosed()) {
    NEARBY_LOG(WARNING, "Tried sending message while socket is closed");
    return Exception::IO;
  }

  if (!socket_->SendMessage(scoped_data.release())) {
    return Exception::IO;
  }
  return Exception::NONE;
}

template <typename Platform>
Exception::Value WebRtcSocket<Platform>::OutputStreamImpl::flush() {
  // Java implementation is empty.
  return Exception::NONE;
}

template <typename Platform>
Exception::Value WebRtcSocket<Platform>::OutputStreamImpl::close() {
  socket_->close();
  return Exception::NONE;
}

// WebRtcSocket
template <typename Platform>
WebRtcSocket<Platform>::WebRtcSocket(
    const std::string& name,
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
    : name_(name),
      data_channel_(std::move(data_channel)),
      pipe_(MakeRefCountedPtr(new Pipe())),
      incoming_data_piped_input_stream_(Pipe::createInputStream(pipe_)),
      incoming_data_piped_output_stream_(Pipe::createOutputStream(pipe_)),
      output_stream_(MakePtr(new OutputStreamImpl(this))),
      closed_(Platform::createAtomicBoolean(false)),
      backpressure_lock_(Platform::createLock()),
      buffer_variable_(
          Platform::createConditionVariable(backpressure_lock_.get())) {}

template <typename Platform>
Ptr<InputStream> WebRtcSocket<Platform>::getInputStream() {
  return incoming_data_piped_input_stream_.get();
}

template <typename Platform>
Ptr<OutputStream> WebRtcSocket<Platform>::getOutputStream() {
  return output_stream_.get();
}

template <typename Platform>
void WebRtcSocket<Platform>::close() {
  if (IsClosed()) return;

  closed_->set(true);
  incoming_data_piped_output_stream_->close();
  incoming_data_piped_input_stream_->close();
  data_channel_->Close();
  WakeUpWriter();
  if (!socket_closed_listener_.isNull()) {
    socket_closed_listener_->OnSocketClosed();
  }
}

template <typename Platform>
void WebRtcSocket<Platform>::NotifyDataChannelMsgReceived(
    ConstPtr<ByteArray> message) {
  Exception::Value exception =
      incoming_data_piped_output_stream_->write(message);
  if (exception != Exception::NONE) close();

  exception = incoming_data_piped_output_stream_->flush();
  if (exception != Exception::NONE) close();
}

template <typename Platform>
void WebRtcSocket<Platform>::NotifyDataChannelBufferedAmountChanged() {
  WakeUpWriter();
}

template <typename Platform>
bool WebRtcSocket<Platform>::SendMessage(ConstPtr<ByteArray> data) {
  ScopedPtr<ConstPtr<ByteArray>> scoped_data(data);
  return data_channel_->Send(webrtc::DataBuffer(
      std::string(scoped_data->getData(), scoped_data->size())));
}

template <typename Platform>
bool WebRtcSocket<Platform>::IsClosed() {
  return closed_->get();
}

template <typename Platform>
void WebRtcSocket<Platform>::WakeUpWriter() {
  Synchronized s(backpressure_lock_.get());
  buffer_variable_->notify();
}

template <typename Platform>
void WebRtcSocket<Platform>::SetOnSocketClosedListener(
    Ptr<SocketClosedListener> listener) {
  socket_closed_listener_ = listener;
}

template <typename Platform>
void WebRtcSocket<Platform>::BlockUntilSufficientSpaceInBuffer(int length) {
  Synchronized s(backpressure_lock_.get());
  while (!IsClosed() &&
         (data_channel_->buffered_amount() + length > kMaxDataSize)) {
    // TODO(himanshujaju): Add wait with timeout.
    buffer_variable_->wait();
  }
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
