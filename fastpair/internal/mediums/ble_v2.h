// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_MEDIUMS_BLE_V2_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_MEDIUMS_BLE_V2_H_

#include <memory>
#include <string>

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/internal/mediums/bluetooth_radio.h"
#include "fastpair/internal/mediums/robust_gatt_client.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {
namespace fastpair {

// Provides the operations that can be performed on the Bluetooth Low Energy
// (BLE_V2) medium.
class BleV2 {
 public:
  explicit BleV2(BluetoothRadio& bluetooth_radio);
  ~BleV2() = default;

  // Returns true, if BleV2 communications are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns a new GattClient connection to a gatt server.
  std::unique_ptr<RobustGattClient> ConnectToGattServer(
      absl::string_view ble_address, RobustGattClient::ConnectionParams params,
      RobustGattClient::ConnectionStatusCallback connection_status_callback);

 private:
  // Same as IsAvailable(), but must be called with mutex_ held.
  bool IsAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;
  BluetoothRadio& radio_ ABSL_GUARDED_BY(mutex_);
  BluetoothAdapter& adapter_ ABSL_GUARDED_BY(mutex_){
      radio_.GetBluetoothAdapter()};
  BleV2Medium medium_ ABSL_GUARDED_BY(mutex_){adapter_};
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_MEDIUMS_BLE_V2_H_
