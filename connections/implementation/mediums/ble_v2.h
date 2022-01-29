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
#include "absl/strings/string_view.h"
#include "connections/advertising_options.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_callback.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_tracker.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "third_party/nearby/cpp/cal/api/ble.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/single_thread_executor.h"

namespace location {
namespace nearby {
namespace connections {

class BleV2 final {
 public:
  using ServerGattConnectionCallback =
      BleV2Medium::ServerGattConnectionCallback;
  using DiscoveredPeripheralCallback = mediums::DiscoveredPeripheralCallback;

  explicit BleV2(BluetoothRadio& bluetooth_radio);

  // Returns true, if Ble communications are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts Ble advertising, delivering additional information through a GATT
  // server.
  bool StartAdvertising(const std::string& service_id,
                        const ByteArray& advertisement_bytes,
                        PowerLevel power_level,
                        const std::string& fast_advertisement_service_uuid)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables Ble advertising.
  bool StopAdvertising(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAdvertising(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts scanning for BLE advertisements (if it is possible for the device).
  bool StartScanning(const std::string& service_id,
                     DiscoveredPeripheralCallback callback,
                     PowerLevel power_level,
                     const std::string& fast_advertisement_service_uuid)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Stops scanning for BLE advertisements.
  bool StopScanning(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsScanning(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if this object owns a valid platform implementation.
  bool IsMediumValid() const ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    return medium_.IsValid();
  }

  // Returns true if this object has a valid BluetoothAdapter reference.
  bool IsAdapterValid() const ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    return adapter_.IsValid();
  }

 private:
  struct AdvertisingInfo {
    bool Empty() const { return service_ids.empty(); }
    void Clear() { service_ids.clear(); }
    void Add(const std::string& service_id) { service_ids.insert(service_id); }
    void Remove(const std::string& service_id) {
      service_ids.erase(service_id);
    }
    bool Existed(const std::string& service_id) const {
      return service_ids.contains(service_id);
    }

    absl::flat_hash_set<std::string> service_ids;
  };

  struct GattServerInfo {
    GattServer gatt_server;
  };

  struct ScanningInfo {
    bool Empty() const { return service_ids.empty(); }
    void Clear() { service_ids.clear(); }
    void Add(const std::string& service_id) { service_ids.emplace(service_id); }
    void Remove(const std::string& service_id) {
      service_ids.erase(service_id);
    }
    bool Existed(const std::string& service_id) const {
      return service_ids.contains(service_id);
    }

    absl::flat_hash_set<std::string> service_ids;
  };

  // Container that stores info about a GATT advertisement.
  struct GattAdvertisementInfo {
    bool Empty() const { return gatt_advertisements.empty(); }
    void Clear() { gatt_advertisements.clear(); }
    void Add(int slot, std::pair<std::string, ByteArray> gatt_advertisement) {
      gatt_advertisements.insert({slot, gatt_advertisement});
    }
    void Remove(int slot) { gatt_advertisements.erase(slot); }
    bool Existed(int slot) const { return gatt_advertisements.contains(slot); }
    std::pair<std::string, ByteArray> GetAdvertisement(int slot) {
      const auto& it = gatt_advertisements.find(slot);
      if (it == gatt_advertisements.end()) {
        return {};
      }
      return it->second;
    }

    // A map of slot -> pair of {service_id, gatt_advertisement}
    absl::flat_hash_map<int, std::pair<std::string, ByteArray>>
        gatt_advertisements;
  };

  static constexpr int kMaxAdvertisementLength = 512;
  static constexpr int kDummyServiceIdLength = 128;
  static constexpr absl::string_view kCopresenceServiceUuid =
      "0000FEF3-0000-1000-8000-00805F9B34FB";

  // Same as IsAvailable(), but must be called with mutex_ held.
  bool IsAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsAdvertising(), but must be called with mutex_ held.
  bool IsAdvertisingLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsScanning(), but must be called with mutex_ held.
  bool IsScanningLocked(const std::string& service_id)
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
      BlePeripheralV2& peripheral, int num_slots,
      mediums::AdvertisementReadResult* advertisement_read_result)
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<mediums::AdvertisementReadResult>
  InternalReadAdvertisementFromGattServerLocked(
      BlePeripheralV2& peripheral, int num_slots,
      std::unique_ptr<mediums::AdvertisementReadResult>
          advertisement_read_result) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool StopAdvertisementGattServerLocked()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  ByteArray CreateAdvertisementHeader() ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  std::string GenerateAdvertisementUuid(int slot);

  // Extract connection advertisement from medium advertisement.
  ByteArray UnwrapAdvertisementBytes(
      const ByteArray& medium_advertisement_data);
  ::nearby::cal::PowerMode PowerLevelToPowerMode(PowerLevel power_level);

  void RunOnBleThread(Runnable runnable);
  mediums::DiscoveredPeripheralTracker::AdvertisementFetcher
  GetAdvertisementFetcher();

  static constexpr ::nearby::cal::api::BleMedium::Mtu kDefaultMtu = 512;

  mutable Mutex mutex_;
  SingleThreadExecutor serial_executor_;
  BluetoothRadio& radio_ ABSL_GUARDED_BY(mutex_);
  BluetoothAdapter& adapter_ ABSL_GUARDED_BY(mutex_);
  BleV2Medium medium_ ABSL_GUARDED_BY(mutex_){adapter_};
  AdvertisingInfo advertising_info_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<GattServerInfo> gatt_server_info_ ABSL_GUARDED_BY(mutex_);
  GattAdvertisementInfo gatt_advertisement_info_ ABSL_GUARDED_BY(mutex_);
  ScanningInfo scanning_info_ ABSL_GUARDED_BY(mutex_);
  DiscoveredPeripheralCallback discovered_peripheral_callback_;
  std::unique_ptr<mediums::DiscoveredPeripheralTracker>
      discovered_peripheral_tracker_ =
          absl::make_unique<mediums::DiscoveredPeripheralTracker>();
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_H_
