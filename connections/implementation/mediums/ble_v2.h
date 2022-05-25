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

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/types/optional.h"
#include "connections/implementation/mediums/ble_v2/advertisement_read_result.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_tracker.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/power_level.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/prng.h"
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

  // Callback that is invoked when a new connection is accepted.
  struct AcceptedConnectionCallback {
    std::function<void(BleV2Socket socket, const std::string& service_id)>
        accepted_cb = DefaultCallback<BleV2Socket, const std::string&>();
  };

  static constexpr absl::Duration kPeripheralLostTimeout = absl::Seconds(3);

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

 private:
  struct AdvertisingInfo {
    mediums::BleAdvertisement medium_advertisement;
    PowerLevel power_level;
    bool is_fast_advertisement;
  };

  // An auto stop mechanism to register advertisement id over a life cycle of
  // platform adverting state in `AdvertisingClientState`.
  class AdvertisementKey {
   public:
    explicit AdvertisementKey(BleV2* ble) : ble_(ble) {
      advertisement_ids_.clear();
    }

    void AddAdvertisementId(int advertisement_id) {
      advertisement_ids_.push_back(advertisement_id);
    }

    ~AdvertisementKey() {
      for (const auto& advertisement_id : advertisement_ids_) {
        bool stop_advertising = ble_->medium_.StopAdvertising(advertisement_id);
        NEARBY_LOGS(INFO)
            << "Stop platform BLE advertising with advertisement_id="
            << advertisement_id << " succeed=" << stop_advertising;
      }
      advertisement_ids_.clear();
    }

   private:
    BleV2* ble_;  // Not owned.
    std::vector<int> advertisement_ids_;
  };

  // A store to track the state of multiple service ids (clients).
  class AdvertisingClientState {
   public:
    // Initializes the state for platform advertising.
    void StartAdvertising(const std::string& service_id, BleV2* ble) {
      acting_advertisement_key_ = std::make_unique<AdvertisementKey>(ble);
      acting_service_id_ = service_id;
    }

    // Updates the state of advertisement id by each succeeded case of
    // platform StartAdvertising.
    void AddAdvertisementId(int advertisement_id) {
      if (acting_advertisement_key_ == nullptr) return;
      acting_advertisement_key_->AddAdvertisementId(advertisement_id);
    }

    // Resets the platform advertising state. It will destroy the
    // `acting_advertisement_key` in which runs stopping platform advertising.
    void StopAdvertising() {
      acting_advertisement_key_.reset();
      acting_service_id_.clear();
    }

    // Gets current acting service id.
    std::string GetActingServiceId() const { return acting_service_id_; }

    // Checks if the service id is in tracking store and acting one.
    bool IsDeactivated(const std::string& service_id) {
      return advertising_infos_.contains(service_id) &&
             (service_id != acting_service_id_);
    }

    // This is to get the next available service id to do re-start advertising
    // by the last time advertising.
    std::string GetNextAvailableServiceId() { return service_ids_.back(); }
    AdvertisingInfo* GetAdvertisingInfo(const std::string& service_id) {
      const auto& it = advertising_infos_.find(service_id);
      if (it == advertising_infos_.end()) {
        return nullptr;
      }
      return &it->second;
    }

    // Gets all the service_id in store.
    std::vector<std::string> GetServiceIds() const { return service_ids_; }

    bool Empty() const { return advertising_infos_.empty(); }
    void Clear() {
      advertising_infos_.clear();
      service_ids_.clear();
      acting_service_id_.clear();
      acting_advertisement_key_.reset();
    }
    void Insert(const std::string& service_id,
                const AdvertisingInfo& advertising_info) {
      advertising_infos_.insert({service_id, advertising_info});
      service_ids_.push_back(service_id);
    }
    void Erase(const std::string& service_id) {
      advertising_infos_.erase(service_id);
      const auto it =
          std::find(service_ids_.begin(), service_ids_.end(), service_id);
      if (it != service_ids_.end()) {
        service_ids_.erase(it);
      }
    }
    // We check `advertising_infos_` only even `service_ids_` is co-existed.
    bool Contains(const std::string& service_id) const {
      return advertising_infos_.contains(service_id);
    }

   private:
    // A map service_id to AdvertisingInfo.
    absl::flat_hash_map<std::string, AdvertisingInfo> advertising_infos_;
    // Acts as a stack for advertising service ids. The last service_id inserted
    // for advertising will be popped up firstly to re-start the advertising.
    // It is co-existed with advertising_infos.
    std::vector<std::string> service_ids_;
    // Current acting service_id for advertising.
    std::string acting_service_id_;
    // Current acting advertisement_key for advertising.
    std::unique_ptr<AdvertisementKey> acting_advertisement_key_;
  };

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

  api::ble_v2::TxPowerLevel PowerLevelToTxPowerLevel(PowerLevel power_level);
  int GenerateAdvertismentId() { return Prng().NextInt32(); }

  void RunOnBleThread(Runnable runnable);

  static constexpr int kMaxConcurrentAcceptLoops = 5;

  SingleThreadExecutor serial_executor_;
  ScheduledExecutor alarm_executor_;

  mutable Mutex mutex_;
  BluetoothRadio& radio_ ABSL_GUARDED_BY(mutex_);
  BluetoothAdapter& adapter_ ABSL_GUARDED_BY(mutex_);
  BleV2Medium medium_ ABSL_GUARDED_BY(mutex_){adapter_};
  AdvertisingClientState advertising_client_state_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<GattServer> gatt_server_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<int, std::pair<std::string, ByteArray>>
      gatt_advertisements_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<api::ble_v2::GattCharacteristic>
      hosted_gatt_characteristics_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<std::string> scanned_service_ids_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<CancelableAlarm> lost_alarm_;
  mediums::DiscoveredPeripheralTracker discovered_peripheral_tracker_
      ABSL_GUARDED_BY(mutex_);

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
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_H_
