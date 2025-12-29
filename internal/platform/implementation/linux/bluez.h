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

#ifndef PLATFORM_IMPL_LINUX_BLUEZ_H_
#define PLATFORM_IMPL_LINUX_BLUEZ_H_

#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/ble_v2.h"

#include <string>

#define BLUEZ_LOG_METHOD_CALL_ERROR(proxy, method, err)                  \
  do {                                                                   \
    LOG(ERROR) << __func__ << ": Got error '" << (err).getName() \
                       << "' with message '" << (err).getMessage()       \
                       << "' while calling " << method << " on object "  \
                       << (proxy)->getObjectPath();                      \
  } while (false)

namespace nearby {
namespace linux {
namespace bluez {
static constexpr const char *SERVICE_DEST = "org.bluez";

static constexpr const char *ADAPTER_INTERFACE = "org.bluez.Adapter1";

static constexpr const char *DEVICE_INTERFACE = "org.bluez.Device1";
static constexpr const char *DEVICE_PROP_ADDRESS = "Address";
static constexpr const char *DEVICE_PROP_ALIAS = "Alias";
static constexpr const char *DEVICE_PROP_PAIRED = "Paired";
static constexpr const char *DEVICE_PROP_CONNECTED = "Connected";
static constexpr const char *DEVICE_NAME = "Name";

static constexpr const char *NEARBY_BLE_GATT_PATH_ROOT =
    "/com/google/nearby/medium/ble/gatt";

std::string device_object_path(const sdbus::ObjectPath &adapter_object_path,
                               absl::string_view mac_address);
sdbus::ObjectPath profile_object_path(absl::string_view service_uuid);
sdbus::ObjectPath adapter_object_path(absl::string_view name);
sdbus::ObjectPath gatt_service_path(size_t num);
sdbus::ObjectPath gatt_characteristic_path(
    const sdbus::ObjectPath &service_path, size_t num);
sdbus::ObjectPath ble_advertisement_path(size_t num);
sdbus::ObjectPath advertisement_monitor_path(absl::string_view uuid);
int16_t TxPowerLevelDbm(api::ble_v2::TxPowerLevel level);

class BluezObjectManager
    : public sdbus::ProxyInterfaces<sdbus::ObjectManager_proxy> {
 public:
  explicit BluezObjectManager(sdbus::IConnection &system_bus)
      : ProxyInterfaces(system_bus, "org.bluez", "/") {
    registerProxy();
  }
  virtual ~BluezObjectManager() { unregisterProxy(); }

 protected:
  void onInterfacesAdded(
      const sdbus::ObjectPath &objectPath,
      const std::map<std::string, std::map<std::string, sdbus::Variant>>
          &interfacesAndProperties) override {}
  void onInterfacesRemoved(
      const sdbus::ObjectPath &objectPath,
      const std::vector<std::string> &interfaces) override {}
};

}  // namespace bluez
}  // namespace linux
}  // namespace nearby

#endif
