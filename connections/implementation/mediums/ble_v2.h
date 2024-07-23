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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_V2_H_
#define CORE_INTERNAL_MEDIUMS_BLE_V2_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/mediums/ble_v2/advertisement_read_result.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_callback.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_tracker.h"
#include "connections/implementation/mediums/ble_v2/instant_on_lost_manager.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/power_level.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/runnable.h"
#include "internal/platform/scheduled_executor.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace connections {

// Provides the operations that can be performed on the Bluetooth Low Energy
// (BLE) medium.
class BleV2 final {
 public:
  using DiscoveredPeripheralCallback = mediums::DiscoveredPeripheralCallback;

  // Callback that is invoked when a new connection is accepted.
  using AcceptedConnectionCallback = absl::AnyInvocable<void(
      BleV2Socket socket, const std::string& service_id)>;

  explicit BleV2(BluetoothRadio& bluetooth_radio);
  ~BleV2();

  // Returns true, if BLE communications are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts BLE advertising, delivering additional information if the platform
  // supports it.
  //
  // service_id            - The service ID to track.
  // advertisement_bytes   - The connections BLE Advertisement used in
  //                         advertising.
  // power_level           - The power level to use for the advertisement.
  // is_fast_advertisement - True to use fast advertisements, which are smaller
  //                         but much more efficient to discover.
  bool StartAdvertising(const std::string& service_id,
                        const ByteArray& advertisement_bytes,
                        PowerLevel power_level, bool is_fast_advertisement)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables BLE advertising.
  bool StopAdvertising(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAdvertising(const std::string& service_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAdvertisingForLegacyDevice(const std::string& service_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Use dummy bytes to do ble advertising, only for legacy devices.
  // Returns true, if data is successfully set, and false otherwise.
  bool StartLegacyAdvertising(
      const std::string& service_id, const std::string& local_endpoint_id,
      const std::string& fast_advertisement_service_uuid)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // (TODO:hais) update this after ble_v2 async api refactor.
  // Stop Ble advertising with dummy bytes for legagy device.
  bool StopLegacyAdvertising(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Enables BLE scanning for a service ID. Will report any discoverable
  // advertisement data through a callback.
  // Returns true, if the scanning is successfully enabled, false otherwise.
  //
  // service_id  - The service ID to track.
  // power_level - The power level to use for the discovery.
  // discovered_peripheral_callback - The callback to invoke for discovery
  //                                  events.
  bool StartScanning(const std::string& service_id, PowerLevel power_level,
                     DiscoveredPeripheralCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables BLE scanning for a service ID.
  // Returns true, if the scanning was previously enabled, false otherwise.
  bool StopScanning(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if the scanning for service ID is enabled.
  bool IsScanning(const std::string& service_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts a worker thread, creates a Ble socket, associates it with a
  // service id.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Closes socket corresponding to a service id.
  bool StopAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Establishes connection to Ble peripheral.
  // Returns socket instance. On success, BleSocket.IsValid() return true.
  BleV2Socket Connect(const std::string& service_id,
                      const BleV2Peripheral& peripheral,
                      CancellationFlag* cancellation_flag)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if this object owns a valid platform implementation.
  bool IsMediumValid() const ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    return medium_.IsValid();
  }

  // Returns true if the BLE device support extended advertisement.
  bool IsExtendedAdvertisementsAvailable() ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    return medium_.IsExtendedAdvertisementsAvailable();
  };

 private:
  struct AdvertisingInfo {
    mediums::BleAdvertisement medium_advertisement;
    PowerLevel power_level;
    bool is_fast_advertisement;
  };

  // Same as IsAvailable(), but must be called with `mutex_` held.
  bool IsAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsAdvertising(), but must be called with `mutex_` held.
  bool IsAdvertisingLocked(const std::string& service_id) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsAdvertisingForLegacyDevice(), but must be called with `mutex_`
  // held.
  bool IsAdvertisingForLegacyDeviceLocked(const std::string& service_id) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsScanning(), but must be called with `mutex_` held.
  bool IsScanningLocked(const std::string& service_id) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsListeningForIncomingConnections(), but must be called with
  // `mutex_` held.
  bool IsAcceptingConnectionsLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool IsAdvertisementGattServerRunningLocked()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool StartAdvertisementGattServerLocked(const std::string& service_id,
                                          const ByteArray& gatt_advertisement)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool GenerateAdvertisementCharacteristic(int slot,
                                           const ByteArray& gatt_advertisement,
                                           GattServer& gatt_server)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void ProcessFetchGattAdvertisementsRequest(
      BleV2Peripheral peripheral, int num_slots, int psm,
      const std::vector<std::string>& interesting_service_ids,
      mediums::AdvertisementReadResult& advertisement_read_result)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool StopAdvertisementGattServerLocked()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  ByteArray CreateAdvertisementHeader(int psm,
                                      bool extended_advertisement_advertised)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  // For devices that don't have extended nor gatt adverting.
  api::ble_v2::BleAdvertisementData CreateAdvertisingDataForLegacyDevice();

  bool StartAdvertisingLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool StartFastAdvertisingLocked(
      const std::string& service_id, PowerLevel power_level,
      const mediums::BleAdvertisement& medium_advertisement)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool StartRegularAdvertisingLocked(
      const std::string& service_id, PowerLevel power_level,
      const mediums::BleAdvertisement& medium_advertisement)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool StartGattAdvertisingLocked(const std::string& service_id,
                                  PowerLevel power_level, int psm,
                                  const ByteArray& medium_advertisement_bytes,
                                  bool extended_advertisement_advertised)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  // Called by StartScanning when using the async methods.
  bool StartAsyncScanningLocked(absl::string_view service_id,
                                PowerLevel power_level)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  // Called by StartScanning when using the async methods.
  bool StopAsyncScanningLocked(absl::string_view service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  api::ble_v2::TxPowerLevel PowerLevelToTxPowerLevel(PowerLevel power_level);

  void RunOnBleThread(Runnable runnable);

  static constexpr int kMaxConcurrentAcceptLoops = 5;

  SingleThreadExecutor serial_executor_;
  ScheduledExecutor alarm_executor_;

  mutable Mutex mutex_;
  BluetoothRadio& radio_ ABSL_GUARDED_BY(mutex_);
  BluetoothAdapter& adapter_ ABSL_GUARDED_BY(mutex_);
  BleV2Medium medium_ ABSL_GUARDED_BY(mutex_){adapter_};
  absl::btree_map<std::string, AdvertisingInfo> advertising_infos_
      ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<GattServer> gatt_server_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<int, std::pair<std::string, ByteArray>>
      gatt_advertisements_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<api::ble_v2::GattCharacteristic>
      hosted_gatt_characteristics_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<std::string> scanned_service_ids_ ABSL_GUARDED_BY(mutex_);
  // This map has the same purpose as the set above, but is used only by
  // the async StartScanning method.
  absl::flat_hash_map<std::string,
                      std::unique_ptr<api::ble_v2::BleMedium::ScanningSession>>
      service_ids_to_scanning_sessions_ ABSL_GUARDED_BY(mutex_);
  // Save advertising sessions by service id, used by the async StartAdvertising
  // method.
  absl::flat_hash_map<
      std::string, std::unique_ptr<api::ble_v2::BleMedium::AdvertisingSession>>
      service_ids_to_advertising_sessions_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<CancelableAlarm> lost_alarm_;
  mediums::DiscoveredPeripheralTracker discovered_peripheral_tracker_
      ABSL_GUARDED_BY(mutex_){medium_.IsExtendedAdvertisementsAvailable()};

  // A thread pool dedicated to running all the accept loops from
  // StartAcceptingConnections().
  MultiThreadExecutor accept_loops_runner_{kMaxConcurrentAcceptLoops};

  // A map of service_id -> ServerSocket. If map is non-empty, we
  // are currently listening for incoming connections.
  absl::flat_hash_map<std::string, BleV2ServerSocket> server_sockets_
      ABSL_GUARDED_BY(mutex_);

  // Tracks currently connected incoming sockets. This lets the device know when
  // it's okay to restart GATT server related operations.
  absl::flat_hash_map<std::string, BleV2Socket> incoming_sockets_
      ABSL_GUARDED_BY(mutex_);

  mediums::InstantOnLostManager instant_on_lost_manager_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_H_
