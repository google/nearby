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

#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_BLUEZ_PROFILE_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_BLUEZ_PROFILE_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include <sdbus-c++/AdaptorInterfaces.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_devices.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/profile_manager_client.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/profile_server.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
class ProfileManager;

class Profile final
    : public sdbus::AdaptorInterfaces<org::bluez::Profile1_adaptor,
                                      sdbus::ManagedObject_adaptor> {
 public:
  Profile(const Profile &) = delete;
  Profile(Profile &&) = delete;
  Profile &operator=(const Profile &) = delete;
  Profile &operator=(Profile &&) = delete;
  Profile(sdbus::IConnection &system_bus, sdbus::ObjectPath profile_object_path,
          BluetoothDevices &devices)
      : AdaptorInterfaces(system_bus, std::move(profile_object_path)),
        released_(false),
        devices_(devices) {
    registerAdaptor();
    NEARBY_LOGS(VERBOSE) << __func__ << ": Created a new BlueZ profile at :"
                         << getObjectPath();
  }
  ~Profile() { unregisterAdaptor(); }

 private:
  friend class ProfileManager;

  struct FDProperties {
    explicit FDProperties(const std::map<std::string, sdbus::Variant> &fd_props)
        : version(std::nullopt), features(std::nullopt) {
      if (fd_props.count("Version") == 1) {
        version = fd_props.at("Version");
      }
      if (fd_props.count("Features") == 1) {
        features = fd_props.at("Features");
      }
    }

    std::optional<uint16_t> version;
    std::optional<uint16_t> features;
  };

  void Release() override;
  void NewConnection(const sdbus::ObjectPath &, const sdbus::UnixFd &,
                     const std::map<std::string, sdbus::Variant> &) override
      ABSL_LOCKS_EXCLUDED(connections_lock_);
  void RequestDisconnection(const sdbus::ObjectPath &) override
      ABSL_LOCKS_EXCLUDED(connections_lock_);

  std::atomic_bool released_;

  absl::Mutex connections_lock_;
  std::map<std::string, std::vector<std::pair<sdbus::UnixFd, FDProperties>>>
      connections_ ABSL_GUARDED_BY(connections_lock_);

  BluetoothDevices &devices_;
};

class ProfileManager final
    : private sdbus::ProxyInterfaces<org::bluez::ProfileManager1_proxy> {
 public:
  ProfileManager(const ProfileManager &) = delete;
  ProfileManager(ProfileManager &&) = delete;
  ProfileManager &operator=(const ProfileManager &) = delete;
  ProfileManager &operator=(ProfileManager &&) = delete;
  ProfileManager(sdbus::IConnection &system_bus, BluetoothDevices &devices)
      : ProxyInterfaces(system_bus, bluez::SERVICE_DEST, "/org/bluez"),
        devices_(devices) {
    registerProxy();
  }
  ~ProfileManager() { unregisterProxy(); }

  bool ProfileRegistered(absl::string_view service_uuid)
      ABSL_LOCKS_EXCLUDED(registered_service_uuids_mutex_);
  bool Register(std::optional<absl::string_view> service_name,
                absl::string_view service_uuid)
      ABSL_LOCKS_EXCLUDED(registered_service_uuids_mutex_);
  bool Register(absl::string_view service_uuid)
      ABSL_LOCKS_EXCLUDED(registered_service_uuids_mutex_) {
    return Register(std::nullopt, service_uuid);
  }
  void Unregister(absl::string_view service_uuid)
      ABSL_LOCKS_EXCLUDED(registered_service_uuids_mutex_);

  std::optional<sdbus::UnixFd> GetServiceRecordFD(
      api::BluetoothDevice &remote_device, absl::string_view service_uuid,
      CancellationFlag *cancellation_flag)
      ABSL_LOCKS_EXCLUDED(registered_service_uuids_mutex_);
  std::optional<std::pair<std::shared_ptr<BluetoothDevice>, sdbus::UnixFd>>
  GetServiceRecordFD(absl::string_view service_uuid,
                     CancellationFlag *cancellation_flag)
      ABSL_LOCKS_EXCLUDED(registered_service_uuids_mutex_);

 private:
  BluetoothDevices &devices_;
  // Maps service UUIDs to RegisteredService
  absl::Mutex registered_service_uuids_mutex_;
  std::map<std::string, std::shared_ptr<Profile>> registered_services_
      ABSL_GUARDED_BY(registered_service_uuids_mutex_);
};

}  // namespace linux
}  // namespace nearby
#endif
