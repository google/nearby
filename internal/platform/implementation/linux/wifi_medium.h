#ifndef PLATFORM_IMPL_LINUX_WIFI_MEDIUM_H_
#define PLATFORM_IMPL_LINUX_WIFI_MEDIUM_H_

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <optional>

#include <ostream>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/networkmanager_accesspoint_client_glue.h"
#include "internal/platform/implementation/linux/networkmanager_client_glue.h"
#include "internal/platform/implementation/linux/networkmanager_connection_active_client_glue.h"
#include "internal/platform/implementation/linux/networkmanager_device_wireless_client_glue.h"
#include "internal/platform/implementation/linux/networkmanager_ip4config_client_glue.h"
#include "internal/platform/implementation/wifi.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
class NetworkManager
    : public sdbus::ProxyInterfaces<org::freedesktop::NetworkManager_proxy> {
public:
  NetworkManager(sdbus::IConnection &system_bus)
      : ProxyInterfaces(system_bus, "org.freedesktop.NetworkManager",
                        "/org/freedesktop/NetworkManager") {
    registerProxy();
    try {
      state_ = State();
    } catch (const sdbus::Error &e) {
      DBUS_LOG_PROPERTY_GET_ERROR(this, "State", e);
    }
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

enum ActiveConnectionState {
  kStateUnknown = 0,
  kStateActivating = 1,
  kStateActivated = 2,
  kStateDeactivating = 3,
  kStateDeactivated = 4
};
enum ActiveConnectionStateReason {
  kStateReasonUnknown = 0,
  kStateReasonNone = 1,
  kStateReasonUserDisconnected = 2,
  kStateReasonDeviceDisconnected = 3,
  kStateReasonServiceStopped = 4,
  kStateReasonIPConfigInvalid = 5,
  kStateReasonConnectTimeout = 6,
  kStateReasonServiceStartTimeout = 7,
  kStateReasonServiceStartFailed = 8,
  kStateReasonNoSecrets = 9,
  kStateReasonLoginFailed = 10,
  kStateReasonConnectionRemoved = 11,
  kStateReasonDependencyFailed = 12,
  kStateReasonDeviceRealizeFailed = 13,
  kStateReasonDeviceRemoved = 14,
};

extern std::ostream &operator<<(std::ostream &s,
                                const ActiveConnectionStateReason &reason);

class NetworkManagerActiveConnection
    : public sdbus::ProxyInterfaces<
          org::freedesktop::NetworkManager::Connection::Active_proxy> {
public:
  NetworkManagerActiveConnection(
      sdbus::IConnection &system_bus,
      const sdbus::ObjectPath &active_connection_path)
      : ProxyInterfaces(system_bus, "org.freedesktop.NetworkManager",
                        active_connection_path) {
    registerProxy();
    try {
      auto state = State();
      if (state >= kStateUnknown && state <= kStateDeactivated) {
        state_ = static_cast<ActiveConnectionState>(state);
      }
    } catch (const sdbus::Error &e) {
      DBUS_LOG_PROPERTY_GET_ERROR(this, "State", e);
    }
  }
  ~NetworkManagerActiveConnection() { unregisterProxy(); }

protected:
  void onStateChanged(const uint32_t &state, const uint32_t &reason) override
      ABSL_LOCKS_EXCLUDED(state_mutex_) {
    absl::MutexLock l(&state_mutex_);
    if (state >= kStateUnknown && state <= kStateDeactivated) {
      state_ = static_cast<ActiveConnectionState>(state);
    }
    if (reason >= kStateReasonUnknown && reason <= kStateReasonDeviceRemoved) {
      reason_ = static_cast<ActiveConnectionStateReason>(reason);
    }
  }

public:
  std::pair<std::optional<ActiveConnectionStateReason>, bool>
  WaitForConnection(absl::Duration timeout = absl::Seconds(10))
      ABSL_LOCKS_EXCLUDED(state_mutex_) {
    NEARBY_LOGS(VERBOSE) << __func__ << ": Waiting for an update to "
                         << getObjectPath() << "'s state";

    auto state_changed = [this]() {
      this->state_mutex_.AssertReaderHeld();
      return this->state_ == kStateActivated ||
             this->state_ == kStateDeactivated;
    };

    absl::Condition cond(&state_changed);
    auto success = state_mutex_.ReaderLockWhenWithTimeout(cond, timeout);
    auto reason = reason_;
    auto state = state_;
    state_mutex_.ReaderUnlock();

    if (!success) {
      return {reason, true};
    }

    return state == kStateActivated ? std::pair{std::nullopt, false}
                                    : std::pair{std::optional(reason), false};
  }

  std::vector<std::string> GetIP4Addresses() {
    sdbus::ObjectPath ip4config_path;
    try {
      ip4config_path = Ip4Config();
    } catch (const sdbus::Error &e) {
      DBUS_LOG_PROPERTY_GET_ERROR(this, "Ip4Config", e);
      return {};
    }

    NetworkManagerIP4Config ip4config(getProxy().getConnection(),
                                      ip4config_path);
    std::vector<std::map<std::string, sdbus::Variant>> address_data;
    try {
      address_data = ip4config.AddressData();
    } catch (const sdbus::Error &e) {
      DBUS_LOG_PROPERTY_GET_ERROR(&ip4config, "AddressData", e);
      return {};
    }

    std::vector<std::string> ip4addresses;
    for (auto &data : address_data) {
      if (data.count("address") == 1) {
        ip4addresses.push_back(data["address"]);
      }
    }
    return ip4addresses;
  }

private:
  absl::Mutex state_mutex_;
  ActiveConnectionState state_ ABSL_GUARDED_BY(state_mutex_);
  ActiveConnectionStateReason reason_ ABSL_GUARDED_BY(state_mutex_);
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
  std::unique_ptr<NetworkManagerActiveConnection>
  GetActiveConnectionForAccessPoint(const sdbus::ObjectPath &access_point_path,
                                    const sdbus::ObjectPath &device_path);

protected:
  void onInterfacesAdded(
      const sdbus::ObjectPath &objectPath,
      const std::map<std::string, std::map<std::string, sdbus::Variant>>
          &interfacesAndProperties) override {}
  void
  onInterfacesRemoved(const sdbus::ObjectPath &objectPath,
                      const std::vector<std::string> &interfaces) override {}
};

class NetworkManagerWifiMedium
    : public api::WifiMedium,
      public sdbus::ProxyInterfaces<
          org::freedesktop::NetworkManager::Device::Wireless_proxy,
          sdbus::Properties_proxy> {
public:
  NetworkManagerWifiMedium(std::shared_ptr<NetworkManager> network_manager,
                           sdbus::IConnection &system_bus,
                           const sdbus::ObjectPath &wireless_device_object_path)
      : ProxyInterfaces(system_bus, "org.freedesktop.NetworkManager",
                        wireless_device_object_path),
        network_manager_(std::move(network_manager)) {
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

  std::shared_ptr<NetworkManagerAccessPoint>
  SearchBySSID(absl::string_view ssid,
               absl::Duration scan_timeout = absl::Seconds(15))
      ABSL_LOCKS_EXCLUDED(known_access_points_lock_);

  api::WifiConnectionStatus
  ConnectToNetwork(absl::string_view ssid, absl::string_view password,
                   api::WifiAuthType auth_type) override;

  bool VerifyInternetConnectivity() override;
  std::string GetIpAddress() override;

  std::unique_ptr<NetworkManagerActiveConnection> GetActiveConnection();

protected:
  void onPropertiesChanged(
      const std::string &interfaceName,
      const std::map<std::string, sdbus::Variant> &changedProperties,
      const std::vector<std::string> &invalidatedProperties) override;

  void onAccessPointAdded(const sdbus::ObjectPath &access_point) override
      ABSL_LOCKS_EXCLUDED(known_access_points_lock_) {
    absl::MutexLock l(&known_access_points_lock_);
    known_access_points_.erase(access_point);
    known_access_points_.emplace(access_point, getProxy().getConnection(),
                                 access_point);
  }
  void onAccessPointRemoved(const sdbus::ObjectPath &access_point) override
      ABSL_LOCKS_EXCLUDED(known_access_points_lock_) {
    absl::MutexLock l(&known_access_points_lock_);
    known_access_points_.erase(access_point);
  }

private:
  std::shared_ptr<NetworkManagerAccessPoint>
  SearchBySSIDNoScan(std::vector<std::uint8_t> &ssid)
      ABSL_LOCKS_EXCLUDED(known_access_points_lock_);

  std::shared_ptr<NetworkManager> network_manager_;

  api::WifiCapability capability_;
  api::WifiInformation information_{false};

  absl::Mutex known_access_points_lock_;
  std::map<sdbus::ObjectPath, std::shared_ptr<NetworkManagerAccessPoint>>
      known_access_points_ ABSL_GUARDED_BY(known_access_points_lock_);

  absl::Mutex scan_result_callback_lock_;
  std::optional<
      std::reference_wrapper<const api::WifiMedium::ScanResultCallback>>
      scan_result_callback_ ABSL_GUARDED_BY(scan_result_callback_lock_);

  absl::Mutex last_scan_lock_;
  std::int64_t last_scan_ ABSL_GUARDED_BY(last_scan_lock_);
};

} // namespace linux
} // namespace nearby

#endif
