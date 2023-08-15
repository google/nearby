#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_ADAPTER_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_ADAPTER_H_
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/ProxyInterfaces.h>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/bluez_adapter_client_glue.h"

namespace nearby {
namespace linux {
class BluetoothAdapter
    : public api::BluetoothAdapter,
      public sdbus::ProxyInterfaces<org::bluez::Adapter1_proxy> {
public:
  BluetoothAdapter(sdbus::IConnection &system_bus,
                   const sdbus::ObjectPath &adapter_object_path)
      : ProxyInterfaces(system_bus, bluez::SERVICE_DEST, adapter_object_path) {
    registerProxy();
  }

  ~BluetoothAdapter() override { unregisterProxy(); }

  bool SetStatus(Status status) override;
  bool IsEnabled() const override;

  ScanMode GetScanMode() const override;

  bool SetScanMode(ScanMode scan_mode) override;
  std::string GetName() const override;

  bool SetName(absl::string_view name) override;
  bool SetName(absl::string_view name, bool persist) override;
  std::string GetMacAddress() const override;
};
} // namespace linux
} // namespace nearby

#endif // PLATFORM_IMPL_LINUX_BLUETOOTH_ADAPTER_H_
