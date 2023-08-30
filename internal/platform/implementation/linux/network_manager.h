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

#ifndef PLATFORM_IMPL_LINUX_NETWORK_MANAGER_H_
#define PLATFORM_IMPL_LINUX_NETWORK_MANAGER_H_

#include <atomic>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/ProxyInterfaces.h>

#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/networkmanager/ip4config_client.h"
#include "internal/platform/implementation/linux/generated/dbus/networkmanager/networkmanager_client.h"
#include "internal/platform/implementation/linux/network_manager_active_connection.h"

namespace nearby {
namespace linux {
class NetworkManager final
    : public sdbus::ProxyInterfaces<org::freedesktop::NetworkManager_proxy> {
 public:
  NetworkManager(const NetworkManager &) = delete;
  NetworkManager(NetworkManager &&) = delete;
  NetworkManager &operator=(const NetworkManager &) = delete;
  NetworkManager &operator=(NetworkManager &&) = delete;
  explicit NetworkManager(sdbus::IConnection &system_bus)
      : ProxyInterfaces(system_bus, "org.freedesktop.NetworkManager",
                        "/org/freedesktop/NetworkManager"),
        state_(kNMStateUnknown) {
    registerProxy();
    try {
      setState(State());
    } catch (const sdbus::Error &e) {
      DBUS_LOG_PROPERTY_GET_ERROR(this, "State", e);
    }
  }
  ~NetworkManager() { unregisterProxy(); }

  // https://networkmanager.dev/docs/api/latest/nm-dbus-types.html#NMState
  enum NMState {
    kNMStateUnknown = 0,
    kNMStateAsleep = 10,
    kNMStateDisconnected = 20,
    kNMStateDisconnecting = 30,
    kNMStateConnecting = 40,
    kNMStateConnectedLocal = 50,
    kNMStateConnectedSite = 60,
    kNMStateConnectedGlobal = 70,
  };

  NMState getState() const { return state_; }

 protected:
  void onCheckPermissions() override {}
  void onStateChanged(const uint32_t &state) override { setState(state); }
  void onDeviceAdded(const sdbus::ObjectPath &device_path) override {}
  void onDeviceRemoved(const sdbus::ObjectPath &device_path) override {}

 private:
  void inline setState(std::uint32_t val) {
#define NM_STATE_CASE_SET(k) \
  case (k):                  \
    state_ = (k);            \
    break

    switch (val) {
      NM_STATE_CASE_SET(kNMStateAsleep);
      NM_STATE_CASE_SET(kNMStateDisconnected);
      NM_STATE_CASE_SET(kNMStateDisconnecting);
      NM_STATE_CASE_SET(kNMStateConnecting);
      NM_STATE_CASE_SET(kNMStateConnectedLocal);
      NM_STATE_CASE_SET(kNMStateConnectedSite);
      NM_STATE_CASE_SET(kNMStateConnectedGlobal);
      default:
        NEARBY_LOGS(ERROR) << __func__ << "invalid NMState value: " << val
                           << ", setting state to unknown";
        NM_STATE_CASE_SET(kNMStateUnknown);
    }
#undef NM_STATE_CASE_SET
  };

  std::atomic<NMState> state_;
};

class NetworkManagerIP4Config
    : public sdbus::ProxyInterfaces<
          org::freedesktop::NetworkManager::IP4Config_proxy> {
 public:
  NetworkManagerIP4Config(const NetworkManagerIP4Config &) = delete;
  NetworkManagerIP4Config(NetworkManagerIP4Config &&) = delete;
  NetworkManagerIP4Config &operator=(const NetworkManagerIP4Config &) = delete;
  NetworkManagerIP4Config &operator=(NetworkManagerIP4Config &&) = delete;
  NetworkManagerIP4Config(sdbus::IConnection &system_bus,
                          const sdbus::ObjectPath &config_object_path)
      : ProxyInterfaces(system_bus, "org.freedesktop.NetworkManager",
                        config_object_path) {
    registerProxy();
  }
  ~NetworkManagerIP4Config() { unregisterProxy(); }
};

class NetworkManagerObjectManager final
    : public sdbus::ProxyInterfaces<sdbus::ObjectManager_proxy> {
 public:
  NetworkManagerObjectManager(const NetworkManagerObjectManager &) = delete;
  NetworkManagerObjectManager(NetworkManagerObjectManager &&) = delete;
  NetworkManagerObjectManager &operator=(const NetworkManagerObjectManager &) =
      delete;
  NetworkManagerObjectManager &operator=(NetworkManagerObjectManager &&) =
      delete;
  explicit NetworkManagerObjectManager(sdbus::IConnection &system_bus)
      : ProxyInterfaces(system_bus, "org.freedesktop.NetworkManager",
                        "/org/freedesktop") {
    registerProxy();
  }
  ~NetworkManagerObjectManager() { unregisterProxy(); }

  std::unique_ptr<NetworkManagerIP4Config> GetIp4Config(
      const sdbus::ObjectPath &access_point);
  std::unique_ptr<NetworkManagerActiveConnection>
  GetActiveConnectionForAccessPoint(const sdbus::ObjectPath &access_point_path,
                                    const sdbus::ObjectPath &device_path);

 protected:
  void onInterfacesAdded(
      const sdbus::ObjectPath &objectPath,
      const std::map<std::string, std::map<std::string, sdbus::Variant>>
          &interfacesAndProperties) override {}
  void onInterfacesRemoved(
      const sdbus::ObjectPath &objectPath,
      const std::vector<std::string> &interfaces) override {}
};

}  // namespace linux
}  // namespace nearby
#endif
