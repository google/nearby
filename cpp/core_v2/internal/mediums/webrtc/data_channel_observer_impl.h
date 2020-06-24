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

#ifndef CORE_V2_INTERNAL_MEDIUMS_WEBRTC_DATA_CHANNEL_OBSERVER_IMPL_H_
#define CORE_V2_INTERNAL_MEDIUMS_WEBRTC_DATA_CHANNEL_OBSERVER_IMPL_H_

#include "core_v2/internal/mediums/webrtc/data_channel_listener.h"
#include "webrtc/api/data_channel_interface.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

class DataChannelObserverImpl : public webrtc::DataChannelObserver {
 public:
  using DataChannelStateChangeCallback = std::function<void()>;

  ~DataChannelObserverImpl() override = default;
  DataChannelObserverImpl(DataChannelListener* data_channel_listener,
                          DataChannelStateChangeCallback callback);

  // webrtc::DataChannelObserver:
  void OnStateChange() override;
  void OnMessage(const webrtc::DataBuffer& buffer) override;
  void OnBufferedAmountChange(uint64_t sent_data_size) override;

 private:
  DataChannelListener* data_channel_listener_;
  DataChannelStateChangeCallback state_change_callback_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_WEBRTC_DATA_CHANNEL_OBSERVER_IMPL_H_
