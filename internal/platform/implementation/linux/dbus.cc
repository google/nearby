#include <memory>
#include <cassert>

#include <sdbus-c++/IConnection.h>

#include "internal/platform/implementation/linux/dbus.h"
#include "absl/base/call_once.h"

namespace nearby {
namespace linux {
static std::unique_ptr<sdbus::IConnection> global_system_bus_connection =
    nullptr;
static std::unique_ptr<sdbus::IConnection> global_default_bus_connection =
    nullptr;
static absl::once_flag bus_connection_init_;

static void initBusConnections() {
  global_system_bus_connection =
      sdbus::createSystemBusConnection("/com/google/nearby");
  global_system_bus_connection->enterEventLoopAsync();
  global_default_bus_connection =
      sdbus::createDefaultBusConnection("/com/google/nearby");
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
} // namespace linux
} // namespace nearby
