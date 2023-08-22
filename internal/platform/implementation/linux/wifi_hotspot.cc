#include <cstring>
#include <memory>

#include <random>
#include <systemd/sd-id128.h>

#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/networkmanager_connection_active_client_glue.h"
#include "internal/platform/implementation/linux/wifi_hotspot.h"
#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
bool NetworkManagerWifiHotspotMedium::ConnectWifiHotspot(
    HotspotCredentials *hotspot_credentials) {
  if (hotspot_credentials == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": hotspot_credentials cannot be null";
    return false;
  }

  auto ssid = hotspot_credentials->GetSSID();
  auto password = hotspot_credentials->GetPassword();

  return wireless_device_->ConnectToNetwork(ssid, password,
                                            api::WifiAuthType::kWpaPsk) ==
         api::WifiConnectionStatus::kConnected;
}

bool NetworkManagerWifiHotspotMedium::DisconnectWifiHotspot() {
  if (!WifiHotspotActive()) {
    NEARBY_LOGS(ERROR) << __func__ << ": WiFi hotspot is not active";
    return false;
  }
  
  sdbus::ObjectPath active_ap_path;

  try {
    active_ap_path = wireless_device_->ActiveAccessPoint();
    if (active_ap_path.empty()) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Not connected to any access points on "
                         << wireless_device_->getObjectPath();
      return false;
    }
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(wireless_device_, "ActiveAccessPoint", e);
  }

  auto object_manager = NetworkManagerObjectManager(system_bus_);

  auto objects = object_manager.GetManagedObjects();

  for (auto &[path, interfaces] : objects) {
    if (path.find("/org/freedesktop/NetworkManager/ActiveConnection/") == 0) {
      if (interfaces.count(org::freedesktop::NetworkManager::Connection::
                               Active_proxy::INTERFACE_NAME) == 1) {
        sdbus::ObjectPath specific_object =
            interfaces[org::freedesktop::NetworkManager::Connection::
                           Active_proxy::INTERFACE_NAME]["SpecificObject"];
        if (specific_object == active_ap_path) {
          NEARBY_LOGS(INFO) << __func__ << ": Deactivating active connection "
                            << active_ap_path;

          try {
            network_manager_->DeactivateConnection(path);
          } catch (const sdbus::Error &e) {
            DBUS_LOG_METHOD_CALL_ERROR(network_manager_, "DeactiveConnection",
                                       e);
            return false;
          }
          return true;
        }
      }
    }
  }

  NEARBY_LOGS(ERROR)
      << __func__
      << ": Could not find an active connection with the access point "
      << active_ap_path;
  return false;
}

bool NetworkManagerWifiHotspotMedium::StartWifiHotspot(
    HotspotCredentials *hotspot_credentials) {
  if (WifiHotspotActive()) {
    NEARBY_LOGS(ERROR) << __func__ << ": " << wireless_device_->getObjectPath()
                       << ": cannot start WiFi hotspot, a hotspot is already "
                          "active on this device";
    return false;
  }

  sd_id128_t id;
  if (auto ret = sd_id128_randomize(&id); ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": error generating a 128-bit ID: "
                       << std::strerror(ret);
    return false;
  }
  std::string ssid = absl::StrCat("DIRECT-", SD_ID128_TO_STRING(id));
  ssid.resize(32);
  hotspot_credentials->SetSSID(ssid);

  if (auto ret = sd_id128_randomize(&id); ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": error generating a 128-bit ID: "
                       << std::strerror(ret);
    return false;
  }
  std::string password = std::string(SD_ID128_TO_STRING(id), 15);
  hotspot_credentials->SetPassword(password);

  if (auto ret = sd_id128_randomize(&id); ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": error generating a 128-bit ID: "
                       << std::strerror(ret);
    return false;
  }

  std::vector<uint8_t> ssid_bytes(ssid.begin(), ssid.end());
  std::map<std::string, std::map<std::string, sdbus::Variant>>
      connection_settings{
          {
              "connection",
              std::map<std::string, sdbus::Variant>{
                  {"uuid", SD_ID128_TO_UUID_STRING(id)},
                  {"id", "Google Nearby Hotspot"},
                  {"type", "802-11-wireless"},
                  {"zone", "Public"}},
          },
          {"802-11-wireless",
           std::map<std::string, sdbus::Variant>{
               {"assigned-mac-address", "random"},
               {"mode", "ap"},
               {"ssid", ssid_bytes},
               {"security", "802-11-wireless-security"}}},
          {"802-11-wireless-security",
           std::map<std::string, sdbus::Variant>{
               {"group", std::vector<std::string>{"ccmp"}},
               {"key-mgmt", "wpa-psk"},
               {
                   "pairwise",
                   std::vector<std::string>{"ccmp"},
               },
               {"proto", std::vector<std::string>{"rsn"}},
               {"psk", password}}},
          {"ipv4", std::map<std::string, sdbus::Variant>{"method", "shared"}},
          {"ipv6", std::map<std::string, sdbus::Variant>{
                       {"addr-gen-mode", static_cast<std::int32_t>(1)},
                       {"method", "shared"},
                   }}};
  std::unique_ptr<NetworkManagerActiveConnection> active_conn;
  try {
    auto [path, active_path, result] = network_manager_->AddAndActivateConnection2(
        connection_settings, wireless_device_->getObjectPath(), "/",
        {{"persist", "volatile"}, {"bind-activation", "dbus-client"}});
    active_conn = std::make_unique<NetworkManagerActiveConnection>(system_bus_,
                                                                   active_path);    
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(network_manager_, "AddAndActivateConnection2",
                               e);
    return false;
  }

  auto [reason, timeout] = active_conn->WaitForConnection();
  if (timeout) {
    NEARBY_LOGS(ERROR) << __func__ << ": "
                       << ": timed out while waiting for connection "
                       << active_conn->getObjectPath()                          
        << " to be activated, last NMActiveConnectionStateReason: "
        << reason.value();
    DisconnectWifiHotspot();
    return false;    
  }

  NEARBY_LOGS(INFO) << __func__ << ": Started a WiFi hotspot on device "
                    << wireless_device_->getObjectPath() << " at "
                    << active_conn->getObjectPath();
  return true;
}

bool NetworkManagerWifiHotspotMedium::WifiHotspotActive() {
  try {
    auto mode = wireless_device_->Mode();
    return mode == 3; // NM_802_11_MODE_AP
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(wireless_device_, "Mode", e);
    return false;
  }
}
} // namespace linux
} // namespace nearby
