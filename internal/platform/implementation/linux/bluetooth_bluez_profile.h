#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_BLUEZ_PROFILE_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_BLUEZ_PROFILE_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>

#include <systemd/sd-bus.h>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/bluetooth_classic.h"

namespace nearby {
namespace linux {

class ProfileManager {
public:
  ProfileManager(sd_bus *system_bus, sd_bus_slot *slot) {
    system_bus_ = system_bus;
    slot_ = slot;
  }  
  ~ProfileManager() { sd_bus_unref(system_bus_); }

  bool ProfileRegistered(absl::string_view service_uuid);
  bool RegisterProfile(absl::string_view sevice_uuid);

  std::optional<int> GetServiceRecordFD(api::BluetoothDevice &remote_device,
                                        absl::string_view service_uuid);

struct MethodData {
    std::map<std::tuple<std::tuple<std::string, std::string>>, int> &connections_;
    absl::Mutex &connections_lock_;
  };
  struct MethodData *GetMethodData() { return &data_; }

private:
  bool InitManagerObj();
  
  // Maps (mac address, service uuid) tuples to FDs. Probably
  // an awful way to do this, but whatever.
  std::map<std::tuple<std::tuple<std::string, std::string>>, int> connections_;
  absl::Mutex connections_lock_;

  MethodData data_{connections_, connections_lock_};

  std::set<std::string> registered_service_uuids_;
  absl::Mutex registered_service_uuids_lock_;

  sd_bus *system_bus_;
  sd_bus_slot *slot_;
};

} // namespace linux
} // namespace nearby
#endif
