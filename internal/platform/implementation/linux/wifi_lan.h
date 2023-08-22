#ifndef PLATFORM_IMPL_LINUX_WIFI_LAN_H_
#define PLATFORM_IMPL_LINUX_WIFI_LAN_H_
#include <memory>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "internal/platform/implementation/linux/avahi.h"
#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/nsd_service_info.h"

namespace nearby {
namespace linux {
class WifiLanMedium : public api::WifiLanMedium {
public:
  WifiLanMedium(sdbus::IConnection &system_bus);
  ~WifiLanMedium() override;

  bool IsNetworkConnected() const override;
  bool StartAdvertising(const NsdServiceInfo &nsd_service_info) override;
  bool StopAdvertising(const NsdServiceInfo &nsd_service_info) override;
  bool StartDiscovery(const std::string &service_type,
                      DiscoveredServiceCallback callback) override;
  bool StopDiscovery(const std::string &service_type) override;
  std::unique_ptr<api::WifiLanSocket>
  ConnectToService(const NsdServiceInfo &remote_service_info,
                   CancellationFlag *cancellation_flag) override {
    return ConnectToService(remote_service_info.GetIPAddress(),
                            remote_service_info.GetPort(), cancellation_flag);
  };
  std::unique_ptr<api::WifiLanSocket>
  ConnectToService(const std::string &ip_address, int port,
                   CancellationFlag *cancellation_flag) override;
  std::unique_ptr<api::WifiLanServerSocket>
  ListenForService(int port = 0) override;
  absl::optional<std::pair<std::int32_t, std::int32_t>>
  GetDynamicPortRange() override;

private:
  DiscoveredServiceCallback discovery_cb_;

  sdbus::IConnection &system_bus_;

  std::shared_ptr<NetworkManager> network_manager_;

  std::shared_ptr<avahi::Server> avahi_;
  std::unique_ptr<avahi::EntryGroup> entry_group_;

  absl::flat_hash_map<std::string, std::unique_ptr<avahi::ServiceBrowser>>
      service_browsers_;

  bool advertising_;
};
} // namespace linux
} // namespace nearby

#endif
