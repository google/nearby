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

#ifndef PLATFORM_IMPL_LINUX_DBUS_H_
#define PLATFORM_IMPL_LINUX_DBUS_H_

#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/StandardInterfaces.h>
#include "internal/platform/logging.h"

#define DBUS_LOG_METHOD_CALL_ERROR(p, m, e)                            \
  do {                                                                 \
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << (e).getName() \
                       << "' with message '" << (e).getMessage()       \
                       << "' while calling " << (m) << " on object "   \
                       << (p)->getObjectPath();                        \
  } while (false)

#define DBUS_LOG_PROPERTY_GET_ERROR(p, prop, e)                        \
  do {                                                                 \
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << (e).getName() \
                       << "' with message '" << (e).getMessage()       \
                       << "' while getting property " << (prop)        \
                       << " on object " << (p)->getObjectPath();       \
  } while (false)

#define DBUS_LOG_PROPERTY_SET_ERROR(p, prop, e)                        \
  do {                                                                 \
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << (e).getName() \
                       << "' with message '" << (e).getMessage()       \
                       << "' while setting property " << (prop)        \
                       << " on object " << (p)->getObjectPath();       \
  } while (false)

namespace nearby {
namespace linux {
extern std::shared_ptr<sdbus::IConnection> getSystemBusConnection();
class RootObjectManager final
    : public sdbus::AdaptorInterfaces<sdbus::ObjectManager_adaptor,
                                      sdbus::Properties_adaptor> {
 public:
  explicit RootObjectManager(sdbus::IConnection &system_bus)
      : AdaptorInterfaces(system_bus, "/") {
    registerAdaptor();
  }
  ~RootObjectManager() { unregisterAdaptor(); }
};

}  // namespace linux
}  // namespace nearby
#endif
