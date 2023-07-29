#include <map>
#include <memory>

#include <systemd/sd-bus-vtable.h>
#include <systemd/sd-bus.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_bluez_profile.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/logging.h"

const char *BLUEZ_PROFILEMANAGER_INTERFACE = "org.bluez.ProfileManager1";
static int profile_release(sd_bus_message *m, void *userdata,
                           sd_bus_error *error) {
  // TODO
}
static int profile_new_connection(sd_bus_message *m, void *userdata,
                                  sd_bus_error *error) {
  // TODO
}
static int profile_new(sd_bus_message *m, void *userdata, sd_bus_error *error) {
  // TODO
}

static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD_WITH_ARGS("Release", SD_BUS_NO_ARGS, SD_BUS_NO_RESULT,
                            profile_release, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS(
        "NewConnection", SD_BUS_ARGS("o", path, "h", fd, "a{sq}", properties),
        SD_BUS_NO_RESULT, profile_new_connection, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS("RequestDisconnection", SD_BUS_ARGS("o", object),
                            SD_BUS_NO_RESULT, profile_new,
                            SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};

namespace nearby {
namespace linux {
std::unique_ptr<ProfileManager> NewProfileManager() {
  sd_bus *system_bus;
  if (auto ret = sd_bus_default_system(&system_bus); ret < 0) {
    __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
        SD_BUS_ERROR_NULL;
    sd_bus_error_set_errno(&err, ret);

    NEARBY_LOGS(ERROR) << __func__
                       << "Error connecting to system bus: " << err.name << ": "
                       << err.message;
    return nullptr;
  }

  sd_bus_slot *slot = nullptr;
  auto manager = new ProfileManager(system_bus, slot);

  if (auto ret = sd_bus_add_object_vtable(
          system_bus, &slot, "/com/github/google/nearby", "org.bluez.Profile1",
          vtable, manager->GetMethodData());
      ret < 0) {
    __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
        SD_BUS_ERROR_NULL;
    sd_bus_error_set_errno(&err, ret);

    NEARBY_LOGS(ERROR) << __func__
                       << "Error adding object /com/github/google/nearby: "
                       << err.name << ": " << err.message;
    return nullptr;
  }

  return std::unique_ptr<ProfileManager>(manager);
}

bool ProfileManager::ProfileRegistered(absl::string_view service_uuid) {
  registered_service_uuids_lock_.ReaderLock();
  bool registered =
      registered_service_uuids_.count(std::string(service_uuid)) == 1;
  registered_service_uuids_lock_.ReaderUnlock();
  return registered;
}

bool ProfileManager::RegisterProfile(absl::string_view service_uuid) {
  if (ProfileRegistered(service_uuid)) {
    return true;
  }

  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  std::string uuid(service_uuid);

  registered_service_uuids_lock_.Lock();
  if (sd_bus_call_method(system_bus_, BLUEZ_SERVICE, "/org/bluez",
                     BLUEZ_PROFILEMANAGER_INTERFACE, "RegisterProfile", &err,
                     nullptr, "osa{sq}", "/com/github/google/nearby",
                     uuid.c_str(), 0, nullptr) < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << "Error calling RegisterProfile: " << err.name << ": "
                       << err.message;
    registered_service_uuids_lock_.Unlock();
    return false;
  }
  registered_service_uuids_.insert(uuid);
  registered_service_uuids_lock_.Unlock();
  return true;
}

} // namespace linux
} // namespace nearby
