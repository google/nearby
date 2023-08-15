#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_PROFILE_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_PAIRING_H_

#include <memory>
#include <string>

#include <sdbus-c++/Error.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <systemd/sd-bus.h>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"

namespace nearby {
namespace linux {
class BluetoothPairing : public api::BluetoothPairing {
public:
  BluetoothPairing(const sdbus::ObjectPath &adapter_object_path,
                   BluetoothDevice &remote_device, BluetoothAdapter &adapter,
                   sdbus::IConnection &system_bus);
  ~BluetoothPairing() override = default;

  bool InitiatePairing(api::BluetoothPairingCallback pairing_cb) override;
  bool FinishPairing(std::optional<absl::string_view> pin_code) override;
  bool CancelPairing() override;
  bool Unpair() override;
  bool IsPaired() override;

private:
  void pairing_reply_handler(const sdbus::Error *e);

  sdbus::PendingAsyncCall pair_async_call_;

  BluetoothDevice &device_;
  BluetoothAdapter &adapter_;

  std::unique_ptr<sdbus::IProxy> bluez_adapter_proxy_;
  api::BluetoothPairingCallback pairing_cb_;
};
} // namespace linux
} // namespace nearby

#endif
