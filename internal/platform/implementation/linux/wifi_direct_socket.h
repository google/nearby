#ifndef PLATFORM_IMPL_LINUX_WIFI_DIRECT_SOCKET_H_
#define PLATFORM_IMPL_LINUX_WIFI_DIRECT_SOCKET_H_

#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/stream.h"
#include "internal/platform/implementation/wifi_direct.h"

namespace nearby {
namespace linux {
class WifiDirectSocket : public api::WifiDirectSocket {
public:
  WifiDirectSocket(int socket);
  ~WifiDirectSocket() = default;

  InputStream &GetInputStream() override;
  OutputStream &GetOutputStream() override;

  Exception Close() override;

private:
  sdbus::UnixFd fd_;
  OutputStream output_stream_;
  InputStream input_stream_;
};
} // namespace linux
} // namespace nearby

#endif
