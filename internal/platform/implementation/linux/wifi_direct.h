#ifndef PLATFORM_IMPL_LINUX_WIFI_DIRECT_H_
#define PLATFORM_IMPL_LINUX_WIFI_DIRECT_H_
#include <memory>

#include <optional>
#include <sdbus-c++/IConnection.h>

#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi_direct.h"

namespace nearby {
namespace linux {
class NetworkManagerWifiDirectMedium : public api::WifiDirectMedium {
public:  
  NetworkManagerWifiDirectMedium(
      sdbus::IConnection &system_bus,
      std::shared_ptr<NetworkManager> network_manager,
      std::unique_ptr<NetworkManagerWifiMedium> wireless_device)
      : system_bus_(system_bus), network_manager_(network_manager),
        wireless_device_(std::move(wireless_device)) {}
  ~NetworkManagerWifiDirectMedium() {}

  bool IsInterfaceValid() const override { return true; }
  std::unique_ptr<api::WifiDirectSocket>
  ConnectToService(absl::string_view ip_address, int port,
                   CancellationFlag *cancellation_flag) override;
  std::unique_ptr<api::WifiDirectServerSocket>
  ListenForService(int port) override;
  bool
  ConnectWifiDirect(WifiDirectCredentials *wifi_direct_credentials) override;
  bool DisconnectWifiDirect() override;

  bool StartWifiDirect(WifiDirectCredentials *wifi_direct_credentials) override;
  bool StopWifiDirect() override;
  
  absl::optional<std::pair<std::int32_t, std::int32_t>>
  GetDynamicPortRange() override {
    return std::nullopt;
  }

  sdbus::IConnection &system_bus_;
  std::shared_ptr<NetworkManager> network_manager_;
  std::unique_ptr<NetworkManagerWifiMedium> wireless_device_;
};
} // namespace linux
} // namespace nearby

#endif
