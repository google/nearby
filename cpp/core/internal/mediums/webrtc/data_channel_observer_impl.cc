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

#include "core/internal/mediums/webrtc/data_channel_observer_impl.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

DataChannelObserverImpl::DataChannelObserverImpl(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel,
    DataChannelListener* data_channel_listener,
    DataChannelStateChangeCallback callback)
    : data_channel_listener_(data_channel_listener),
      state_change_callback_(std::move(callback)),
      data_channel_{std::move(data_channel)} {
  data_channel_->RegisterObserver(this);
}

DataChannelObserverImpl::~DataChannelObserverImpl() { Disconnect(); }

void DataChannelObserverImpl::OnStateChange() {
  if (data_channel_->state() ==
      webrtc::DataChannelInterface::DataState::kClosed) {
    Disconnect();
  }
  state_change_callback_();
}

void DataChannelObserverImpl::OnMessage(const webrtc::DataBuffer& buffer) {
  data_channel_listener_->data_channel_message_received_cb(
      ByteArray(buffer.data.data<char>(), buffer.size()));
}

void DataChannelObserverImpl::OnBufferedAmountChange(uint64_t sent_data_size) {
  data_channel_listener_->data_channel_buffered_amount_changed_cb();
}

void DataChannelObserverImpl::Disconnect() {
  data_channel_->UnregisterObserver();
  if (data_channel_listener_) {
    data_channel_listener_->data_channel_closed_cb();
    data_channel_listener_ = nullptr;
  }
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
