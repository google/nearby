#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_DATA_CHANNEL_LISTENER_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_DATA_CHANNEL_LISTENER_H_

#include "core/listeners.h"
#include "platform/base/byte_array.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// Callbacks from the data channel.
struct DataChannelListener {
  std::function<void()> data_channel_closed_cb = DefaultCallback<>();

  // Called when a new message was received on the data channel.
  std::function<void(const ByteArray&)> data_channel_message_received_cb =
      DefaultCallback<const ByteArray&>();

  // Called when the data channel indicates that the buffered amount has
  // changed.
  std::function<void()> data_channel_buffered_amount_changed_cb =
      DefaultCallback<>();
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_DATA_CHANNEL_LISTENER_H_
