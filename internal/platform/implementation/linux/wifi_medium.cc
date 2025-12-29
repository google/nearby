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
#include <netinet/in.h>
#include <climits>
#include <cstdint>
#include <memory>
#include <type_traits>

#include <sdbus-c++/Error.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>
#include <systemd/sd-id128.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/networkmanager/device_wireless_client.h"
#include "internal/platform/implementation/linux/network_manager_active_connection.h"
#include "internal/platform/implementation/linux/utils.h"
#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi.h"

namespace nearby {
namespace linux {
api::WifiCapability &NetworkManagerWifiMedium::GetCapability() {
  try {
    auto cap_mask = WirelessCapabilities();
    // https://networkmanager.dev/docs/api/latest/nm-dbus-types.html#NMDeviceWifiCapabilities
    capability_.supports_5_ghz = (cap_mask & 0x00000400) != 0;
    capability_.supports_6_ghz = false;
    capability_.support_wifi_direct = true;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(&getProxy(), "WirelessCapabilities", e);
  }

  return capability_;
}

inline std::int32_t to_signed(std::uint32_t v) {
  if (v <= INT_MAX) return static_cast<std::int32_t>(v);
  if (v >= INT_MIN) return static_cast<std::int32_t>(v - INT_MIN) + INT_MIN;

  return INT_MAX;
}

api::WifiInformation &NetworkManagerWifiMedium::GetInformation() {
  std::unique_ptr<NetworkManagerAccessPoint> active_access_point;

  try {
    auto ap_path = ActiveAccessPoint();
    if (ap_path.empty()) {
      information_ = api::WifiInformation{false};
      return information_;
    }
    active_access_point =
        std::make_unique<NetworkManagerAccessPoint>(*system_bus_, ap_path);
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(this, "ActiveAccessPoint", e);
  }

  try {
    auto ssid_vec = active_access_point->Ssid();
    std::string ssid{ssid_vec.begin(), ssid_vec.end()};

    information_ =
        api::WifiInformation{true, ssid, active_access_point->HwAddress(),
                             to_signed(active_access_point->Frequency())};
    networkmanager::ObjectManager manager(system_bus_);
    auto ip4config = manager.GetIp4Config(active_access_point->getObjectPath());

    if (ip4config != nullptr) {
      auto address_data = ip4config->AddressData();
      if (!address_data.empty()) {
        std::string address = address_data[0]["address"];
        information_.ip_address_dot_decimal = address;

        struct in_addr addr {};
        inet_aton(address.c_str(), &addr);

        char addr_bytes[4];
        memcpy(addr_bytes, &addr.s_addr, sizeof(addr_bytes));
        information_.ip_address_4_bytes = std::string(addr_bytes, 4);
      }
    } else {
      LOG(ERROR) << __func__ << ": " << getObjectPath()
                         << ": Could not find the Ip4Config object for "
                         << active_access_point->getObjectPath();
    }
  } catch (const sdbus::Error &e) {
    LOG(ERROR)
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

  if (changedProperties.count("LastScan") == 1) {
    absl::MutexLock l(&last_scan_lock_);
    last_scan_ = changedProperties.at("LastScan");
  }
}

bool NetworkManagerWifiMedium::Scan(
    const api::WifiMedium::ScanResultCallback &scan_result_callback) {
  // absl::MutexLock l(&scan_result_callback_lock_);
  // scan_result_callback_ = scan_result_callback;

  try {
    RequestScan({});
  } catch (const sdbus::Error &e) {
    scan_result_callback_ = std::nullopt;
    DBUS_LOG_METHOD_CALL_ERROR(&getProxy(), "RequestScan", e);
    return false;
  }
  return false;
}

std::shared_ptr<NetworkManagerAccessPoint>
NetworkManagerWifiMedium::SearchBySSIDNoScan(
    std::vector<std::uint8_t> &ssid_bytes) {
  absl::ReaderMutexLock l(&known_access_points_lock_);
  for (auto &[object_path, ap] : known_access_points_) {
    try {
      if (ap->Ssid() == ssid_bytes) {
        return ap;
      }
    } catch (const sdbus::Error &e) {
      DBUS_LOG_PROPERTY_GET_ERROR(ap, "Ssid", e);
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

  LOG(INFO) << __func__ << ": " << getObjectPath() << ": SSID " << ssid
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

  auto scan_finish = [&, cur_last_scan]() {
    last_scan_lock_.AssertReaderHeld();
    return cur_last_scan != last_scan_;
  };

  absl::Condition cond(&scan_finish);
  bool success = last_scan_lock_.ReaderLockWhenWithTimeout(cond, scan_timeout);
  last_scan_lock_.ReaderUnlock();

  if (!success) {
    LOG(WARNING) << __func__ << ": " << getObjectPath()
                         << ": timed out waiting for scan to finish";
  }

  ap = SearchBySSIDNoScan(ssid_bytes);
  if (ap == nullptr) {
    LOG(WARNING) << __func__ << ": " << getObjectPath()
                         << ": Couldn't find SSID " << ssid;
  }

  return ap;
}

static inline std::pair<std::optional<std::string>, std::string>
AuthAlgAndKeyMgmt(api::WifiAuthType auth_type) {
  switch (auth_type) {
    case api::WifiAuthType::kUnknown:
    case api::WifiAuthType::kOpen:
      return {"open", "none"};
    case api::WifiAuthType::kWpaPsk:
      return {std::nullopt, "wpa-psk"};
    case api::WifiAuthType::kWep:
      return {"none", "wep"};
  }
}

api::WifiConnectionStatus NetworkManagerWifiMedium::ConnectToNetwork(
    absl::string_view ssid, absl::string_view password,
    api::WifiAuthType auth_type) {
  auto ap = SearchBySSID(ssid);
  if (ap == nullptr) {
    LOG(ERROR) << __func__ << ": " << getObjectPath()
                       << ": Couldn't find SSID " << ssid;
    return api::WifiConnectionStatus::kConnectionFailure;
  }

  auto connection_id = NewUuidStr();
  if (!connection_id.has_value()) {
    LOG(ERROR) << __func__ << ": could not generate a connection UUID";
    return api::WifiConnectionStatus::kUnknown;
  }

  auto [auth_alg, key_mgmt] = AuthAlgAndKeyMgmt(auth_type);

  std::map<std::string, std::map<std::string, sdbus::Variant>>
      connection_settings{
          {"connection",
           {
               {"uuid", *connection_id},
               {"autoconnect", true},
               {"id", std::string(ssid)},
               {"type", "802-11-wireless"},
               {"zone", "Public"},
           }},
          {"802-11-wireless",
           {
               {"ssid", std::vector<uint8_t>(ssid.begin(), ssid.end())},
               {"mode", "infrastructure"},
               {"security", "802-11-wireless-security"},
               {"assigned-mac-address", "random"},
           }},
          {"802-11-wireless-security", {{"key-mgmt", key_mgmt}}}};
  if (!password.empty()) {
    connection_settings["802-11-wireless-security"]["psk"] =
        std::string(password);
  }
  if (auth_alg.has_value()) {
    connection_settings["802-11-wireless-security"]["auth-alg"] = *auth_alg;
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

  LOG(INFO) << __func__ << ": " << getObjectPath()
                    << ": Added a new connection at " << connection_path;
  auto active_connection =
      networkmanager::ActiveConnection(system_bus_, active_conn_path);
  auto [reason, timeout] = active_connection.WaitForConnection();
  if (timeout) {
    LOG(ERROR)
        << __func__ << ": " << getObjectPath()
        << ": timed out while waiting for connection " << active_conn_path
        << " to be activated, last NMActiveConnectionStateReason: "
        << reason->ToString();
    return api::WifiConnectionStatus::kUnknown;
  }

  if (reason.has_value()) {
    LOG(ERROR) << __func__ << ": " << getObjectPath() << ": connection "
                       << active_conn_path
                       << " failed to activate, NMActiveConnectionStateReason:"
                       << reason->ToString();
    if (reason->value ==
            networkmanager::ActiveConnection::ActiveConnectionStateReason::
                kStateReasonNoSecrets ||
        reason->value ==
            networkmanager::ActiveConnection::ActiveConnectionStateReason::
                kStateReasonLoginFailed)
      return api::WifiConnectionStatus::kAuthFailure;
  }

  LOG(INFO) << __func__ << ": Activated connection " << connection_path;
  return api::WifiConnectionStatus::kConnected;
}

bool NetworkManagerWifiMedium::VerifyInternetConnectivity() {
  try {
    std::uint32_t connectivity = network_manager_->CheckConnectivity();
    return connectivity == 4;  // NM_CONNECTIVITY_FULL
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(network_manager_, "CheckConnectivity", e);
    return false;
  }
}

std::string NetworkManagerWifiMedium::GetIpAddress() {
  GetInformation();
  return information_.ip_address_dot_decimal;
}

std::unique_ptr<networkmanager::ActiveConnection>
NetworkManagerWifiMedium::GetActiveConnection() {
  sdbus::ObjectPath active_ap_path;

  try {
    active_ap_path = ActiveAccessPoint();
    if (active_ap_path.empty()) {
      LOG(ERROR) << __func__ << ": No active access points on "
                         << getObjectPath();
      return nullptr;
    }
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(this, "ActiveAccessPoint", e);
    return nullptr;
  }

  auto object_manager = networkmanager::ObjectManager(system_bus_);
  auto conn = object_manager.GetActiveConnectionForAccessPoint(active_ap_path,
                                                               getObjectPath());

  if (conn == nullptr) {
    LOG(ERROR)
        << __func__
        << ": Could not find an active connection using the access point "
        << active_ap_path << " and device " << getObjectPath();
  }
  return conn;
}
}  // namespace linux
}  // namespace nearby
