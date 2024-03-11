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
#include <cstdlib>
#include <memory>

#include <sdbus-c++/IConnection.h>

#include "absl/base/call_once.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/dbus.h"

namespace nearby {
namespace linux {

namespace {
static absl::Mutex global_system_bus_mutex;
static std::weak_ptr<sdbus::IConnection> global_system_bus_connection
    ABSL_GUARDED_BY(global_system_bus_mutex);
}  // namespace

std::shared_ptr<sdbus::IConnection> getSystemBusConnection() {
  absl::MutexLock lock(&global_system_bus_mutex);
  auto bus = global_system_bus_connection.lock();
  if (bus == nullptr) {
    bus =
        std::shared_ptr<sdbus::IConnection>(sdbus::createSystemBusConnection());
    bus->enterEventLoopAsync();
    global_system_bus_connection = bus;
  }

  return bus;
}
}  // namespace linux
}  // namespace nearby
