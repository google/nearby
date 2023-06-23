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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_RETROACTIVE_RETROACTIVE_PAIRING_DETECTOR_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_RETROACTIVE_RETROACTIVE_PAIRING_DETECTOR_H_

#include "fastpair/common/fast_pair_device.h"
namespace nearby {
namespace fastpair {
// A RetroactivePairingDetector instance is responsible for detecting Fast Pair
// devices that can be paired retroactively, and notifying observers of this
// device.
class RetroactivePairingDetector {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnRetroactivePairFound(FastPairDevice& device) = 0;
  };

  virtual ~RetroactivePairingDetector() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_RETROACTIVE_RETROACTIVE_PAIRING_DETECTOR_H_
