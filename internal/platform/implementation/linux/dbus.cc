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

#include <cassert>
#include <memory>

#include <sdbus-c++/IConnection.h>

#include "absl/base/call_once.h"
#include "internal/platform/implementation/linux/dbus.h"

namespace nearby {
namespace linux {
static std::unique_ptr<sdbus::IConnection> global_system_bus_connection =
    nullptr;
static std::unique_ptr<sdbus::IConnection> global_default_bus_connection =
    nullptr;
static absl::once_flag bus_connection_init_;

static void initBusConnections() {
  global_system_bus_connection = sdbus::createSystemBusConnection();
  global_system_bus_connection->enterEventLoopAsync();
  global_default_bus_connection =
      sdbus::createDefaultBusConnection("com.google.nearby");
  global_default_bus_connection->enterEventLoopAsync();
}

sdbus::IConnection &getSystemBusConnection() {
  absl::call_once(bus_connection_init_, initBusConnections);
  assert(global_system_bus_connection != nullptr);
  return *global_system_bus_connection;
}
sdbus::IConnection &getDefaultBusConnection() {
  absl::call_once(bus_connection_init_, initBusConnections);
  assert(global_default_bus_connection != nullptr);
  return *global_default_bus_connection;
}
}  // namespace linux
}  // namespace nearby
