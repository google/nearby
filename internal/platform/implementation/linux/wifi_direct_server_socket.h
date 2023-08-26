#ifndef PLATFORM_IMPL_LINUX_WIFI_DIRECT_SERVER_SOCKET_H_
#define PLATFORM_IMPL_LINUX_WIFI_DIRECT_SERVER_SOCKET_H_

#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi_direct.h"
#include <sdbus-c++/IConnection.h>
namespace nearby {
  namespace linux {
  class NetworkManagerWifiDirectServerSocket
      : public api::WifiDirectServerSocket {
public:    
      NetworkManagerWifiDirectServerSocket(int socket, sdbus::IConnection &system_bus,
                             const sdbus::ObjectPath &active_connection_path,
                             std::shared_ptr<NetworkManager> network_manager) : fd_(socket), system_bus_(system_bus), active_connection_path_(active_connection_path), network_manager_(network_manager) {}
      ~NetworkManagerWifiDirectServerSocket() {}

      std::string GetIPAddress() const override;
      int GetPort() const override;
      std::unique_ptr<api::WifiDirectSocket> Accept() override;
      Exception Close() override;
private:
  sdbus::UnixFd fd_;
  sdbus::IConnection &system_bus_;
  sdbus::ObjectPath active_connection_path_;
  std::shared_ptr<NetworkManager> network_manager_ ;     
    };
  }
}

#endif
