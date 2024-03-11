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

#ifndef PLATFORM_IMPL_LINUX_BLUEZ_GATT_SERVICE_CLIENT_H_
#define PLATFORM_IMPL_LINUX_BLUEZ_GATT_SERVICE_CLIENT_H_
#include <memory>

#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/IConnection.h>

#include "internal/platform/implementation/linux/generated/dbus/bluez/gatt_service_client.h"

namespace nearby {
namespace linux {
class GattServiceClient final
    : public sdbus::ProxyInterfaces<org::bluez::GattService1_proxy> {
 public:
  GattServiceClient(std::shared_ptr<sdbus::IConnection> system_bus,
                    sdbus::ObjectPath service_object_path)
      : ProxyInterfaces(*system_bus, "org.bluez",
                        std::move(service_object_path)) {
    registerProxy();
  }
  ~GattServiceClient() { unregisterProxy(); }
};
}  // namespace linux
}  // namespace nearby

#endif
