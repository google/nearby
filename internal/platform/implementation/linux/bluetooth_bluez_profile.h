#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_BLUEZ_PROFILE_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_BLUEZ_PROFILE_H_

#include <atomic>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include <systemd/sd-bus.h>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/bluetooth_classic.h"

namespace nearby {
namespace linux {

struct RegisteredService {
public:
  sd_bus_slot *slot;
  absl::Mutex connections_lock;
  // Maps mac addresses to unclaimed FDs. Probably an awful way to do this, but
  // whatever.
  std::map<std::string, int> connections;
  std::string &uuid;
  RegisteredService(std::string &uuid) : uuid(uuid) {}
};

class ProfileManager {
public:
  ProfileManager(sd_bus *system_bus) { system_bus_ = system_bus; }
  ~ProfileManager() { sd_bus_unref(system_bus_); }

  bool ProfileRegistered(absl::string_view service_uuid);
  bool RegisterProfile(absl::string_view service_name,
                       absl::string_view service_uuid);
  bool RegisterProfile(absl::string_view service_uuid) {
    return RegisterProfile("", service_uuid);
  }

  std::optional<int> GetServiceRecordFD(api::BluetoothDevice &remote_device,
                                        absl::string_view service_uuid);
  std::optional<std::pair<std::string, int>>
  GetServiceRecordFD(absl::string_view service_uuid);

private:
  // Maps service UUIDs to RegisteredService
  std::map<std::string, struct RegisteredService *> registered_services_;
  absl::Mutex registered_service_uuids_lock_;

  sd_bus *system_bus_;
};

} // namespace linux
} // namespace nearby
#endif
