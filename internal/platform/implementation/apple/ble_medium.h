// Copyright 2022 Google LLC
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

// Note: File language is detected using heuristics. Many Objective-C++ headers are incorrectly
// classified as C++ resulting in invalid linter errors. The use of "NSArray" and other Foundation
// classes like "NSData", "NSDictionary" and "NSUUID" are highly weighted for Objective-C and
// Objective-C++ scores. Oddly, "#import <Foundation/Foundation.h>" does not contribute any points.
// This comment alone should be enough to trick the IDE in to believing this is actually some sort
// of Objective-C file. See: cs/google3/devtools/search/lang/recognize_language_classifiers_data

#import <Foundation/Foundation.h>

#include <memory>
#include <string>

#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/uuid.h"

#import "internal/platform/implementation/apple/ble_peripheral.h"
#import "internal/platform/implementation/apple/ble_server_socket.h"
#import "internal/platform/implementation/apple/bluetooth_adapter_v2.h"

@class GNCBLEMedium;
@class GNSCentralManager;
@class GNSPeripheralManager;
@class GNSPeripheralServiceManager;

namespace nearby {
namespace apple {

// The main BLE medium used inside of Nearby. This serves as the entry point for all BLE and GATT
// related operations.
class BleMedium : public api::ble_v2::BleMedium {
 public:
  BleMedium();
  ~BleMedium() override = default;

  // Async interface for StartAdvertising.
  //
  // Result status will be passed to start_advertising_result callback. To stop advertising, invoke
  // the stop_advertising callback in AdvertisingSession.
  //
  // Advertising must be stopped before attempting to start advertising again.
  std::unique_ptr<api::ble_v2::BleMedium::AdvertisingSession> StartAdvertising(
      const api::ble_v2::BleAdvertisementData &advertising_data,
      api::ble_v2::AdvertiseParameters advertise_set_parameters,
      api::ble_v2::BleMedium::AdvertisingCallback callback) override;

  // Starts BLE advertising and returns whether or not it was successful.
  //
  // Advertising must be stopped before attempting to start advertising again.
  bool StartAdvertising(const api::ble_v2::BleAdvertisementData &advertising_data,
                        api::ble_v2::AdvertiseParameters advertise_set_parameters) override;

  // Stops advertising.
  //
  // Returns whether or not advertising was successfully stopped.
  bool StopAdvertising() override;

  // Async interface for StartScanning.
  //
  // Result status will be passed to start_scanning_result callback on a private queue. To stop
  // scanning, invoke the stop_scanning callback in ScanningSession.
  //
  // Scanning must be stopped before attempting to start scanning again.
  std::unique_ptr<api::ble_v2::BleMedium::ScanningSession> StartScanning(
      const Uuid &service_uuid, api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::BleMedium::ScanningCallback callback) override;

  // Starts scanning and returns whether or not it was successful.
  //
  // Scanning must be stopped before attempting to start scanning again.
  bool StartScanning(const Uuid &service_uuid, api::ble_v2::TxPowerLevel tx_power_level,
                     api::ble_v2::BleMedium::ScanCallback callback) override;

  // Stops scanning.
  //
  // Returns whether or not scanning was successfully stopped.
  bool StopScanning() override;

  // TODO(b/290385712): ServerGattConnectionCallback methods are not yet implemented.
  //
  // Starts a GATT server. Returns a nullptr upon error.
  std::unique_ptr<api::ble_v2::GattServer> StartGattServer(
      api::ble_v2::ServerGattConnectionCallback callback) override;

  // Connects to a GATT server and negotiates the specified connection parameters. Returns nullptr
  // upon error.
  //
  // The peripheral must outlive the GATT client or undefined behavior will occur. The peripheral
  // should not be modified by this method.
  std::unique_ptr<api::ble_v2::GattClient> ConnectToGattServer(
      api::ble_v2::BlePeripheral &peripheral, api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::ClientGattConnectionCallback callback) override;

  // Opens a BLE server socket based on service ID.
  //
  // On success, returns a new BleServerSocket. On error, returns nullptr.
  std::unique_ptr<api::ble_v2::BleServerSocket> OpenServerSocket(
      const std::string &service_id) override;

  // TODO(b/290385712): cancellation_flag support is not yet implemented.
  //
  // Connects to a BLE peripheral.
  //
  // The peripheral must outlive the socket or undefined behavior will occur. The peripheral
  // should not be modified by this method.
  //
  // On success, returns a new BleSocket. On error, returns nullptr.
  std::unique_ptr<api::ble_v2::BleSocket> Connect(const std::string &service_id,
                                                  api::ble_v2::TxPowerLevel tx_power_level,
                                                  api::ble_v2::BlePeripheral &peripheral,
                                                  CancellationFlag *cancellation_flag) override;

  // Returns whether the hardware supports BOTH advertising extensions and extended scans.
  //
  // This is currently always false for all Apple hardware.
  bool IsExtendedAdvertisementsAvailable() override;

  // A peripheral cannot be retreived via MAC address on Apple platforms.
  //
  // This always returns false and does not call the callback.
  bool GetRemotePeripheral(const std::string &mac_address,
                           api::ble_v2::BleMedium::GetRemotePeripheralCallback callback) override;

  // Returns true if `id` refers to a known BLE peripheral and calls `callback` with a reference to
  // said peripheral that is only guaranteed to be available for the duration of the callback.
  // Otherwise, does not call the callback and returns false.
  bool GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId id,
                           api::ble_v2::BleMedium::GetRemotePeripheralCallback callback) override;

 private:
  void HandleAdvertisementFound(id<GNCPeripheral> peripheral,
                                NSDictionary<CBUUID *, NSData *> *serviceData);

  GNCBLEMedium *medium_;

  absl::Mutex peripherals_mutex_;
  absl::flat_hash_map<api::ble_v2::BlePeripheral::UniqueId, std::unique_ptr<BlePeripheral>>
      peripherals_ ABSL_GUARDED_BY(peripherals_mutex_);

  std::unique_ptr<EmptyBlePeripheral> local_peripheral_;

  GNSPeripheralServiceManager *socketPeripheralServiceManager_;
  GNSPeripheralManager *socketPeripheralManager_;
  GNSCentralManager *socketCentralManager_;

  // Used for the blocking version of StartAdvertising and only has an advertisement found callback.
  api::ble_v2::BleMedium::ScanCallback scan_cb_;
  // Used for the async version of StartAdvertising and has both an advertisement found and result
  // callback.
  api::ble_v2::BleMedium::ScanningCallback scanning_cb_;
};

}  // namespace apple
}  // namespace nearby
