#ifndef PLATFORM_IMPL_LINUX_WIFI_HOTSPOT_SOCKET_H_
#define PLATFORM_IMPL_LINUX_WIFI_HOTSPOT_SOCKET_H_

#include "internal/platform/implementation/linux/stream.h"
#include "internal/platform/implementation/wifi_hotspot.h"

namespace nearby {
namespace linux {
class WifiHotspotSocket : public api::WifiHotspotSocket {
public:  
  WifiHotspotSocket(int connection_fd)
      : fd_(sdbus::UnixFd(connection_fd)), output_stream_(fd_),
        input_stream_(fd_) {}
  ~WifiHotspotSocket() {}

  nearby::InputStream &GetInputStream() override { return input_stream_; };
  nearby::OutputStream &GetOutputStream() override { return output_stream_; };
  Exception Close() override {
    input_stream_.Close();
    output_stream_.Close();

    return Exception{Exception::kSuccess};
  };

private:
  sdbus::UnixFd fd_;
  OutputStream output_stream_;
  InputStream input_stream_;
};
} // namespace linux
} // namespace nearby

#endif
