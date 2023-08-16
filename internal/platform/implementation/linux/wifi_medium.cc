#include <arpa/inet.h>
#include <cstdint>
#include <memory>
#include <netinet/in.h>
#include <type_traits>

#include <sdbus-c++/Error.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/networkmanager_device_wireless_client_glue.h"
#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi.h"

namespace nearby {
namespace linux {

std::unique_ptr<NetworkManagerIP4Config>
NetworkManagerObjectManager::GetIp4Config(
    const sdbus::ObjectPath &access_point) {
  auto objects = GetManagedObjects();
  for (auto &[object_path, interfaces] : objects) {
    if (object_path.find("/org/freedesktop/NetworkManager/ActiveConnection/",
                         0) == 0) {
      if (interfaces.count(
              "org.freedesktop.NetworkManager.Connection.Active") == 1) {
        auto props =
            interfaces["org.freedesktop.NetworkManager.Connection.Active"];
        sdbus::ObjectPath specific_object = props["SpecificObject"];
        sdbus::ObjectPath ip4config = props["Ip4Config"];

        if (specific_object == access_point)
          return std::make_unique<NetworkManagerIP4Config>(
              getProxy().getConnection(), ip4config);
      }
    }
  }

  return nullptr;
}

api::WifiCapability &NetworkManagerWifiMedium::GetCapability() {
  try {
    auto cap_mask = WirelessCapabilities();
    // https://networkmanager.dev/docs/api/latest/nm-dbus-types.html#NMDeviceWifiCapabilities
    capability_.supports_5_ghz = (cap_mask & 0x00000400);
    capability_.supports_6_ghz = false;
    capability_.support_wifi_direct = true;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(&getProxy(), "WirelessCapabilities", e);
  }

  return capability_;
}

api::WifiInformation &NetworkManagerWifiMedium::GetInformation() {
  {
    absl::ReaderMutexLock l(&active_access_point_lock_);
    if (!active_access_point_.has_value()) {
      information_ = api::WifiInformation{false};
      return information_;
    }
  }
  try {
    absl::MutexLock l(&active_access_point_lock_);

    auto ssid_vec = active_access_point_->Ssid();
    std::string ssid{ssid_vec.begin(), ssid_vec.end()};

    information_ =
        api::WifiInformation{true, ssid, active_access_point_->HwAddress(),
                             (int32_t)(active_access_point_->Frequency())};
    NetworkManagerObjectManager manager(getProxy().getConnection());
    auto ip4config =
        manager.GetIp4Config(active_access_point_->getObjectPath());

    if (ip4config != nullptr) {
      auto address_data = ip4config->AddressData();
      if (address_data.size() > 0) {
        std::string address = address_data[0]["address"];
        information_.ip_address_dot_decimal = address;

        struct in_addr addr;
        inet_aton(address.c_str(), &addr);

        char addr_bytes[4];
        memcpy(addr_bytes, &addr.s_addr, sizeof(addr_bytes));
        information_.ip_address_4_bytes = std::string(addr_bytes, 4);
      }
    } else {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Could not find the Ip4Config object for "
                         << active_access_point_->getObjectPath();
    }
  } catch (const sdbus::Error &e) {
    absl::ReaderMutexLock l(&active_access_point_lock_);
    NEARBY_LOGS(ERROR)
        << __func__ << ": Got error '" << e.getName() << "' with message '"
        << e.getMessage()
        << "' while populating network information for access point "
        << active_access_point_->getObjectPath();
  }

  return information_;
}

void NetworkManagerWifiMedium::onPropertiesChanged(
    const std::string &interfaceName,
    const std::map<std::string, sdbus::Variant> &changedProperties,
    const std::vector<std::string> &invalidatedProperties) {
  if (interfaceName != org::freedesktop::NetworkManager::Device::
                           Wireless_proxy::INTERFACE_NAME) {
    return;
  }

  for (auto &[property, _val] : changedProperties) {
    if (property == "LastScan") {
      absl::ReaderMutexLock l(&scan_result_callback_lock_);
      if (scan_result_callback_.has_value()) {
        // scan_result_callback_->get().OnScanResults()
      }
    }
  }
}

bool NetworkManagerWifiMedium::Scan(
    const api::WifiMedium::ScanResultCallback &scan_result_callback) {
  // absl::MutexLock l(&scan_result_callback_lock_);
  // scan_result_callback_ = scan_result_callback;

  // try {
  //   RequestScan(std::map<std::string, sdbus::Variant>());
  // } catch (const sdbus::Error &e) {
  //   scan_result_callback_ = std::nullopt;
  //   DBUS_LOG_METHOD_CALL_ERROR(&getProxy(), "RequestScan", e);
  //   return false;
  // }
  return false;
}

api::WifiConnectionStatus
NetworkManagerWifiMedium::ConnectToNetwork(absl::string_view ssid,
                                           absl::string_view password,
                                           api::WifiAuthType auth_type) {
  return api::WifiConnectionStatus::kUnknown;
}

bool NetworkManagerWifiMedium::VerifyInternetConnectivity() {
  auto network_manager_proxy_ = sdbus::createProxy(
      "org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager");
  network_manager_proxy_->finishRegistration();

  try {
    std::uint32_t connectivity = network_manager_.CheckConnectivity();
    return connectivity == 4; // NM_CONNECTIVITY_FULL
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(network_manager_proxy_, "CheckConnectivity", e);
    return false;
  }
}

std::string NetworkManagerWifiMedium::GetIpAddress() {
  GetInformation();
  return information_.ip_address_dot_decimal;
}

} // namespace linux
} // namespace nearby
