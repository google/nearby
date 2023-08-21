#ifndef PLATFORM_IMPL_LINUX_WIFI_LAN_SERVER_SOCKET_H_
#define PLATFORM_IMPL_LINUX_WIFI_LAN_SERVER_SOCKET_H_

#include <netinet/in.h>

#include <sdbus-c++/Types.h>

#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_lan.h"

namespace nearby {
namespace linux {
class WifiLanServerSocket : public api::WifiLanServerSocket {
public:
  WifiLanServerSocket(int socket) {
    fd_ = sdbus::UnixFd(socket);
  }
  
  ~WifiLanServerSocket() override = default;

  std::string GetIPAddress() const override;

  int GetPort() const override;

  std::unique_ptr<api::WifiLanSocket> Accept() override;

  Exception Close() override;

  sdbus::UnixFd fd_;
};
} // namespace linux
} // namespace nearby
#endif
