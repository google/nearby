#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_DEVICE_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_DEVICE_H_

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "internal/base/observer_list.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluez_device_client_glue.h"

namespace nearby {
namespace linux {
// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice
    : public api::BluetoothDevice,
      public sdbus::ProxyInterfaces<org::bluez::Device1_proxy> {
public:
  BluetoothDevice(sdbus::IConnection &system_bus, const sdbus::ObjectPath &);
  ~BluetoothDevice() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  std::string GetName() const override;

  // Returns BT MAC address assigned to this device.
  std::string GetMacAddress() const override;

  bool ConnectToProfile(absl::string_view service_uuid);

  void
  set_pair_reply_callback(absl::AnyInvocable<void(const sdbus::Error *)> cb) {
    absl::MutexLock l(&pair_callback_lock_);
    on_pair_reply_cb_ = std::move(cb);
  }

  void reset_pair_reply_callback() {
    absl::MutexLock l(&pair_callback_lock_);
    on_pair_reply_cb_ = DefaultCallback<const sdbus::Error *>();
  }

protected:
  void onConnectProfileReply(const sdbus::Error *error) override;
  void onPairReply(const sdbus::Error *error) override {
    absl::ReaderMutexLock l(&pair_callback_lock_);
    on_pair_reply_cb_(error);
  };

private:
  absl::Mutex pair_callback_lock_;
  absl::AnyInvocable<void(const sdbus::Error *)> on_pair_reply_cb_ =
      DefaultCallback<const sdbus::Error *>();

  mutable absl::Mutex properties_mutex_;
  mutable std::string last_known_name_ ABSL_GUARDED_BY(properties_mutex_);
  mutable std::string last_known_address_ ABSL_GUARDED_BY(properties_mutex_);
};

class MonitoredBluetoothDevice
    : public BluetoothDevice,
      public sdbus::ProxyInterfaces<sdbus::Properties_proxy> {
public:
  using sdbus::ProxyInterfaces<sdbus::Properties_proxy>::registerProxy;
  using sdbus::ProxyInterfaces<sdbus::Properties_proxy>::unregisterProxy;
  using sdbus::ProxyInterfaces<sdbus::Properties_proxy>::getObjectPath;

  MonitoredBluetoothDevice(
      sdbus::IConnection &system_bus, const sdbus::ObjectPath &,
      ObserverList<api::BluetoothClassicMedium::Observer> &observers);
  ~MonitoredBluetoothDevice() { unregisterProxy(); }

protected:
  void onPropertiesChanged(
      const std::string &interfaceName,
      const std::map<std::string, sdbus::Variant> &changedProperties,
      const std::vector<std::string> &invalidatedProperties) override;

private:
  ObserverList<api::BluetoothClassicMedium::Observer> &observers_;
};

} // namespace linux
} // namespace nearby

#endif
