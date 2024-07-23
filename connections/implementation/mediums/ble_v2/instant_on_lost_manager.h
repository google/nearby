// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_BLE_V2_INSTANT_ON_LOST_MANAGER_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_BLE_V2_INSTANT_ON_LOST_MANAGER_H_

#include <list>
#include <memory>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/mutex.h"
#include "internal/platform/scheduled_executor.h"

namespace nearby {
namespace connections {
namespace mediums {

class InstantOnLostManager {
 public:
  InstantOnLostManager() = default;
  ~InstantOnLostManager() = default;

  void OnAdvertisingStarted(const std::string& service_id,
                            const ByteArray& advertisement_data)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnAdvertisingStopped(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool Shutdown() ABSL_LOCKS_EXCLUDED(mutex_);

  std::list<std::string> GetOnLostHashes() ABSL_LOCKS_EXCLUDED(mutex_);
  bool IsOnLostAdvertising() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  struct OnLostAdvertisementHashInfo {
    absl::Time start_time;
    std::string hash;

    bool operator==(const OnLostAdvertisementHashInfo& info) const {
      return this->start_time == info.start_time && this->hash == info.hash;
    }
  };

  bool StartInstantOnLostAdvertisement() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool StopOnLostAdvertising() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void RemoveExpiredOnLostAdvertisements()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  std::list<std::string> GetOnLostHashesInternal()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Mutex mutex_;

  std::unique_ptr<CancelableAlarm> stop_advertising_alarm_
      ABSL_GUARDED_BY(mutex_);

  // BLE medium used for lost packet advertising.
  BluetoothAdapter adapter_ ABSL_GUARDED_BY(mutex_);
  BleV2Medium ble_medium_ ABSL_GUARDED_BY(mutex_) = BleV2Medium{adapter_};
  bool is_on_lost_advertising_ ABSL_GUARDED_BY(mutex_) = false;

  bool is_shutdown_ ABSL_GUARDED_BY(mutex_) = false;

  ScheduledExecutor executor_ ABSL_GUARDED_BY(mutex_);

  // Active on lost advertising, the maximum size is 5.
  std::list<OnLostAdvertisementHashInfo> active_on_lost_advertising_list_
      ABSL_GUARDED_BY(mutex_);

  // Map of service ID to advertisement data hash.
  absl::flat_hash_map<std::string, ByteArray> active_advertising_map_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_BLE_V2_INSTANT_ON_LOST_MANAGER_H_
