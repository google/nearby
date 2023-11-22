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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_H_
#define CORE_INTERNAL_MEDIUMS_BLE_H_

#include <cstdint>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/listeners.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/ble.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/mutex.h"

namespace nearby {
namespace connections {

class Ble {
 public:
  using DiscoveredPeripheralCallback = BleMedium::DiscoveredPeripheralCallback;
  using AcceptedConnectionCallback = BleMedium::AcceptedConnectionCallback;

  explicit Ble(BluetoothRadio& bluetooth_radio);
  ~Ble() = default;

  // Returns true, if Ble communications are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Sets custom advertisement data, and then enables Ble advertising.
  // Returns true, if data is successfully set, and false otherwise.
  bool StartAdvertising(const std::string& service_id,
                        const ByteArray& advertisement_bytes,
                        const std::string& fast_advertisement_service_uuid)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables Ble advertising.
  bool StopAdvertising(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // (TODO:hais) remove this after ble_v2 refactor
  // Sets custom advertisement data, and then enables Ble advertising.
  // Returns true, if data is successfully set, and false otherwise.
  bool StartLegacyAdvertising(
      const std::string& service_id, const std::string& local_endpoint_id,
      const std::string& fast_advertisement_service_uuid)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // (TODO:hais) remove this after ble_v2 refactor
  // Disables Ble advertising.
  bool StopLegacyAdvertising(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsAdvertising(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Enables Ble scanning mode. Will report any discoverable peripherals in
  // range through a callback. Returns true, if scanning mode was enabled,
  // false otherwise.
  bool StartScanning(const std::string& service_id,
                     const std::string& fast_advertisement_service_uuid,
                     DiscoveredPeripheralCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables Ble discovery mode.
  bool StopScanning(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsScanning(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

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

  // Establishes connection to Ble peripheral that was might be started on
  // another peripheral with StartAcceptingConnections() using the same
  // service_id. Blocks until connection is established, or server-side is
  // terminated. Returns socket instance. On success, BleSocket.IsValid() return
  // true.
  BleSocket Connect(BlePeripheral& peripheral, const std::string& service_id,
                    CancellationFlag* cancellation_flag)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  struct AdvertisingInfo {
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

  struct AcceptingConnectionsInfo {
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

  static constexpr int kMaxAdvertisementLength = 512;

  static ByteArray GenerateHash(const std::string& source, size_t size);
  static ByteArray GenerateDeviceToken();

  // Same as IsAvailable(), but must be called with mutex_ held.
  bool IsAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsAdvertising(), but must be called with mutex_ held.
  bool IsAdvertisingLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsDiscovering(), but must be called with mutex_ held.
  bool IsScanningLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsAcceptingConnections(), but must be called with mutex_ held.
  bool IsAcceptingConnectionsLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Extract connection advertisement from medium advertisement.
  ByteArray UnwrapAdvertisementBytes(
      const ByteArray& medium_advertisement_data);

  mutable Mutex mutex_;
  BluetoothRadio& radio_ ABSL_GUARDED_BY(mutex_);
  BluetoothAdapter& adapter_ ABSL_GUARDED_BY(mutex_){
      radio_.GetBluetoothAdapter()};
  BleMedium medium_ ABSL_GUARDED_BY(mutex_){adapter_};
  AdvertisingInfo advertising_info_ ABSL_GUARDED_BY(mutex_);
  ScanningInfo scanning_info_ ABSL_GUARDED_BY(mutex_);
  DiscoveredPeripheralCallback discovered_peripheral_callback_;
  AcceptingConnectionsInfo accepting_connections_info_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLE_H_
