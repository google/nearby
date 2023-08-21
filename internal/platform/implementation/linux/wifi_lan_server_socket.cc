#include <cerrno>
#include <cstring>
#include <ifaddrs.h>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <sdbus-c++/Types.h>

#include "internal/platform/implementation/linux/wifi_lan_server_socket.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/wifi_lan_socket.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
std::string WifiLanServerSocket::GetIPAddress() const {
  struct ifaddrs *addrs = nullptr;
  getifaddrs(&addrs);

  for (auto ifaddr = addrs; ifaddr != NULL; ifaddr = ifaddr->ifa_next) {
    if (ifaddr->ifa_addr == nullptr) {
      continue;
    }
    if (ifaddr->ifa_addr->sa_family == AF_INET) {
      auto addr =
          &(reinterpret_cast<struct sockaddr_in *>(ifaddr->ifa_addr))->sin_addr;
      char buf[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, addr, buf, INET_ADDRSTRLEN);

      return std::string(buf);
    }
  }

  return std::string();
}

int WifiLanServerSocket::GetPort() const {
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

std::unique_ptr<api::WifiLanSocket> WifiLanServerSocket::Accept() {
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

  return std::make_unique<WifiLanSocket>(sdbus::UnixFd(conn));
}

Exception WifiLanServerSocket::Close() {
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
