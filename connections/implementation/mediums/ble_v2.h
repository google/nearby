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
#include "absl/strings/string_view.h"
#include "connections/advertising_options.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_callback.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_tracker.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/multi_thread_executor.h"
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
  using ServerGattConnectionCallback =
      BleV2Medium::ServerGattConnectionCallback;
  using DiscoveredPeripheralCallback = mediums::DiscoveredPeripheralCallback;

  // Callback that is invoked when a new connection is accepted.
  struct AcceptedConnectionCallback {
    std::function<void(BleSocket socket, const std::string& service_id)>
        accepted_cb = DefaultCallback<BleSocket, const std::string&>();
  };

  explicit BleV2(BluetoothRadio& bluetooth_radio);

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
  // Note: fast_advertisement_service_uuid can be emptry string to indicate that
  // `fast_advertisement_service_uuid` will be ignored for regular
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

  // Starts scanning for BLE advertisements (if it is possible for the device).
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

  // Stops scanning for BLE advertisements.
  bool StopScanning(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

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
  BleSocket Connect(const std::string& service_id,
                    BleV2Peripheral& peripheral,
                    CancellationFlag* cancellation_flag)
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
  std::unique_ptr<mediums::AdvertisementReadResult>
  ProcessFetchGattAdvertisementsRequest(
      int num_slots, int psm,
      const std::vector<std::string>& interesting_service_ids,
      mediums::AdvertisementReadResult* advertisement_read_result,
      BleV2Peripheral& peripheral) ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<mediums::AdvertisementReadResult>
  InternalReadAdvertisementFromGattServerLocked(
      int num_slots, int psm,
      const std::vector<std::string>& interesting_service_ids,
      std::unique_ptr<mediums::AdvertisementReadResult>
          advertisement_read_result,
      BleV2Peripheral& peripheral) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool StopAdvertisementGattServerLocked()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  ByteArray CreateAdvertisementHeader() ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  api::ble_v2::PowerMode PowerLevelToPowerMode(PowerLevel power_level);

  void RunOnBleThread(Runnable runnable);
  mediums::DiscoveredPeripheralTracker::AdvertisementFetcher
  GetAdvertisementFetcher();

  static constexpr api::ble_v2::BleMedium::Mtu kDefaultMtu = 512;
  static constexpr int kMaxConcurrentAcceptLoops = 5;

  mutable Mutex mutex_;
  SingleThreadExecutor serial_executor_;
  ScheduledExecutor alarm_executor_;
  BluetoothRadio& radio_ ABSL_GUARDED_BY(mutex_);
  BluetoothAdapter& adapter_ ABSL_GUARDED_BY(mutex_);
  BleV2Medium medium_ ABSL_GUARDED_BY(mutex_){adapter_};
  absl::flat_hash_set<std::string> advertising_info_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<GattServer> gatt_server_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<int, std::pair<std::string, ByteArray>>
      gatt_advertisements_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<api::ble_v2::GattCharacteristic> gatt_characteristics_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, CancelableAlarm> scanning_info_
      ABSL_GUARDED_BY(mutex_);
  DiscoveredPeripheralCallback discovered_peripheral_callback_;
  std::unique_ptr<mediums::DiscoveredPeripheralTracker>
      discovered_peripheral_tracker_ =
          std::make_unique<mediums::DiscoveredPeripheralTracker>();

  // A thread pool dedicated to running all the accept loops from
  // StartAcceptingConnections().
  MultiThreadExecutor accept_loops_runner_{kMaxConcurrentAcceptLoops};

  // A map of service_id -> ServerSocket. If map is non-empty, we
  // are currently listening for incoming connections.
  // BleServerSocket instances are used from accept_loops_runner_,
  // and thus require pointer stability.
  absl::flat_hash_map<std::string, BleServerSocket> server_sockets_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_H_
