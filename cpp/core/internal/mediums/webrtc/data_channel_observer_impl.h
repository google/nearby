#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_DATA_CHANNEL_OBSERVER_IMPL_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_DATA_CHANNEL_OBSERVER_IMPL_H_

#include "core/internal/mediums/webrtc/data_channel_listener.h"
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

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_DATA_CHANNEL_OBSERVER_IMPL_H_
