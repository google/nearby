#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_PROFILE_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_PAIRING_H_

#include <string>

#include <systemd/sd-bus.h>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/bluetooth_classic.h"

namespace nearby {
namespace linux {
class BluetoothPairing : public api::BluetoothPairing {
public:
  BluetoothPairing(sd_bus *system_bus, absl::string_view device_object_path) {
    system_bus_ = system_bus;
    device_object_path_ = device_object_path;
  }
  ~BluetoothPairing() { sd_bus_unref(system_bus_); }

  bool InitiatePairing(api::BluetoothPairingCallback pairing_cb) override;
  bool FinishPairing(std::optional<absl::string_view> pin_code) override;
  bool CancelPairing() override;
  bool Unpair() override;
  bool IsPaired() override;

private:
  std::string device_object_path_;
  sd_bus *system_bus_;
  api::BluetoothPairingCallback pairing_cb_;
};
} // namespace linux
} // namespace nearby

#endif
