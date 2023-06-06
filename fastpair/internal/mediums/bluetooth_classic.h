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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_MEDIUMS_BLUETOOTH_CLASSIC_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_MEDIUMS_BLUETOOTH_CLASSIC_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "fastpair/internal/mediums/bluetooth_radio.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/mutex.h"
namespace nearby {
namespace fastpair {

class BluetoothClassic {
 public:
  explicit BluetoothClassic(BluetoothRadio& bluetooth_radio);
  ~BluetoothClassic() = default;

  // Returns true, if BT communications are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  void AddObserver(BluetoothClassicMedium::Observer* observer);
  void RemoveObserver(BluetoothClassicMedium::Observer* observer);

  // Enables BT discovery mode to observer any discoverable device in range.
  // Returns true, if discovery mode was enabled, false otherwise.
  bool StartDiscovery() ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables BT discovery mode.
  // Returns true, if discovery mode was previously enabled, false otherwise.
  bool StopDiscovery() ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns a new BluetoothPairing instance to handle the pairing process
  // with the remote device.
  std::unique_ptr<BluetoothPairing> CreatePairing(
      absl::string_view public_address);

  BluetoothClassicMedium& GetMedium() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // Same as IsAvailable(), but must be called with mutex_ held.
  bool IsAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Returns true if device is currently in discovery mode.
  bool IsDiscovering() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;
  BluetoothRadio& radio_ ABSL_GUARDED_BY(mutex_);
  BluetoothAdapter& adapter_ ABSL_GUARDED_BY(mutex_){
      radio_.GetBluetoothAdapter()};
  BluetoothClassicMedium medium_ ABSL_GUARDED_BY(mutex_){adapter_};
  bool is_scanning ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_MEDIUMS_BLUETOOTH_CLASSIC_H_
