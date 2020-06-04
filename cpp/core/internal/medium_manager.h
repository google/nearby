// Copyright 2020 Google LLC
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

#ifndef CORE_INTERNAL_MEDIUM_MANAGER_H_
#define CORE_INTERNAL_MEDIUM_MANAGER_H_

#include "core/internal/ble_compat.h"
#include "core/internal/mediums/mediums.h"
#include "platform/api/bluetooth_classic.h"
#include "platform/api/lock.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

/**
 * Manages everything related to the mediums used by Nearby Connections, acting
 * as a simplifying layer around the different APIs used for said management.
 *
 * <p>An overview of thread safety:
 *
 * <ul>
 *   <li>Methods are synchronized at a per-medium level. For example, all
 *       Bluetooth Classic calls are synchronized under the same
 *       'bluetooth_classic_lock_'. This ensures work on a particular medium is
 *       well-ordered without blocking other mediums from running. Nearby
 *       Mediums as a whole is already threadsafe, which is why we don't need to
 *       synchronize at a per-radio level.
 *   <li>All calls are guarded by the flag 'mediums_are_available_', which
 *       defaults to true and is set to false in shutdown(). This flag ensures
 *       that no further work is done after shutdown() has been called.
 *       Note: shutdown() is the one and only time we grab every
 *       medium-specific lock, to ensure everything stops at once.
 * </ul>
 *
 * <p>Note: For methods that start an action (eg. startAdvertising()), the radio
 * is first enabled. This is a prerequisite before doing any work on a medium;
 * they will otherwise fail if the radio is off. Calls that stop an action (eg.
 * stopAdvertising()) do not attempt to enable the radio because, if the radio
 * was off, there is no work for them to stop.
 */
template <typename Platform>
class MediumManager {
 public:
  MediumManager();
  ~MediumManager();

  // ~~~~~~~~~~~~~~~~~~~~~~~~ BLUETOOTH ~~~~~~~~~~~~~~~~~~~~~~~~
  bool isBluetoothAvailable();

  bool turnOnBluetoothDiscoverability(const string& device_name);
  void turnOffBluetoothDiscoverability();

  class FoundBluetoothDeviceProcessor {
   public:
    virtual ~FoundBluetoothDeviceProcessor() {}

    virtual void onFoundBluetoothDevice(
        Ptr<BluetoothDevice> bluetooth_device) = 0;
    virtual void onLostBluetoothDevice(
        Ptr<BluetoothDevice> bluetooth_device) = 0;
  };

  bool startScanningForBluetoothDevices(
      Ptr<FoundBluetoothDeviceProcessor> found_bluetooth_device_processor);
  void stopScanningForBluetoothDevices();

  class IncomingBluetoothConnectionProcessor {
   public:
    virtual ~IncomingBluetoothConnectionProcessor() {}

    virtual void onIncomingBluetoothConnection(
        Ptr<BluetoothSocket> bluetooth_socket) = 0;
  };

  bool isListeningForIncomingBluetoothConnections(const string& service_name);
  bool startListeningForIncomingBluetoothConnections(
      const string& service_name, Ptr<IncomingBluetoothConnectionProcessor>
                                      incoming_bluetooth_connection_processor);
  void stopListeningForIncomingBluetoothConnections(const string& service_name);

  Ptr<BluetoothSocket> connectToBluetoothDevice(
      Ptr<BluetoothDevice> bluetooth_device, const string& service_name);

  // ~~~~~~~~~~~~~~~~~~~~~~~~ BLE ~~~~~~~~~~~~~~~~~~~~~~~~

  bool isBleAvailable();

  bool startBleAdvertising(const string& service_id,
                           ConstPtr<ByteArray> advertisement_data);
  void stopBleAdvertising(const string& service_id);

  class IncomingBleConnectionProcessor {
   public:
    virtual ~IncomingBleConnectionProcessor() {}

    virtual void onIncomingBleConnection(Ptr<BLESocket> ble_socket,
                                         const string& service_id) = 0;
  };

  bool isListeningForIncomingBleConnections(const string& service_id);
  bool startListeningForIncomingBleConnections(
      const string& service_id,
      Ptr<IncomingBleConnectionProcessor> incoming_ble_connection_processor);
  void stopListeningForIncomingBleConnections(const string& service_id);

  class FoundBlePeripheralProcessor {
   public:
    virtual ~FoundBlePeripheralProcessor() {}

    virtual void onFoundBlePeripheral(
        Ptr<BLE_PERIPHERAL> ble_peripheral, const string& service_id,
        ConstPtr<ByteArray> advertisement_data) = 0;
    virtual void onLostBlePeripheral(Ptr<BLE_PERIPHERAL> ble_peripheral,
                                     const string& service_id) = 0;
  };

  bool startBleScanning(
      const string& service_id,
      Ptr<FoundBlePeripheralProcessor> found_ble_peripheral_processor);
  void stopBleScanning(const string& service_id);

  Ptr<BLESocket> connectToBlePeripheral(Ptr<BLE_PERIPHERAL> ble_peripheral,
                                        const string& service_id);

  // ~~~~~~~~~~~~~~~~~~~~~~~~ WIFI-LAN ~~~~~~~~~~~~~~~~~~~~~~~~

  bool IsWifiLanAvailable();

  bool StartWifiLanAdvertising(absl::string_view service_id,
                               absl::string_view wifi_lan_service_info_name);
  void StopWifiLanAdvertising(absl::string_view service_id);

  class FoundWifiLanServiceProcessor {
   public:
    virtual ~FoundWifiLanServiceProcessor() {}

    virtual void OnFoundWifiLanService(
        Ptr<WifiLanService> wifi_lan_service) = 0;
    virtual void OnLostWifiLanService(Ptr<WifiLanService> wifi_lan_service) = 0;
  };

  bool StartWifiLanDiscovery(
      absl::string_view service_id,
      Ptr<FoundWifiLanServiceProcessor> found_wifi_lan_service_processor);
  void StopWifiLanDiscovery(absl::string_view service_id);

  class IncomingWifiLanConnectionProcessor {
   public:
    virtual ~IncomingWifiLanConnectionProcessor() {}

    virtual void OnIncomingWifiLanConnection(
        Ptr<WifiLanSocket> wifi_lan_socket) = 0;
  };

  bool IsListeningForIncomingWifiLanConnections(absl::string_view service_id);
  bool StartListeningForIncomingWifiLanConnections(
      absl::string_view service_id, Ptr<IncomingWifiLanConnectionProcessor>
                                        incoming_wifi_lan_connection_processor);
  void StopListeningForIncomingWifiLanConnections(absl::string_view service_id);

  Ptr<WifiLanSocket> ConnectToWifiLanService(
      Ptr<WifiLanService> wifi_lan_service, absl::string_view service_id);

 private:
  // The destructor for this needs to be manually invoked after the locks below
  // are acquired, so it cannot be a ScopedPtr.
  Ptr<Mediums<Platform> > mediums_;

  ScopedPtr<Ptr<Lock> > bluetooth_classic_lock_;
  ScopedPtr<Ptr<Lock> > ble_lock_;
  ScopedPtr<Ptr<Lock> > wifi_lan_lock_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/medium_manager.cc"

#endif  // CORE_INTERNAL_MEDIUM_MANAGER_H_
