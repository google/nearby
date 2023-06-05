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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_PAIRING_PAIRER_BROKER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_PAIRING_PAIRER_BROKER_H_

#include <optional>

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"

namespace nearby {
namespace fastpair {

// The PairerBroker is the entry point for the Fast Pair Pairing component.
// It is responsible for brokering the 'pair to device' calls to
// the correct concrete Pairer implementation, and exposing an observer pattern
// for other components to become aware of pairing results.

class PairerBroker {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnHandshakeComplete(FastPairDevice& device) {}
    virtual void OnDevicePaired(FastPairDevice& device) {}
    virtual void OnAccountKeyWrite(FastPairDevice& device,
                                   std::optional<PairFailure> error) {}
    virtual void OnPairingComplete(FastPairDevice& device) {}
    virtual void OnPairFailure(FastPairDevice& device, PairFailure failure) {}
  };

  virtual ~PairerBroker() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual void PairDevice(FastPairDevice& device) = 0;
  virtual bool IsPairing() = 0;
  virtual void StopPairing() = 0;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_PAIRING_PAIRER_BROKER_H_
