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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_SCANNER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_SCANNER_H_

#include <memory>

#include "fastpair/common/fast_pair_device.h"
#include "internal/platform/bluetooth_adapter.h"

namespace nearby {
namespace fastpair {

// This registers a BluetoothLowEnergy Scanner and exposes the Fast Pair
// devices found/lost events to its observers.
class FastPairScanner {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnDeviceFound(const BlePeripheral& peripheral) = 0;
    virtual void OnDeviceLost(const BlePeripheral& peripheral) = 0;
  };

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  virtual ~FastPairScanner() = default;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_FASTPAIR_FAST_PAIR_SCANNER_H_
