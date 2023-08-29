// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <arpa/inet.h>
#include <cstdint>
#include <memory>
#include <netinet/in.h>
#include <type_traits>

#include <sdbus-c++/Error.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>
#include <systemd/sd-id128.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/networkmanager/connection_active_client.h"
#include "internal/platform/implementation/linux/generated/dbus/networkmanager/device_wireless_client.h"
#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi.h"

namespace nearby {
namespace linux {

std::ostream &operator<<(std::ostream &s,
                         const ActiveConnectionStateReason &reason) {
  switch (reason) {
  case kStateReasonUnknown:
    return s << "The reason for the active connection state change is unknown.";
  case kStateReasonNone:
    return s << "No reason was given for the active connection state change.";
  case kStateReasonUserDisconnected:
    return s << "The active connection changed state because the user "
                "disconnected it.";
  case kStateReasonDeviceDisconnected:
    return s << "The active connection changed state because the device it was "
                "using was disconnected.";
  case kStateReasonServiceStopped:
    return s << "The service providing the VPN connection was stopped.";
  case kStateReasonIPConfigInvalid:
    return s << "The IP config of the active connection was invalid.";
  case kStateReasonConnectTimeout:
    return s << "The connection attempt to the VPN service timed out.";
  case kStateReasonServiceStartTimeout:
    return s << "A timeout occurred while starting the service providing the "
                "VPN connection.";
  case kStateReasonServiceStartFailed:
    return s << "Starting the service providing the VPN connection failed.";
  case kStateReasonNoSecrets:
    return s << "Necessary secrets for the connection were not provided.";
  case kStateReasonLoginFailed:
    return s << "Authentication to the server failed.";
  case kStateReasonConnectionRemoved:
    return s << "The connection was deleted from settings.";
  case kStateReasonDependencyFailed:
    return s << "Master connection of this connection failed to activate.";
  case kStateReasonDeviceRealizeFailed:
    return s << "Could not create the software device link.";
  case kStateReasonDeviceRemoved:
    return s << "The device this connection depended on disappeared.";
  }
}

std::unique_ptr<NetworkManagerIP4Config>
NetworkManagerObjectManager::GetIp4Config(
    const sdbus::ObjectPath &active_connection) {
  std::map<sdbus::ObjectPath,
           std::map<std::string, std::map<std::string, sdbus::Variant>>>
      objects;
  try {
    objects = GetManagedObjects();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(this, "GetManagedObjects", e);
    return nullptr;
  }

  for (auto &[object_path, interfaces] : objects) {
    if (object_path.find("/org/freedesktop/NetworkManager/ActiveConnection/",
                         0) == 0) {
      if (interfaces.count(org::freedesktop::NetworkManager::Connection::
                               Active_proxy::INTERFACE_NAME) == 1) {
        auto props = interfaces[org::freedesktop::NetworkManager::Connection::
                                    Active_proxy::INTERFACE_NAME];
        sdbus::ObjectPath specific_object = props["SpecificObject"];
        sdbus::ObjectPath ip4config = props["Ip4Config"];

        if (specific_object == active_connection)
          return std::make_unique<NetworkManagerIP4Config>(
              getProxy().getConnection(), ip4config);
      }
    }
  }

  return nullptr;
}

std::unique_ptr<NetworkManagerActiveConnection>
NetworkManagerObjectManager::GetActiveConnectionForAccessPoint(
    const sdbus::ObjectPath &access_point,
    const sdbus::ObjectPath &device_path) {
  std::map<sdbus::ObjectPath,
           std::map<std::string, std::map<std::string, sdbus::Variant>>>
      objects;
  try {
    objects = GetManagedObjects();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(this, "GetManagedObjects", e);
    return nullptr;
  }

  for (auto &[object_path, interfaces] : objects) {
    if (object_path.find("/org/freedesktop/NetworkManager/ActiveConnection/") ==
        0) {
      if (interfaces.count(org::freedesktop::NetworkManager::Connection::
                               Active_proxy::INTERFACE_NAME) == 1) {
        auto props = interfaces[org::freedesktop::NetworkManager::Connection::
                                    Active_proxy::INTERFACE_NAME];
        sdbus::ObjectPath specific_object = props["SpecificObject"];
        if (specific_object == access_point) {
          std::vector<sdbus::ObjectPath> devices = props["Devices"];
          for (auto &path : devices) {
            if (path == device_path) {
              return std::make_unique<NetworkManagerActiveConnection>(
                  getProxy().getConnection(), object_path);
            }
          }
        }
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
  std::unique_ptr<NetworkManagerAccessPoint> active_access_point;

  try {
    auto ap_path = ActiveAccessPoint();
    if (ap_path.empty()) {
      information_ = api::WifiInformation{false};
      return information_;
    }
    active_access_point = std::make_unique<NetworkManagerAccessPoint>(
        getProxy().getConnection(), ap_path);
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(this, "ActiveAccessPoint", e);
  }

  try {
    auto ssid_vec = active_access_point->Ssid();
    std::string ssid{ssid_vec.begin(), ssid_vec.end()};

    information_ =
        api::WifiInformation{true, ssid, active_access_point->HwAddress(),
                             (int32_t)(active_access_point->Frequency())};
    NetworkManagerObjectManager manager(getProxy().getConnection());
    auto ip4config = manager.GetIp4Config(active_access_point->getObjectPath());

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
      NEARBY_LOGS(ERROR) << __func__ << ": " << getObjectPath()
                         << ": Could not find the Ip4Config object for "
                         << active_access_point->getObjectPath();
    }
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": " << getObjectPath() << ": Got error '" << e.getName()
        << "' with message '" << e.getMessage()
        << "' while populating network information for access point "
        << active_access_point->getObjectPath();
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

  for (auto &[property, val] : changedProperties) {
    if (property == "LastScan") {
      {
        absl::MutexLock l(&last_scan_lock_);
        last_scan_ = val;
      }
      // absl::ReaderMutexLock l(&scan_result_callback_lock_);
      // if (scan_result_callback_.has_value()) {
      // }
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

std::shared_ptr<NetworkManagerAccessPoint>
NetworkManagerWifiMedium::SearchBySSIDNoScan(
    std::vector<std::uint8_t> &ssid_bytes) {
  absl::ReaderMutexLock l(&known_access_points_lock_);
  for (auto &[object_path, ap] : known_access_points_) {
    if (ap->Ssid() == ssid_bytes) {
      return ap;
    }
  }

  return nullptr;
}

std::shared_ptr<NetworkManagerAccessPoint>
NetworkManagerWifiMedium::SearchBySSID(absl::string_view ssid,
                                       absl::Duration scan_timeout) {
  std::vector<std::uint8_t> ssid_bytes(ssid.begin(), ssid.end());
  // First, try to see if we already know an AP with this SSID.
  auto ap = SearchBySSIDNoScan(ssid_bytes);
  if (ap != nullptr) {
    return ap;
  }

  NEARBY_LOGS(INFO) << __func__ << ": " << getObjectPath() << ": SSID " << ssid
                    << " not currently known by device " << getObjectPath()
                    << ", requesting a scan";

  std::int64_t cur_last_scan;
  {
    absl::ReaderMutexLock l(&last_scan_lock_);
    cur_last_scan = last_scan_;
  }

  // Otherwise, request a Scan first and wait for it to finish.
  try {
    RequestScan(
        {{"ssids", std::vector<std::vector<std::uint8_t>>{ssid_bytes}}});
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(this, "RequestScan", e);
  }

  auto scan_finish = [cur_last_scan, this]() {
    this->last_scan_lock_.AssertReaderHeld();
    return cur_last_scan != this->last_scan_;
  };

  absl::Condition cond(&scan_finish);
  bool success = last_scan_lock_.ReaderLockWhenWithTimeout(cond, scan_timeout);
  last_scan_lock_.ReaderUnlock();

  if (!success) {
    NEARBY_LOGS(WARNING) << __func__ << ": " << getObjectPath()
                         << ": timed out waiting for scan to finish";
  }

  ap = SearchBySSIDNoScan(ssid_bytes);
  if (ap == nullptr) {
    NEARBY_LOGS(WARNING) << __func__ << ": " << getObjectPath()
                         << ": Couldn't find SSID " << ssid;
  }

  return ap;
}

static inline std::pair<std::string, std::string>
AuthAlgAndKeyMgmt(api::WifiAuthType auth_type) {
  switch (auth_type) {
  case api::WifiAuthType::kUnknown:
    return {"open", "none"};
  case api::WifiAuthType::kOpen:
    return {"open", "none"};
  case api::WifiAuthType::kWpaPsk:
    return {"shared", "wpa-psk"};
  case api::WifiAuthType::kWep:
    return {"none", "wep"};
  }
}

api::WifiConnectionStatus
NetworkManagerWifiMedium::ConnectToNetwork(absl::string_view ssid,
                                           absl::string_view password,
                                           api::WifiAuthType auth_type) {

  auto ap = SearchBySSID(ssid);
  if (ap == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": " << getObjectPath()
                       << ": Couldn't find SSID " << ssid;
    return api::WifiConnectionStatus::kConnectionFailure;
  }

  std::vector<std::uint8_t> ssid_bytes(ssid.begin(), ssid.end());
  std::string connection_id;

  {
    sd_id128_t id;
    char id_cstr[SD_ID128_UUID_STRING_MAX];

    if (auto ret = sd_id128_randomize(&id); ret < 0) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": could not generation a connection UUID";
      return api::WifiConnectionStatus::kUnknown;
    }

    sd_id128_to_uuid_string(id, id_cstr);
    connection_id = std::string(id_cstr);
  }

  auto [auth_alg, key_mgmt] = AuthAlgAndKeyMgmt(auth_type);

  std::map<std::string, std::map<std::string, sdbus::Variant>>
      connection_settings{
          {"connection",
           std::map<std::string, sdbus::Variant>{
               {"uuid", connection_id},
               {"autoconnect", true},
               {"id", std::string(ssid)},
               {"type", "802-11-wireless"},
               {"zone", "Public"},
           }},
          {"802-11-wireless",
           std::map<std::string, sdbus::Variant>{
               {"ssid", ssid_bytes},
               {"mode", "infrastructure"},
               {"security", "802-11-wireless-security"},
               {"assigned-mac-address", "random"},
           }},
          {"802-11-wireless-security",
           std::map<std::string, sdbus::Variant>{{"auth-alg", auth_alg},
                                                 {"key-mgmt", key_mgmt}}}};
  if (!password.empty()) {
    connection_settings["802-11-wireless-security"]["psk"] =
        std::string(password);
  }

  sdbus::ObjectPath connection_path, active_conn_path;
  try {
    auto [cp, acp, _r] = network_manager_->AddAndActivateConnection2(
        connection_settings, getObjectPath(), ap->getObjectPath(),
        {{"persist", "volatile"}, {"bind-activation", "dbus-client"}});
    connection_path = std::move(cp);
    active_conn_path = std::move(acp);
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(this, "AddAndActivateConnection2", e);
    return api::WifiConnectionStatus::kUnknown;
  }

  NEARBY_LOGS(INFO) << __func__ << ": " << getObjectPath()
                    << ": Added a new connection at " << connection_path;
  auto active_connection = NetworkManagerActiveConnection(
      getProxy().getConnection(), active_conn_path);
  auto [reason, timeout] = active_connection.WaitForConnection();
  if (timeout) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": " << getObjectPath()
        << ": timed out while waiting for connection " << active_conn_path
        << " to be activated, last NMActiveConnectionStateReason: "
        << reason.value();
    return api::WifiConnectionStatus::kUnknown;
  }

  if (reason.has_value()) {
    NEARBY_LOGS(ERROR) << __func__ << ": " << getObjectPath() << ": connection "
                       << active_conn_path
                       << " failed to activate, NMActiveConnectionStateReason:"
                       << *reason;
    if (*reason == ActiveConnectionStateReason::kStateReasonNoSecrets ||
        *reason == ActiveConnectionStateReason::kStateReasonLoginFailed)
      return api::WifiConnectionStatus::kAuthFailure;
  }

  return api::WifiConnectionStatus::kConnected;
}

bool NetworkManagerWifiMedium::VerifyInternetConnectivity() {
  try {
    std::uint32_t connectivity = network_manager_->CheckConnectivity();
    return connectivity == 4; // NM_CONNECTIVITY_FULL
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(network_manager_, "CheckConnectivity", e);
    return false;
  }
}

std::string NetworkManagerWifiMedium::GetIpAddress() {
  GetInformation();
  return information_.ip_address_dot_decimal;
}

std::unique_ptr<NetworkManagerActiveConnection>
NetworkManagerWifiMedium::GetActiveConnection() {
  sdbus::ObjectPath active_ap_path;

  try {
    active_ap_path = ActiveAccessPoint();
    if (active_ap_path.empty()) {
      NEARBY_LOGS(ERROR) << __func__ << ": No active access points on "
                         << getObjectPath();
      return nullptr;
    }
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(this, "ActiveAccessPoint", e);
    return nullptr;
  }

  auto object_manager = NetworkManagerObjectManager(getProxy().getConnection());
  auto conn = object_manager.GetActiveConnectionForAccessPoint(active_ap_path,
                                                               getObjectPath());

  if (conn == nullptr) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Could not find an active connection using the access point "
        << active_ap_path << " and device " << getObjectPath();
  }
  return conn;
}

} // namespace linux
} // namespace nearby
