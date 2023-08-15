#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_MEDIUM_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_MEDIUM_H_

#include <functional>
#include <map>
#include <memory>
#include <optional>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>
#include <systemd/sd-bus.h>

#include "absl/synchronization/mutex.h"
#include "internal/base/observer_list.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_bluez_profile.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_devices.h"

namespace nearby {
namespace linux {
// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium : public api::BluetoothClassicMedium {
public:
  BluetoothClassicMedium(sdbus::IConnection &system_bus,
                         absl::string_view adapter);
  ~BluetoothClassicMedium() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#startDiscovery()
  //
  // Returns true once the process of discovery has been initiated.
  bool StartDiscovery(DiscoveryCallback discovery_callback) override;
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#cancelDiscovery()
  //
  // Returns true once discovery is well and truly stopped; after this returns,
  // there must be no more invocations of the DiscoveryCallback passed in to
  // StartDiscovery().
  bool StopDiscovery() override;

  // A combination of
  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#createInsecureRfcommSocketToServiceRecord
  // followed by
  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#connect().
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  // On success, returns a new BluetoothSocket.
  // On error, returns nullptr.
  std::unique_ptr<api::BluetoothSocket>
  ConnectToService(api::BluetoothDevice &remote_device,
                   const std::string &service_uuid,
                   CancellationFlag *cancellation_flag) override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#listenUsingInsecureRfcommWithServiceRecord
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  //  Returns nullptr error.
  std::unique_ptr<api::BluetoothServerSocket>
  ListenForService(const std::string &service_name,
                   const std::string &service_uuid) override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#createBond()
  //
  // Start the bonding (pairing) process with the remote device.
  // Return a Bluetooth pairing instance to handle the pairing process with the
  // remote device.
  std::unique_ptr<api::BluetoothPairing>
  CreatePairing(api::BluetoothDevice &remote_device) override;

  api::BluetoothDevice *
  GetRemoteDevice(const std::string &mac_address) override;

  void AddObserver(Observer *observer) override {
    observers_.AddObserver(observer);
  };
  void RemoveObserver(Observer *observer) override {
    observers_.RemoveObserver(observer);
  };

  std::optional<std::reference_wrapper<BluetoothDevice>>
  get_device_by_path(const sdbus::ObjectPath &);
  std::optional<std::reference_wrapper<BluetoothDevice>>
  get_device_by_address(const std::string &);
  void remove_device_by_path(const sdbus::ObjectPath &);

private:
  void onInterfacesAdded(sdbus::Signal &signal);
  void onInterfacesRemoved(sdbus::Signal &signal);

  BluetoothDevices devices_;

  absl::Mutex discovery_cb_lock_;
  std::optional<BluetoothClassicMedium::DiscoveryCallback> discovery_cb_;

  ProfileManager profile_manager_;
  ObserverList<Observer> observers_;

  std::unique_ptr<sdbus::IProxy> bluez_adapter_proxy_;
  std::unique_ptr<sdbus::IProxy> bluez_proxy_;
};

} // namespace linux
} // namespace nearby

#endif
