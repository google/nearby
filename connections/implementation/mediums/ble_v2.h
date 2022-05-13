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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "connections/implementation/mediums/ble_v2/advertisement_read_result.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_tracker.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/power_level.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/scheduled_executor.h"
#include "internal/platform/single_thread_executor.h"

namespace location {
namespace nearby {
namespace connections {

// Provides the operations that can be performed on the Bluetooth Low Energy
// (BLE) medium.
class BleV2 final {
 public:
  using DiscoveredPeripheralCallback = mediums::DiscoveredPeripheralCallback;

  static constexpr absl::Duration kPeripheralLostTimeout = absl::Seconds(3);

  explicit BleV2(BluetoothRadio& bluetooth_radio);
  ~BleV2();

  // Returns true, if BLE communications are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts BLE advertising, delivering additional information if the platform
  // supports it.
  //
  // service_id           - The service ID to track.
  // advertisement_bytes  - The connections BLE Advertisement used in
  //                        advertising.
  // power_level          - The power level to use for the advertisement.
  // fast_advertisement_service_uuid - The service UUID to look for fast
  //                                   advertisements on.
  // Note: fast_advertisement_service_uuid can be an empty string to indicate
  // that `fast_advertisement_service_uuid` will be ignored for regular
  // advertisement.
  bool StartAdvertising(const std::string& service_id,
                        const ByteArray& advertisement_bytes,
                        PowerLevel power_level,
                        const std::string& fast_advertisement_service_uuid)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables BLE advertising.
  bool StopAdvertising(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAdvertising(const std::string& service_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Enables BLE scanning for a service ID. Will report any discoverable
  // advertisement data through a callback.
  // Returns true, if the scanning is successfully enabled, false otherwise.
  //
  // service_id  - The service ID to track.
  // power_level - The power level to use for the discovery.
  // discovered_peripheral_callback - The callback to invoke for discovery
  //                                  events.
  // Note: fast_advertisement_service_uuid can be emptry string to indicate that
  // `fast_advertisement_service_uuid` will be ignored for regular
  // advertisement.
  bool StartScanning(const std::string& service_id, PowerLevel power_level,
                     DiscoveredPeripheralCallback callback,
                     const std::string& fast_advertisement_service_uuid)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables BLE scanning for a service ID.
  // Returns true, if the scanning was previously enabled, false otherwise.
  bool StopScanning(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if the scanning for service ID is enabled.
  bool IsScanning(const std::string& service_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if this object owns a valid platform implementation.
  bool IsMediumValid() const ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    return medium_.IsValid();
  }

 private:
  // Same as IsAvailable(), but must be called with `mutex_` held.
  bool IsAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsAdvertising(), but must be called with `mutex_` held.
  bool IsAdvertisingLocked(const std::string& service_id) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsScanning(), but must be called with `mutex_` held.
  bool IsScanningLocked(const std::string& service_id) const
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

  ByteArray CreateAdvertisementHeader() ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  api::ble_v2::PowerMode PowerLevelToPowerMode(PowerLevel power_level);

  void RunOnBleThread(Runnable runnable);

  SingleThreadExecutor serial_executor_;
  ScheduledExecutor alarm_executor_;

  mutable Mutex mutex_;
  BluetoothRadio& radio_ ABSL_GUARDED_BY(mutex_);
  BluetoothAdapter& adapter_ ABSL_GUARDED_BY(mutex_);
  BleV2Medium medium_ ABSL_GUARDED_BY(mutex_){adapter_};
  absl::flat_hash_set<std::string> advertising_service_ids_
      ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<GattServer> gatt_server_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<int, std::pair<std::string, ByteArray>>
      gatt_advertisements_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<api::ble_v2::GattCharacteristic>
      hosted_gatt_characteristics_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<std::string> scanned_service_ids_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<CancelableAlarm> lost_alarm_;
  mediums::DiscoveredPeripheralTracker discovered_peripheral_tracker_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_H_
