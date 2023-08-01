#include <fcntl.h>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include <systemd/sd-bus-protocol.h>
#include <systemd/sd-bus-vtable.h>
#include <systemd/sd-bus.h>

#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/bluetooth_bluez_profile.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/logging.h"

const char *BLUEZ_PROFILEMANAGER_INTERFACE = "org.bluez.ProfileManager1";

namespace nearby {
namespace linux {
static int profile_release(sd_bus_message *m, void *userdata,
                           sd_bus_error *error) {
  return 0;
}
static int profile_new_connection(sd_bus_message *m, void *userdata,
                                  sd_bus_error *error) {
  char *c_device_object = nullptr;
  int fd = 0, ret;

  ret = sd_bus_message_read(m, "oh", &c_device_object, &fd);
  if (ret < 0) {
    return ret;
  }

  std::string device_object(c_device_object);
  fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
  if (fd < 0) {
    return sd_bus_error_set_errno(error, errno);
  }

  sd_bus *bus;
  sd_bus_default_system(&bus);

  BluetoothDevice device(bus, device_object);
  auto mac_addr = device.GetMacAddress();
  if (mac_addr.empty()) {
    return -1;
  }

  struct RegisteredService *service =
      static_cast<struct RegisteredService *>(userdata);
  service->connections_lock.Lock();
  service->connections[mac_addr] = fd;
  service->connections_lock.Unlock();

  return 0;
}

static int profile_request_disconnection(sd_bus_message *m, void *userdata,
                                         sd_bus_error *error) {
  // TODO
  return 0;
}

static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD_WITH_ARGS("Release", SD_BUS_NO_ARGS, SD_BUS_NO_RESULT,
                            profile_release, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS(
        "NewConnection", SD_BUS_ARGS("o", path, "h", fd, "a{sq}", properties),
        SD_BUS_NO_RESULT, profile_new_connection, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS("RequestDisconnection", SD_BUS_ARGS("o", object),
                            SD_BUS_NO_RESULT, profile_request_disconnection,
                            SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};

bool ProfileManager::ProfileRegistered(absl::string_view service_uuid) {
  registered_service_uuids_lock_.ReaderLock();
  bool registered = registered_services_.count(std::string(service_uuid)) == 1;
  registered_service_uuids_lock_.ReaderUnlock();
  return registered;
}

bool ProfileManager::RegisterProfile(absl::string_view name,
                                     absl::string_view service_uuid) {
  if (ProfileRegistered(service_uuid)) {
    return true;
  }

  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  std::string uuid(service_uuid);

  auto profile_object_path =
      absl::Substitute("/com/github/google/nearby/profiles/$0", uuid);
  struct RegisteredService *service = new struct RegisteredService(uuid);
  service->slot = nullptr;

  registered_service_uuids_lock_.Lock();
  auto ret = sd_bus_add_object_vtable(system_bus_, &service->slot,
                                      profile_object_path.c_str(),
                                      "org.bluez.Profile1", vtable, service);
  if (ret < 0) {
    sd_bus_error_set_errno(&err, ret);

    NEARBY_LOGS(ERROR) << __func__ << "Error adding object "
                       << profile_object_path << ": " << err.message;
    registered_service_uuids_lock_.Unlock();
    return false;
  }

  NEARBY_LOGS(VERBOSE) << __func__
                       << "Registered a ProfileManager for service UUID "
                       << uuid << " at " << profile_object_path;

  if (sd_bus_call_method(system_bus_, BLUEZ_SERVICE, "/org/bluez",
                         BLUEZ_PROFILEMANAGER_INTERFACE, "RegisterProfile",
                         &err, nullptr, "osa{sq}", "/com/github/google/nearby",
                         uuid.c_str(), 1, "Name",
                         std::string(name).c_str()) < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << "Error calling RegisterProfile: " << err.name << ": "
                       << err.message;
    registered_service_uuids_lock_.Unlock();
    return false;
  }

  registered_services_[uuid] = service;
  registered_service_uuids_lock_.Unlock();
  return true;
}

std::optional<int>
ProfileManager::GetServiceRecordFD(api::BluetoothDevice &remote_device,
                                   absl::string_view service_uuid) {
  if (!ProfileRegistered(service_uuid)) {
    return std::nullopt;
  }

  auto mac_addr = remote_device.GetMacAddress();

  registered_service_uuids_lock_.ReaderLock();
  auto service = registered_services_[std::string(service_uuid)];
  registered_service_uuids_lock_.ReaderUnlock();

  service->connections_lock.Lock();
  auto cond = [mac_addr, service]() {
    return service->connections.count(mac_addr) == 1;
  };
  service->connections_lock.Await(absl::Condition(&cond));
  int fd = service->connections[mac_addr];
  service->connections.erase(mac_addr);
  service->connections_lock.Unlock();

  return fd;
}

std::optional<std::pair<std::string, int>>
ProfileManager::GetServiceRecordFD(absl::string_view service_uuid) {
  if (!ProfileRegistered(service_uuid)) {
    return std::nullopt;
  }

  registered_service_uuids_lock_.ReaderLock();
  auto service = registered_services_[std::string(service_uuid)];
  registered_service_uuids_lock_.ReaderUnlock();
  service->connections_lock.Lock();
  auto cond = [service]() { return !service->connections.empty(); };
  service->connections_lock.Await(absl::Condition(&cond));
  auto it = service->connections.begin();
  auto mac_addr = it->first;
  auto fd = it->second;
  service->connections.erase(it);
  service->connections_lock.Unlock();

  return std::pair<std::string, int>(mac_addr, fd);
}

} // namespace linux
} // namespace nearby
