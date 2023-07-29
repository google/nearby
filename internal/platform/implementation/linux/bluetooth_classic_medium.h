#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_MEDIUM_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_MEDIUM_H_

#include <map>
#include <memory>

#include <systemd/sd-bus.h>

#include "internal/base/observer_list.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"

namespace nearby {
namespace linux {
// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium : public api::BluetoothClassicMedium {
public:
  BluetoothClassicMedium(absl::string_view adapter);
  ~BluetoothClassicMedium();

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

  void AddObserver(Observer *observer) override;
  void RemoveObserver(Observer *observer) override;

  struct DiscoveryParams {
    std::string &adapter_object_path;
    std::map<std::string, std::unique_ptr<BluetoothDevice>> &devices_by_path;
    ObserverList<Observer> &observers_;
    BluetoothClassicMedium::DiscoveryCallback cb;
  };

private:
  std::string GetDeviceObjectPath(absl::string_view mac_address);
  
  sd_bus *system_bus_ = nullptr;
  sd_bus_slot *system_bus_slot_ = nullptr;
  std::string adapter_object_path_ = std::string();
  std::map<std::string, std::unique_ptr<BluetoothDevice>> devices_by_id_;
  ObserverList<Observer> observers_;

  DiscoveryParams discovery_params_ = {adapter_object_path_, devices_by_id_, observers_};
};
} // namespace linux
} // namespace nearby

#endif
