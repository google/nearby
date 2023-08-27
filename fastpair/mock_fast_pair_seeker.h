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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_MOCK_FAST_PAIR_SEEKER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_MOCK_FAST_PAIR_SEEKER_H_

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "fastpair/fast_pair_seeker.h"

namespace nearby {
namespace fastpair {

class MockFastPairSeeker : public FastPairSeeker {
 public:
  MOCK_METHOD(absl::Status, StartInitialPairing,
              (const FastPairDevice& device, const InitialPairingParam& params,
               PairingCallback callback),
              (override));

  MOCK_METHOD(absl::Status, StartSubsequentPairing,
              (const FastPairDevice& device,
               const SubsequentPairingParam& params, PairingCallback callback),
              (override));

  MOCK_METHOD(absl::Status, StartRetroactivePairing,
              (const FastPairDevice& device,
               const RetroactivePairingParam& param, PairingCallback callback),
              (override));
  MOCK_METHOD(absl::Status, FinishRetroactivePairing,
              (const FastPairDevice& device,
               const FinishRetroactivePairingParam& param,
               PairingCallback callback),
              (override));
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_MOCK_FAST_PAIR_SEEKER_H_
