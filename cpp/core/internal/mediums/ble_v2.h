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
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "third_party/nearby/cpp/core/internal/mediums/bluetooth_radio.h"
#include "third_party/nearby/cpp/platform/base/byte_array.h"
#include "third_party/nearby/cpp/platform/public/mutex.h"

namespace location {
namespace nearby {
namespace connections {

class BleV2 {
 public:
  explicit BleV2(BluetoothRadio& bluetooth_radio);
  ~BleV2() = default;

 private:
  // Container that stores info about a GATT advertisement.
  struct GattAdvertisementInfo {
    bool Empty() const { return gatt_advertisements.empty(); }
    void Clear() { gatt_advertisements.clear(); }
    void Add(int slot, std::pair<std::string, ByteArray> gatt_advertisement) {
      gatt_advertisements.insert({slot, gatt_advertisement});
    }
    void Remove(int slot) { gatt_advertisements.erase(slot); }
    bool Existed(int slot) const {
      return gatt_advertisements.contains(slot);
    }
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

  static ByteArray GenerateAdvertisementHash(
      const ByteArray& advertisement_bytes);
  static ByteArray GenerateHash(const std::string& source, size_t size);
  static ByteArray GenerateDeviceToken();

  ByteArray CreateAdvertisementHeader() ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;
  BluetoothRadio& radio_ ABSL_GUARDED_BY(mutex_);
  BluetoothAdapter& adapter_ ABSL_GUARDED_BY(mutex_){
      radio_.GetBluetoothAdapter()};
  GattAdvertisementInfo gatt_advertisement_info ABSL_GUARDED_BY(mutex_);
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_H_
