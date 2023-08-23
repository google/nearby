#include <netinet/in.h>
#include <sys/socket.h>

#include "internal/platform/implementation/linux/wifi_hotspot_server_socket.h"
#include "internal/platform/implementation/linux/wifi_hotspot_socket.h"
#include "internal/platform/implementation/linux/wifi_medium.h"

namespace nearby {
namespace linux {
std::string NetworkManagerWifiHotspotServerSocket::GetIPAddress() const {
  NetworkManagerActiveConnection active_conn(system_bus_,
                                             active_connection_path_);
  auto ip4addresses = active_conn.GetIP4Addresses();
  if (ip4addresses.empty()) {
  NEARBY_LOGS(ERROR)
      << __func__
      << ": Could not find any IPv4 addresses for active connection "
      << active_connection_path_;
  return std::string();
  }
  return ip4addresses[0];
}

int NetworkManagerWifiHotspotServerSocket::GetPort() const {
  struct sockaddr_in sin;
  socklen_t len = sizeof(sin);
  auto ret =
      getsockname(fd_.get(), reinterpret_cast<struct sockaddr *>(&sin), &len);
  if (ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error getting information for socket "
                       << fd_.get() << ": " << std::strerror(errno);
    return 0;
  }

  return ntohs(sin.sin_port);
}

std::unique_ptr<api::WifiHotspotSocket>
NetworkManagerWifiHotspotServerSocket::Accept() {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);

  auto conn =
      accept(fd_.get(), reinterpret_cast<struct sockaddr *>(&addr), &len);
  if (conn < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error accepting incoming connections on socket "
                       << fd_.get() << ": " << std::strerror(errno);
    return nullptr;
  }

  return std::make_unique<WifiHotspotSocket>(conn);
}

Exception NetworkManagerWifiHotspotServerSocket::Close() {
  int fd = fd_.release();
  auto ret = close(fd);
  if (ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error closing socket " << fd << ": "
                       << std::strerror(errno);
    return {Exception::kFailed};
  }

  return {Exception::kSuccess};
}
} // namespace linux
} // namespace nearby
