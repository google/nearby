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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MEDIUMS_MEDIUMS_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MEDIUMS_MEDIUMS_H_

#include "internal/platform/bluetooth_adapter.h"
#include "presence/implementation/mediums/ble.h"

namespace nearby {
namespace presence {

/*
 * This class owns medium instance like Ble and etc. And the instance of
 * this class will be owned in {@code ServiceControllerImpl}.
 */
class Mediums {
 public:
  // Returns a handle to the Ble medium.
  Ble& GetBle() { return ble_; }

 private:
  nearby::BluetoothAdapter adapter_;
  Ble ble_{adapter_};
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MEDIUMS_MEDIUMS_H_
