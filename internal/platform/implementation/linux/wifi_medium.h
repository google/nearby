#ifndef PLATFORM_IMPL_LINUX_WIFI_MEDIUM_H_
#define PLATFORM_IMPL_LINUX_WIFI_MEDIUM_H_

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/networkmanager_accesspoint_client_glue.h"
#include "internal/platform/implementation/linux/networkmanager_client_glue.h"
#include "internal/platform/implementation/linux/networkmanager_device_wireless_client_glue.h"
#include "internal/platform/implementation/linux/networkmanager_ip4config_client_glue.h"
#include "internal/platform/implementation/wifi.h"

namespace nearby {
namespace linux {
class NetworkManager
    : public sdbus::ProxyInterfaces<org::freedesktop::NetworkManager_proxy> {
public:
  NetworkManager(sdbus::IConnection &system_bus)
      : ProxyInterfaces(system_bus, "org.freedesktop.NetworkManager",
                        "/org/freedesktop/NetworkManager") {
    registerProxy();
  }
  ~NetworkManager() { unregisterProxy(); }

  std::uint32_t getState() const { return state_; }

protected:
  void onCheckPermissions() override {}
  void onStateChanged(const uint32_t &state) override { state_ = state; }
  void onDeviceAdded(const sdbus::ObjectPath &device_path) override {}
  void onDeviceRemoved(const sdbus::ObjectPath &device_path) override {}

private:
  std::atomic_uint32_t state_;
};

class NetworkManagerIP4Config
    : public sdbus::ProxyInterfaces<
          org::freedesktop::NetworkManager::IP4Config_proxy> {
public:
  NetworkManagerIP4Config(sdbus::IConnection &system_bus,
                          const sdbus::ObjectPath &config_object_path)
      : ProxyInterfaces(system_bus, "org.freedesktop.NetworkManager",
                        config_object_path) {
    registerProxy();
  }
  ~NetworkManagerIP4Config() { unregisterProxy(); }
};

class NetworkManagerObjectManager
    : public sdbus::ProxyInterfaces<sdbus::ObjectManager_proxy> {
public:
  NetworkManagerObjectManager(sdbus::IConnection &system_bus)
      : ProxyInterfaces(system_bus, "org.freedesktop.NetworkManager",
                        "/org/freedesktop") {
    registerProxy();
  }
  ~NetworkManagerObjectManager() { unregisterProxy(); }

  std::unique_ptr<NetworkManagerIP4Config>
  GetIp4Config(const sdbus::ObjectPath &access_point);

protected:
  void onInterfacesAdded(
      const sdbus::ObjectPath &objectPath,
      const std::map<std::string, std::map<std::string, sdbus::Variant>>
          &interfacesAndProperties) override {}
  void
  onInterfacesRemoved(const sdbus::ObjectPath &objectPath,
                      const std::vector<std::string> &interfaces) override {}
};

class NetworkManagerAccessPoint
    : public sdbus::ProxyInterfaces<
          org::freedesktop::NetworkManager::AccessPoint_proxy> {
public:
  NetworkManagerAccessPoint(sdbus::IConnection &system_bus,
                            const sdbus::ObjectPath &access_point_object_path)
      : ProxyInterfaces(system_bus, "org.freedesktop.NetworkManager",
                        access_point_object_path) {
    registerProxy();
  }
  ~NetworkManagerAccessPoint() { unregisterProxy(); }
};

class NetworkManagerWifiMedium
    : public api::WifiMedium,
      sdbus::ProxyInterfaces<
          org::freedesktop::NetworkManager::Device::Wireless_proxy,
          sdbus::Properties_proxy> {
public:
  NetworkManagerWifiMedium(NetworkManager &network_manager,
                           sdbus::IConnection &system_bus,
                           const sdbus::ObjectPath &wireless_device_object_path)
      : ProxyInterfaces(system_bus, "org.freedesktop.NetworkManager",
                        wireless_device_object_path),
        network_manager_(network_manager) {
    active_access_point_ = std::nullopt;
    registerProxy();
  }

  ~NetworkManagerWifiMedium() override { unregisterProxy(); }

  class ScanResultCallback : public api::WifiMedium::ScanResultCallback {
  public:
    ~ScanResultCallback() override = default;
    void OnScanResults(
        const std::vector<api::WifiScanResult> &scan_results) override {
      // TODO: Add implementation at some point
    }
  };

  bool IsInterfaceValid() const override { return true; };
  api::WifiCapability &GetCapability() override;
  api::WifiInformation &GetInformation() override;

  bool Scan(
      const api::WifiMedium::ScanResultCallback &scan_result_callback) override;

  api::WifiConnectionStatus
  ConnectToNetwork(absl::string_view ssid, absl::string_view password,
                   api::WifiAuthType auth_type) override;

  bool VerifyInternetConnectivity() override;
  std::string GetIpAddress() override;

protected:
  void onPropertiesChanged(
      const std::string &interfaceName,
      const std::map<std::string, sdbus::Variant> &changedProperties,
      const std::vector<std::string> &invalidatedProperties) override;

private:
  NetworkManager &network_manager_;

  api::WifiCapability capability_;
  api::WifiInformation information_{false};

  absl::Mutex active_access_point_lock_;
  std::optional<NetworkManagerAccessPoint> active_access_point_;

  absl::Mutex scan_result_callback_lock_;
  std::optional<
      std::reference_wrapper<const api::WifiMedium::ScanResultCallback>>
      scan_result_callback_;
};

} // namespace linux
} // namespace nearby

#endif
