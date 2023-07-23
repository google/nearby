#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_ADAPTER_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_ADAPTER_H_

#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include <systemd/sd-bus.h>

namespace nearby {
namespace linux {
class BluetoothAdapter : public api::BluetoothAdapter {
public:
  ~BluetoothAdapter() override {
    if (system_bus) {
      sd_bus_unrefp(&system_bus);
    }
  };

  bool SetStatus(Status status) override;
  bool IsEnabled() const override;

  ScanMode GetScanMode() const override;

  bool SetScanMode(ScanMode scan_mode) override;
  std::string GetName() const override;

  bool SetName(absl::string_view name) override;
  bool SetName(absl::string_view name, bool persist) override;
  std::string GetMacAddress() const override;

private:
  sd_bus *system_bus;
};
} // namespace linux
} // namespace nearby

#endif // PLATFORM_IMPL_LINUX_BLUETOOTH_ADAPTER_H_
