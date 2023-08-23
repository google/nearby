#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_DEVICES_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_DEVICES_H_

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>

#include "absl/synchronization/mutex.h"
#include "internal/base/observer_list.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"

namespace nearby {
namespace linux {
class BluetoothDevices {
public:
  BluetoothDevices(
      sdbus::IConnection &system_bus,
      const sdbus::ObjectPath &adapter_object_path,
      ObserverList<api::BluetoothClassicMedium::Observer> &observers)
      : system_bus_(system_bus), observers_(observers),
        adapter_object_path_(adapter_object_path) {}

  std::optional<std::reference_wrapper<BluetoothDevice>>
  get_device_by_path(const sdbus::ObjectPath &);
  std::optional<std::reference_wrapper<BluetoothDevice>>
  get_device_by_address(const std::string &);
  void remove_device_by_path(const sdbus::ObjectPath &);
  BluetoothDevice &add_new_device(sdbus::ObjectPath);

private:
  absl::Mutex devices_by_path_lock_;
  std::map<std::string, MonitoredBluetoothDevice> devices_by_path_;

  sdbus::IConnection &system_bus_;
  ObserverList<api::BluetoothClassicMedium::Observer> &observers_;
  sdbus::ObjectPath adapter_object_path_;
};
} // namespace linux
} // namespace nearby

#endif