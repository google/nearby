#ifndef PLATFORM_IMPL_LINUX_WIFI_LAN_SOCKET_H_
#define PLATFORM_IMPL_LINUX_WIFI_LAN_SOCKET_H_

namespace nearby {
  namespace api {
    class WifiLanSocket {
    public:
      ~WifiLanSocket() = default;

    private:
      int fd;
    };
  }
}

#endif
