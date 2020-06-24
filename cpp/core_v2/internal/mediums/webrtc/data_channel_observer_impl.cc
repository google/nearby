#include "core_v2/internal/mediums/webrtc/data_channel_observer_impl.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

DataChannelObserverImpl::DataChannelObserverImpl(
    DataChannelListener* data_channel_listener,
    DataChannelStateChangeCallback callback)
    : data_channel_listener_(data_channel_listener),
      state_change_callback_(std::move(callback)) {}

void DataChannelObserverImpl::OnStateChange() { state_change_callback_(); }

void DataChannelObserverImpl::OnMessage(const webrtc::DataBuffer& buffer) {
  data_channel_listener_->data_channel_message_received_cb(
      ByteArray(buffer.data.data<char>(), buffer.size()));
}

void DataChannelObserverImpl::OnBufferedAmountChange(uint64_t sent_data_size) {
  data_channel_listener_->data_channel_buffered_amount_changed_cb();
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
