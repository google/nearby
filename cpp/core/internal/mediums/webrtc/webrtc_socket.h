#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_SOCKET_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_SOCKET_H_

#include "platform/api/atomic_boolean.h"
#include "platform/api/input_stream.h"
#include "platform/api/output_stream.h"
#include "platform/api/socket.h"
#include "platform/pipe.h"
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
template <typename Platform>
class WebRtcSocket : public Socket {
 public:
  WebRtcSocket(const string& name,
               rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);
  ~WebRtcSocket() override = default;

  WebRtcSocket(const WebRtcSocket& other) = delete;
  WebRtcSocket& operator=(const WebRtcSocket& other) = delete;

  // Overrides for location::nearby::Socket:
  Ptr<InputStream> getInputStream() override;
  Ptr<OutputStream> getOutputStream() override;
  void close() override;

  // Callback from WebRTC data channel when new message has been received from
  // the remote.
  void NotifyDataChannelMsgReceived(ConstPtr<ByteArray> message);

  // Callback from WebRTC data channel that the buffered data amount has
  // changed.
  void NotifyDataChannelBufferedAmountChanged();

  // Listener class the gets called when the socket is closed.
  class SocketClosedListener {
   public:
    virtual ~SocketClosedListener() = default;
    virtual void OnSocketClosed() = 0;
  };
  void SetOnSocketClosedListener(Ptr<SocketClosedListener> listener);

 private:
  class OutputStreamImpl : public OutputStream {
   public:
    explicit OutputStreamImpl(WebRtcSocket<Platform>* const socket)
        : socket_(socket) {}
    ~OutputStreamImpl() override = default;

    OutputStreamImpl(const OutputStreamImpl& other) = delete;
    OutputStreamImpl& operator=(const OutputStreamImpl& other) = delete;

    // OutputStream:
    Exception::Value write(ConstPtr<ByteArray> data) override;
    Exception::Value flush() override;
    Exception::Value close() override;

   private:
    // |this| OutputStreamImpl is owned by |socket_|.
    WebRtcSocket<Platform>* const socket_;
  };

  void WakeUpWriter();
  bool IsClosed();
  bool SendMessage(ConstPtr<ByteArray> data);
  void BlockUntilSufficientSpaceInBuffer(int length);

  string name_;
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;

  Ptr<Pipe> pipe_;
  ScopedPtr<Ptr<InputStream>> incoming_data_piped_input_stream_;
  ScopedPtr<Ptr<OutputStream>> incoming_data_piped_output_stream_;

  ScopedPtr<Ptr<OutputStream>> output_stream_;

  ScopedPtr<Ptr<AtomicBoolean>> closed_;

  Ptr<SocketClosedListener> socket_closed_listener_;

  ScopedPtr<Ptr<Lock>> backpressure_lock_;
  ScopedPtr<Ptr<ConditionVariable>> buffer_variable_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/mediums/webrtc/webrtc_socket.cc"

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_SOCKET_H_
