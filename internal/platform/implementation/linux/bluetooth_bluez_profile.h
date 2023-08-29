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
#include "internal/platform/implementation/linux/bluez_profile_glue.h"
#include "internal/platform/implementation/linux/bluez_profile_manager_client_glue.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
class Profile : public sdbus::AdaptorInterfaces<org::bluez::Profile1_adaptor> {
public:
  Profile(sdbus::IConnection &system_bus, absl::string_view profile_object_path,
          BluetoothDevices &devices)
      : AdaptorInterfaces(system_bus, std::string(profile_object_path)),
        released_(false), devices_(devices) {
    registerAdaptor();
    NEARBY_LOGS(VERBOSE) << __func__ << ": Created a new BlueZ profile at :"
                         << getObjectPath();
  }
  ~Profile() { unregisterAdaptor(); }

  struct FDProperties {
    FDProperties(const std::map<std::string, sdbus::Variant> &fd_props) {
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
                     const std::map<std::string, sdbus::Variant> &) override;
  void RequestDisconnection(const sdbus::ObjectPath &) override;

  std::atomic_bool released_;

  absl::Mutex connections_lock_;
  std::map<std::string, std::vector<std::pair<sdbus::UnixFd, FDProperties>>>
      connections_;

  BluetoothDevices &devices_;
};

class ProfileManager
    : private sdbus::ProxyInterfaces<org::bluez::ProfileManager1_proxy> {
public:
  ProfileManager(sdbus::IConnection &system_bus, BluetoothDevices &devices)
      : ProxyInterfaces(system_bus, bluez::SERVICE_DEST, "/org/bluez"),
        devices_(devices) {
    registerProxy();
  }
  ~ProfileManager() { unregisterProxy(); }

  bool ProfileRegistered(absl::string_view service_uuid);
  bool Register(std::optional<absl::string_view> service_name,
                absl::string_view service_uuid);
  bool Register(absl::string_view service_uuid) {
    return Register(std::nullopt, service_uuid);
  }
  void Unregister(absl::string_view service_uuid);

  std::optional<sdbus::UnixFd>
  GetServiceRecordFD(api::BluetoothDevice &remote_device,
                     absl::string_view service_uuid,
                     CancellationFlag *cancellation_flag);
  std::optional<
      std::pair<std::reference_wrapper<BluetoothDevice>, sdbus::UnixFd>>
  GetServiceRecordFD(absl::string_view service_uuid);

private:
  BluetoothDevices &devices_;
  // Maps service UUIDs to RegisteredService
  std::map<std::string, std::shared_ptr<Profile>> registered_services_;
  absl::Mutex registered_service_uuids_lock_;
};

} // namespace linux
} // namespace nearby
#endif
