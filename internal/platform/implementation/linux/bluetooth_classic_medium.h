// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_MEDIUM_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_CLASSIC_MEDIUM_H_

#include <functional>
#include <map>
#include <memory>
#include <optional>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sdbus-c++/Types.h>
#include <systemd/sd-bus.h>

#include "absl/synchronization/mutex.h"
#include "internal/base/observer_list.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_bluez_profile.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_devices.h"

namespace nearby {
namespace linux {
// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium final
    : public api::BluetoothClassicMedium,
      sdbus::ProxyInterfaces<sdbus::ObjectManager_proxy> {
 public:
  BluetoothClassicMedium(const BluetoothClassicMedium &) = delete;
  BluetoothClassicMedium(BluetoothClassicMedium &&) = delete;
  BluetoothClassicMedium &operator=(const BluetoothClassicMedium &) = delete;
  BluetoothClassicMedium &operator=(BluetoothClassicMedium &&) = delete;
  BluetoothClassicMedium(sdbus::IConnection &system_bus,
                         const sdbus::ObjectPath &adapter_object_path);
  ~BluetoothClassicMedium() override { unregisterProxy(); };

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
  std::unique_ptr<api::BluetoothSocket> ConnectToService(
      api::BluetoothDevice &remote_device, const std::string &service_uuid,
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
  std::unique_ptr<api::BluetoothServerSocket> ListenForService(
      const std::string &service_name,
      const std::string &service_uuid) override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#createBond()
  //
  // Start the bonding (pairing) process with the remote device.
  // Return a Bluetooth pairing instance to handle the pairing process with the
  // remote device.
  std::unique_ptr<api::BluetoothPairing> CreatePairing(
      api::BluetoothDevice &remote_device) override;

  api::BluetoothDevice *GetRemoteDevice(
      const std::string &mac_address) override;

  void AddObserver(Observer *observer) override {
    observers_.AddObserver(observer);
  };
  void RemoveObserver(Observer *observer) override {
    observers_.RemoveObserver(observer);
  };

 protected:
  void onInterfacesAdded(
      const sdbus::ObjectPath &objectPath,
      const std::map<std::string, std::map<std::string, sdbus::Variant>>
          &interfacesAndProperties) override;
  void onInterfacesRemoved(const sdbus::ObjectPath &objectPath,
                           const std::vector<std::string> &interfaces) override;

 private:
  std::unique_ptr<BluetoothAdapter> adapter_;
  std::unique_ptr<BluetoothDevices> devices_;

  std::optional<BluetoothClassicMedium::DiscoveryCallback> discovery_cb_;

  std::unique_ptr<ProfileManager> profile_manager_;
  ObserverList<Observer> observers_;
};

}  // namespace linux
}  // namespace nearby

#endif
