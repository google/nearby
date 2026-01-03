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

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/Error.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/implementation/linux/bluetooth_bluez_profile.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_devices.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/logging.h"
#include "absl/strings/str_cat.h"

namespace nearby {
namespace linux {

bool ProfileManager::ProfileRegistered(absl::string_view service_uuid) {
  registered_service_uuids_mutex_.ReaderLock();
  bool registered = registered_services_.count(std::string(service_uuid)) == 1;
  registered_service_uuids_mutex_.ReaderUnlock();
  return registered;
}

void Profile::Release() {
  released_ = true;
  LOG(INFO) << __func__ << ": Profile object " << getObjectPath()
                       << " has been released";
}

void Profile::NewConnection(
    const sdbus::ObjectPath &device_object_path, const sdbus::UnixFd &fd,
    const std::map<std::string, sdbus::Variant> &fd_props) {
  if (released_) {
    LOG(ERROR) << __func__
                       << ": NewConnection called on released object "
                       << getObjectPath();
    throw sdbus::Error("org.bluez.Error.Rejected",
                       "NewConnection called on released object");
  }

  auto device = devices_.get_device_by_path(device_object_path);

  if (device == nullptr) {
    device = devices_.add_new_device(device_object_path);
  }

  auto alias = device->GetName();
  auto mac_addr = device->GetAddress();
  LOG(INFO) << __func__ << ": " << getObjectPath()
                       << ": Connected to " << mac_addr.ToString();

  FDProperties props(fd_props);

  LOG(INFO) << "PUSH key(GetAddress.ToString)=" << mac_addr.ToString()
            << " alias=" << alias;
  LOG(INFO) << "PUSH_ENTER profile=" << this
          << " mutex=" << &connections_lock_
          << " obj=" << getObjectPath()
          << " path=" << device_object_path;
  {
    absl::MutexLock l(&connections_lock_);
    connections_[mac_addr.ToString()].push_back(std::pair(fd, props));
  }
}

void Profile::RequestDisconnection(
    const sdbus::ObjectPath &device_object_path) {
  auto device = devices_.get_device_by_path(device_object_path);
  if (device == nullptr) {
    LOG(ERROR) << __func__ << ": " << getObjectPath()
                       << ": RequestDisconnection called with a device object "
                          "we don't know about: "
                       << device_object_path;
    throw sdbus::Error("org.bluez.Error.Rejected", "Unknown object");
  }

  auto mac_addr = device->GetMacAddress();
  LOG(INFO) << __func__ << ": Disconnection requested for device "
                       << device_object_path;

  absl::MutexLock l(&connections_lock_);
  if (connections_.count(mac_addr) == 0) {
    LOG(ERROR)
        << __func__
        << ": Disconnection requested, but we are not connected to this device";
    return;
  }
  connections_.erase(mac_addr);
}

bool ProfileManager::Register(std::optional<absl::string_view> name,
                              absl::string_view service_uuid) {
  absl::MutexLock l(&registered_service_uuids_mutex_);
  if (registered_services_.count(std::string(service_uuid)) == 1) {
    LOG(WARNING) << __func__ << ": Trying to register profile "
                         << service_uuid << " which was already registered.";
    return true;
  }

  auto profile = std::make_shared<Profile>(
      getProxy().getConnection(), bluez::profile_object_path(service_uuid),
      devices_);

  try {
    std::map<std::string, sdbus::Variant> options;
    if (name.has_value()) {
      options["Name"] = std::string(*name);
    }
    options["RequireAuthorization"] = false;
    options["RequireAuthentication"] = false;
    options["Channel"] = static_cast<uint16_t>(0);
    options["PSM"] = static_cast<uint16_t>(0);

    RegisterProfile(profile->getObjectPath(), std::string(service_uuid),
                    options);
  } catch (const sdbus::Error &e) {
    BLUEZ_LOG_METHOD_CALL_ERROR(&getProxy(), "RegisterProfile", e);
    return false;
  }

  registered_services_.emplace(service_uuid, profile);

  LOG(INFO) << __func__
                    << ": Registered profile instance for service uuid "
                    << service_uuid;

  return true;
}

void ProfileManager::Unregister(absl::string_view service_uuid) {
  absl::MutexLock l(&registered_service_uuids_mutex_);
  if (registered_services_.count(std::string(service_uuid)) == 0) {
    LOG(WARNING)
        << __func__
        << ": attempted to unregister a profile that is not registered";
    return;
  }

  auto profile_object_path = bluez::profile_object_path(service_uuid);
  LOG(INFO) << __func__ << ": Unregistering profile "
                       << profile_object_path;

  try {
    UnregisterProfile(profile_object_path);
  } catch (const sdbus::Error &e) {
    BLUEZ_LOG_METHOD_CALL_ERROR(&getProxy(), "UnregisterProfile", e);
  }

  registered_services_.erase(std::string(service_uuid));
}

// Get a service record FD for a connected profile (identified by service_uuid)
// to the given device. Only fires when we're requesting a new connection. i.e: we're the client
std::optional<sdbus::UnixFd> ProfileManager::GetServiceRecordFD(
    api::BluetoothDevice &remote_device, absl::string_view service_uuid,
    CancellationFlag *cancellation_flag) {
  std::shared_ptr<Profile> profile;
  {
    absl::ReaderMutexLock lock(&registered_service_uuids_mutex_);
    if (registered_services_.count(std::string(service_uuid)) == 0) {
      LOG(ERROR) << __func__ << ": Service " << service_uuid
                         << " is not registered";
      return std::nullopt;
    }
    profile = registered_services_[std::string(service_uuid)];
  }
  auto mac_addr = remote_device.GetMacAddress();

  std::unique_ptr<CancellationFlagListener> cancel_listener;
  if (cancellation_flag != nullptr)
    cancel_listener = std::make_unique<CancellationFlagListener>(
        cancellation_flag, [profile]() {
  if (profile->connections_lock_.TryLock()) {
    profile->connections_lock_.Unlock();
  }
}
);

  LOG(INFO) << __func__ << ": " << profile->getObjectPath()
                       << ": Attempting to get a FD for service "
                       << service_uuid << " on device " << mac_addr;

  LOG(INFO) << "WAIT profile=" << profile.get()
            << " mutex=" << &profile->connections_lock_
            << " obj=" << profile->getObjectPath()
            << " key=" << mac_addr;
  auto cond = [mac_addr, profile, cancellation_flag]() {
    profile->connections_lock_.AssertHeld();
    LOG(INFO) << "connections_lock_ is held by: " << mac_addr << " with ptr: " << &profile -> connections_lock_;
    return profile->connections_.count(mac_addr) != 0 ||
           (cancellation_flag != nullptr && cancellation_flag->Cancelled());
  };

  // BUG: Race condition. Hangs here
  LOG(INFO) << "WAIT key(GetMacAddress)=" << mac_addr;
  LOG(INFO) << "connections_ size" << profile -> connections_.size();
  absl::MutexLock connections_lock(&profile->connections_lock_,
                                   absl::Condition(&cond));
  LOG(INFO) << "WAIT_ACQUIRED "
            << " map_size=" << profile->connections_.size();

  // Clean up pending tracking
  profile->pending_outgoing_.erase(mac_addr);

  if (cancellation_flag != nullptr && cancellation_flag->Cancelled()) {
    LOG(INFO)
        << __func__ << ": " << profile->getObjectPath() << ": "
        << remote_device.GetMacAddress()
        << ": Cancelled waiting for a new connection on profile "
        << service_uuid;
    return std::nullopt;
  }

  auto [fd, properties] = profile->connections_[mac_addr].back();
  profile->connections_[mac_addr].pop_back();

  if (profile->connections_[mac_addr].empty())

    profile->connections_.erase(mac_addr);

  return std::move(fd);
}

// Listen for a connected profile on any device, returning the connected device
// with its FD. Only fires when another device requests connection from us. i.e. we're the server
std::optional<std::pair<std::shared_ptr<BluetoothDevice>, sdbus::UnixFd>>
ProfileManager::GetServiceRecordFD(absl::string_view service_uuid,
                                   CancellationFlag *cancellation_flag) {
  std::shared_ptr<Profile> profile;

  {
    absl::ReaderMutexLock lock(&registered_service_uuids_mutex_);
    if (registered_services_.count(std::string(service_uuid)) == 0) {
      return std::nullopt;
    }

    profile = registered_services_[std::string(service_uuid)];
  }

  LOG(INFO) << __func__ << ": " << profile->getObjectPath()
                       << ": Attempting to get a FD for service "
                       << service_uuid;

  std::unique_ptr<CancellationFlagListener> cancel_listener;
  if (cancellation_flag != nullptr)
    cancel_listener = std::make_unique<CancellationFlagListener>(
        cancellation_flag, [&profile]() {
          profile->connections_lock_.Lock();
          profile->connections_lock_.Unlock();
        });

  profile->connections_lock_.Lock();
  auto cond = [profile, &cancellation_flag]() {
    profile->connections_lock_.AssertReaderHeld();

    // Only accept connections that DON'T have pending outgoing attempts
    for (const auto& [mac, fds] : profile->connections_) {
      if (profile->pending_outgoing_.count(mac) == 0) {
        return true; // Found a connection without pending outgoing
      }
    }

    return cancellation_flag != nullptr && cancellation_flag->Cancelled();
  };
  profile->connections_lock_.Await(absl::Condition(&cond));

  if (cancellation_flag != nullptr && cancellation_flag->Cancelled()) {
    LOG(INFO)
        << __func__ << ": Cancelled waiting for new connections on profile "
        << profile->getObjectPath();
    profile->connections_lock_.Unlock();
    return std::nullopt;
  }

  // Find first connection without pending outgoing
  std::string mac_addr;
  sdbus::UnixFd fd;
  bool found = false;

  for (auto it = profile->connections_.begin(); it != profile->connections_.end(); ++it) {
    if (profile->pending_outgoing_.count(it->first) == 0) {
      mac_addr = it->first;
      auto& fds = it->second;
      // Use auto to avoid accessing private FDProperties type
      auto [fd_tmp, properties] = fds.back();
      fd = std::move(fd_tmp);
      fds.pop_back();
      if (fds.empty()) {
        profile->connections_.erase(it);
      }
      found = true;
      break;
    }
  }

  LOG(INFO) << __func__ << " Cleared connections";
  profile->connections_lock_.Unlock();

  if (!found) {
    LOG(ERROR) << __func__ << ": No eligible connection found";
    return std::nullopt;
  }

  auto device = devices_.get_device_by_address(mac_addr);
  if (device == nullptr) {
    LOG(ERROR) << __func__ << ": Device " << mac_addr
                       << " is no longer available";
    return std::nullopt;
  }

  return std::pair(device, std::move(fd));
}
  void ProfileManager::MarkPendingOutgoing(absl::string_view service_uuid,
                                                const std::string& mac_address) {
  absl::ReaderMutexLock lock(&registered_service_uuids_mutex_);
  if (registered_services_.count(std::string(service_uuid)) == 0) {
    return;
  }
  auto profile = registered_services_[std::string(service_uuid)];
  absl::MutexLock l(&profile->connections_lock_);
  profile->pending_outgoing_.insert(mac_address);
  LOG(INFO) << __func__ << ": Marked " << mac_address
            << " as pending outgoing for " << service_uuid;
}

  void ProfileManager::ClearPendingOutgoing(absl::string_view service_uuid,
                                            const std::string& mac_address) {
  absl::ReaderMutexLock lock(&registered_service_uuids_mutex_);
  if (registered_services_.count(std::string(service_uuid)) == 0) {
    return;
  }
  auto profile = registered_services_[std::string(service_uuid)];
  absl::MutexLock l(&profile->connections_lock_);
  profile->pending_outgoing_.erase(mac_address);
  LOG(INFO) << __func__ << ": Cleared " << mac_address
            << " as pending outgoing for " << service_uuid;
}
}  // namespace linux
}  // namespace nearby
