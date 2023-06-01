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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_PAIRING_FASTPAIR_FAST_PAIR_PAIRER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_PAIRING_FASTPAIR_FAST_PAIR_PAIRER_H_

#include "absl/functional/any_invocable.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
namespace nearby {
namespace fastpair {

// A FastPairPairer instance is responsible for the pairing procedure to a
// single device.  Pairing begins on instantiation.
class FastPairPairer {
 public:
  // Triggered when paired with the remote device.
  using OnPairedCallback = absl::AnyInvocable<void(FastPairDevice& device)>;
  // Triggered when failed to pair with the remote device.
  using OnPairingFailedCallback =
      absl::AnyInvocable<void(FastPairDevice& device, PairFailure failure)>;
  // Triggered when completed the whole pairing process,
  // including pairing with the remote device and writing the accountkey to it.
  using OnPairingCompletedCallback =
      absl::AnyInvocable<void(FastPairDevice& device)>;
  // Triggered when failed to write accountkey to the remote device.
  using OnAccountKeyFailureCallback =
      absl::AnyInvocable<void(FastPairDevice& device, PairFailure failure)>;

  virtual ~FastPairPairer() = default;

  virtual void StartPairing() = 0;
  virtual bool IsPaired() = 0;
  virtual bool CancelPairing() = 0;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_PAIRING_FASTPAIR_FAST_PAIR_PAIRER_H_
