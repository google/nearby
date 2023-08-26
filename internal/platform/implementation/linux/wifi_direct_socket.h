#ifndef PLATFORM_IMPL_LINUX_WIFI_DIRECT_SOCKET_H_
#define PLATFORM_IMPL_LINUX_WIFI_DIRECT_SOCKET_H_

#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/stream.h"
#include "internal/platform/implementation/wifi_direct.h"

namespace nearby {
namespace linux {
class WifiDirectSocket : public api::WifiDirectSocket {
public:
  WifiDirectSocket(int socket)
      : fd_(sdbus::UnixFd(socket)), output_stream_(fd_), input_stream_(fd_) {}
  ~WifiDirectSocket() = default;

  InputStream &GetInputStream() override { return input_stream_; };
  OutputStream &GetOutputStream() override { return output_stream_; };

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
