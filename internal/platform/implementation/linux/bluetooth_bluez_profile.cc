#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <sdbus-c++/Error.h>
#include <string>
#include <tuple>

#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/bluetooth_bluez_profile.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_devices.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

bool ProfileManager::ProfileRegistered(absl::string_view service_uuid) {
  registered_service_uuids_lock_.ReaderLock();
  bool registered = registered_services_.count(std::string(service_uuid)) == 1;
  registered_service_uuids_lock_.ReaderUnlock();
  return registered;
}

void Profile::Release() {
  released_ = true;
  NEARBY_LOGS(VERBOSE) << __func__ << "Profile object " << getObjectPath()
                       << " has been released";
}

void Profile::NewConnection(
    const sdbus::ObjectPath &device_object_path, const sdbus::UnixFd &fd,
    const std::map<std::string, sdbus::Variant> &fd_props) {
  if (released_) {
    NEARBY_LOGS(ERROR) << __func__ << "NewConnection called on released object "
                       << getObjectPath();
    throw sdbus::Error("org.bluez.Error.Rejected",
                       "NewConnection called on released object");
  }

  auto device = devices_.get_device_by_path(device_object_path);

  if (!device.has_value()) {
    NEARBY_LOGS(ERROR)
        << __func__
        << "NewConection called with a device object we don't know about: "
        << device_object_path;
    throw sdbus::Error("org.bluez.Error.Rejected", "Unknown object");
  }

  auto alias = device->get().Alias();
  auto mac_addr = device->get().Address();
  NEARBY_LOGS(VERBOSE) << __func__ << ": " << getObjectPath()
                       << ": Connected to " << device->get().getObjectPath();

  FDProperties props(fd_props);

  absl::MutexLock l(&connections_lock_);
  if (connections_.count(mac_addr) != 0) {
    connections_[mac_addr].push_back(std::pair(fd, std::move(props)));
  } else {
    connections_[mac_addr] = std::vector{std::pair(fd, std::move(props))};
  }
}

void Profile::RequestDisconnection(
    const sdbus::ObjectPath &device_object_path) {
  auto device = devices_.get_device_by_path(device_object_path);
  if (!device.has_value()) {
    NEARBY_LOGS(ERROR) << __func__ << ": " << getObjectPath()
                       << ": RequestDisconnection called with a device object "
                          "we don't know about: "
                       << device_object_path;
    throw sdbus::Error("org.bluez.Error.Rejected", "Unknown object");
  }

  auto mac_addr = device->get().Address();
  NEARBY_LOGS(VERBOSE) << __func__ << ": Disconnection requested for device "
                       << device_object_path;

  absl::MutexLock l(&connections_lock_);
  if (connections_.count(mac_addr) == 0) {
    NEARBY_LOGS(ERROR)
        << __func__
        << "Disconnection requested, but we are not connected to this device";
    return;
  }

  connections_.erase(mac_addr);
}

bool ProfileManager::Register(std::optional<absl::string_view> name,
                              absl::string_view service_uuid) {
  if (ProfileRegistered(service_uuid)) {
    NEARBY_LOGS(WARNING) << __func__ << ": Trying to register profile "
                         << service_uuid << " which was already registered.";
    return true;
  }

  auto profile_object_path = bluez::profile_object_path(service_uuid);
  try {
    std::map<std::string, sdbus::Variant> options;
    if (name.has_value()) {
      options["Name"] = std::string(*name);
    }
    RegisterProfile(profile_object_path, std::string(service_uuid), options);
  } catch (const sdbus::Error &e) {
    BLUEZ_LOG_METHOD_CALL_ERROR(&getProxy(), "RegisterProfile", e);
    return false;
  }

  {
    absl::MutexLock l(&registered_service_uuids_lock_);
    registered_services_.emplace(
        std::string(service_uuid),
        std::make_shared<Profile>(getProxy().getConnection(),
                                  profile_object_path, devices_));
  }

  NEARBY_LOGS(INFO) << __func__
                    << ": Registered profile instancefor service uuid "
                    << service_uuid;

  return true;
}

void ProfileManager::Unregister(absl::string_view service_uuid) {
  if (!ProfileRegistered(service_uuid)) {
    NEARBY_LOGS(WARNING)
        << __func__
        << ": attempted to unregister a profile that is not registered";
    return;
  }

  auto profile_object_path = bluez::profile_object_path(service_uuid);
  NEARBY_LOGS(VERBOSE) << __func__ << ": Unregistering profile "
                       << profile_object_path;

  try {
    UnregisterProfile(profile_object_path);
  } catch (const sdbus::Error &e) {
    BLUEZ_LOG_METHOD_CALL_ERROR(&getProxy(), "UnregisterProfile", e);
  }

  {
    absl::MutexLock l(&registered_service_uuids_lock_);
    registered_services_.erase(std::string(service_uuid));
  }
}

// Get a service record FD for a connected profile (identified by service_uuid)
// to the given device.
std::optional<sdbus::UnixFd>
ProfileManager::GetServiceRecordFD(api::BluetoothDevice &remote_device,
                                   absl::string_view service_uuid,
                                   CancellationFlag *cancellation_flag) {
  if (!ProfileRegistered(service_uuid)) {
    NEARBY_LOGS(ERROR) << __func__ << ": Service " << service_uuid
                       << " is not registered";
    return std::nullopt;
  }

  auto mac_addr = remote_device.GetMacAddress();

  registered_service_uuids_lock_.ReaderLock();
  auto profile = registered_services_[std::string(service_uuid)];
  registered_service_uuids_lock_.ReaderUnlock();

  NEARBY_LOGS(VERBOSE) << __func__ << ": " << profile->getObjectPath()
                       << ": Attempting to get a FD for service "
                       << service_uuid << " on device " << mac_addr;

  auto cond = [mac_addr, profile, cancellation_flag]() {
    return profile->connections_.count(mac_addr) != 0 ||
           (cancellation_flag != nullptr && cancellation_flag->Cancelled());
  };
  profile->connections_lock_.Lock();
  profile->connections_lock_.Await(absl::Condition(&cond));

  if (cancellation_flag != nullptr && cancellation_flag->Cancelled()) {
    NEARBY_LOGS(WARNING)
        << __func__ << ": " << profile->getObjectPath() << ": "
        << remote_device.GetMacAddress()
        << ": Cancelled waiting for a service record for profile "
        << service_uuid;
    profile->connections_lock_.Unlock();
    return std::nullopt;
  }

  auto [fd, properties] = profile->connections_[mac_addr].back();
  profile->connections_[mac_addr].pop_back();
  if (profile->connections_[mac_addr].empty())
    profile->connections_.erase(mac_addr);
  profile->connections_lock_.Unlock();

  return fd;
}

// Listen for a connected profile on any device, returning the connected device
// with its FD.
std::optional<std::pair<std::reference_wrapper<BluetoothDevice>, sdbus::UnixFd>>
ProfileManager::GetServiceRecordFD(absl::string_view service_uuid) {
  if (!ProfileRegistered(service_uuid)) {
    return std::nullopt;
  }

  registered_service_uuids_lock_.ReaderLock();
  auto profile = registered_services_[std::string(service_uuid)];
  registered_service_uuids_lock_.ReaderUnlock();

  NEARBY_LOGS(VERBOSE) << __func__ << ": " << profile->getObjectPath()
                       << ": Attempting to get a FD for service "
                       << profile->getObjectPath();

  profile->connections_lock_.Lock();
  auto cond = [profile]() { return !profile->connections_.empty(); };
  profile->connections_lock_.Await(absl::Condition(&cond));

  auto it = profile->connections_.begin();
  auto mac_addr = it->first;
  auto [fd, properties] = it->second.back();
  it->second.pop_back();
  if (it->second.empty())
    profile->connections_.erase(it);
  profile->connections_lock_.Unlock();

  return std::pair(devices_.get_device_by_address(mac_addr).value(), fd);
}

} // namespace linux
} // namespace nearby