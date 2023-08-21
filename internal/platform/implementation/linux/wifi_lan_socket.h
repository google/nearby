#ifndef PLATFORM_IMPL_LINUX_WIFI_LAN_SOCKET_H_
#define PLATFORM_IMPL_LINUX_WIFI_LAN_SOCKET_H_

#include <optional>

#include <sdbus-c++/Types.h>

#include "internal/platform/implementation/linux/stream.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace linux {
class WifiLanSocket : public api::WifiLanSocket {
public:
  WifiLanSocket(sdbus::UnixFd fd)
      : fd_(fd), output_stream_(fd), input_stream_(fd) {}
  ~WifiLanSocket() = default;

  nearby::InputStream &GetInputStream() override {
    return input_stream_;
  };
  nearby::OutputStream &GetOutputStream() override {
    return output_stream_;
  };
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
