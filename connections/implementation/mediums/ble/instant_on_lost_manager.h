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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_BLE_INSTANT_ON_LOST_MANAGER_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_BLE_INSTANT_ON_LOST_MANAGER_H_

#include <list>
#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/ble.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/scheduled_executor.h"

namespace nearby {
namespace connections {
namespace mediums {

// This InstantOnLostManager class is thread-compatible, and all methods are
// executed on a single thread.
class InstantOnLostManager {
 public:
  InstantOnLostManager() = default;
  ~InstantOnLostManager() = default;

  void OnAdvertisingStarted(const std::string& service_id,
                            const ByteArray& advertisement_data);
  void OnAdvertisingStopped(const std::string& service_id);

  bool Shutdown();

  // For testing only.
  std::list<std::string> GetOnLostHashesForTesting();
  bool IsOnLostAdvertisingForTesting();

 private:
  struct OnLostAdvertisementHashInfo {
    absl::Time start_time;
    std::string hash;

    bool operator==(const OnLostAdvertisementHashInfo& info) const {
      return this->start_time == info.start_time && this->hash == info.hash;
    }
  };

  bool StartInstantOnLostAdvertisement();
  bool StopOnLostAdvertising();
  void RemoveExpiredOnLostAdvertisements();
  std::list<std::string> GetOnLostHashesInternal();

  void RunOnInstantOnLostThread(absl::string_view task_name,
                                absl::AnyInvocable<void()> task);

  bool is_shutdown_ = false;
  ScheduledExecutor service_thread_;
  std::unique_ptr<CancelableAlarm> stop_advertising_alarm_;

  // BLE medium used for lost packet advertising.
  BluetoothAdapter adapter_;
  BleMedium ble_medium_ = BleMedium{adapter_};
  bool is_on_lost_advertising_ = false;
  absl::Time last_advertising_start_time_ = absl::InfinitePast();

  // Active on lost advertising, the maximum size is 5.
  std::list<OnLostAdvertisementHashInfo> active_on_lost_advertising_list_;

  // Map of service ID to advertisement data hash.
  absl::flat_hash_map<std::string, ByteArray> active_advertising_map_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_BLE_INSTANT_ON_LOST_MANAGER_H_
