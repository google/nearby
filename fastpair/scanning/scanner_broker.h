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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_SCANNER_BROKER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_SCANNER_BROKER_H_

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/protocol.h"

namespace nearby {
namespace fastpair {

// The ScannerBroker is the entry point for the Scanning component in the Fast
// Pair. It is responsible for brokering the start/stop scanning calls
// to the correct concrete Scanner implementation, and exposing an observer
// pattern for other components to become aware of device found/lost events.
class ScannerBroker {
 public:
  // Observes the activity of the scan broker.
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnDeviceFound(const FastPairDevice& device) = 0;
    virtual void OnDeviceLost(const FastPairDevice& device) = 0;
  };

  virtual ~ScannerBroker() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual void StartScanning(Protocol protocol) = 0;
  virtual void StopScanning(Protocol protocol) = 0;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SCANNING_SCANNER_BROKER_H_
