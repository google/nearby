#ifndef PLATFORM_IMPL_LINUX_WIFI_LAN_SERVER_SOCKET_H_
#define PLATFORM_IMPL_LINUX_WIFI_LAN_SERVER_SOCKET_H_

#include <netinet/in.h>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/Types.h>

#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi_lan.h"

namespace nearby {
namespace linux {
class WifiLanServerSocket : public api::WifiLanServerSocket {
public:
  WifiLanServerSocket(int socket,
                      std::shared_ptr<NetworkManager> network_manager,
                      sdbus::IConnection &system_bus)
      : fd_(sdbus::UnixFd(socket)), network_manager_(network_manager),
        system_bus_(system_bus) {}
  ~WifiLanServerSocket() override = default;

  std::string GetIPAddress() const override;
  int GetPort() const override;

  std::unique_ptr<api::WifiLanSocket> Accept() override;
  Exception Close() override;

private:
  sdbus::UnixFd fd_;
  std::shared_ptr<NetworkManager> network_manager_;
  sdbus::IConnection &system_bus_;
};
} // namespace linux
} // namespace nearby
#endif
