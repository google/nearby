#ifndef PLATFORM_IMPL_LINUX_WIFI_HOTSPOT_H_
#define PLATFORM_IMPL_LINUX_WIFI_HOTSPOT_H_

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/Types.h>

#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi_hotspot.h"

namespace nearby {
  namespace linux {
    class NetworkManagerWifiHotspotMedium : api::WifiHotspotMedium {
    public:
      NetworkManagerWifiHotspotMedium(
          sdbus::IConnection &system_bus,
          std::shared_ptr<NetworkManager> network_manager,
          const sdbus::ObjectPath &wireless_device_object_path)
          : system_bus_(system_bus),
            wireless_device_(std::make_unique<NetworkManagerWifiMedium>(
                network_manager, system_bus, wireless_device_object_path)),
            network_manager_(network_manager) {}
      ~NetworkManagerWifiHotspotMedium() {}

      bool IsInterfaceValid() const override { return true; }
      std::unique_ptr<api::WifiHotspotSocket>
      ConnectToService(absl::string_view ip_address, int port,
                       CancellationFlag *cancellation_flag) override;
      std::unique_ptr<api::WifiHotspotServerSocket>
      ListenForService(int port) override;

      bool StartWifiHotspot(HotspotCredentials *hotspot_credentials) override;
      bool StopWifiHotspot() override;

      bool ConnectWifiHotspot(HotspotCredentials *hotspot_credentials) override;
      bool DisconnectWifiHotspot() override;

      absl::optional<std::pair<std::int32_t, std::int32_t>>
      GetDynamicPortRange() override {return absl::nullopt;}

    private:
      bool WifiHotspotActive();
      
      sdbus::IConnection &system_bus_;
      std::unique_ptr<NetworkManagerWifiMedium> wireless_device_;
      std::shared_ptr<NetworkManager> network_manager_;
    };
  }
}

#endif
