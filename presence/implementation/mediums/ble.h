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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MEDIUMS_BLE_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MEDIUMS_BLE_H_

#include "internal/platform/ble_v2.h"

namespace nearby {
namespace presence {

/*
 * This Ble class utilizes platform/ble_v2 BleV2Medium, provides ble functions
 * for presence logic layer to invoke.
 * This class would have states like if ble is available or not, if it's doing
 * broadcast/scan.
 */
class Ble {
 public:
  Ble() = default;
  ~Ble() = default;
  // TODO(hais): add expected function set.
 private:
  location::nearby::BleV2Medium medium_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MEDIUMS_BLE_H_
