#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_DEVICE_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_DEVICE_H_

#include <systemd/sd-bus.h>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/bluetooth_classic.h"

namespace nearby {
namespace linux {
const char *BLUEZ_DEVICE_INTERFACE = "org.bluez.Device1";
// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice : public api::BluetoothDevice {
public:
  BluetoothDevice(sd_bus *system_bus, absl::string_view adapter,
                  absl::string_view address);
  BluetoothDevice(sd_bus *system_bus, absl::string_view device_object_path);

  ~BluetoothDevice() override { sd_bus_unref(system_bus_); };

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  std::string GetName() const override;

  // Returns BT MAC address assigned to this device.
  std::string GetMacAddress() const override;

private:
  sd_bus *system_bus_;
  std::string object_path_;
  std::string mac_addr_;
};
} // namespace linux
} // namespace nearby

#endif
