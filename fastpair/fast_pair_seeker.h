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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_SEEKER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_SEEKER_H_

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "fastpair/common/fast_pair_device.h"

namespace nearby {
namespace fastpair {

// Pairing result callback.
struct PairingCallback {
  absl::AnyInvocable<void(const FastPairDevice&, absl::Status)>
      on_pairing_result =
          [](const FastPairDevice& device, absl::Status status) {};
};

// Initial Pairing parameters.
struct InitialPairingParam {};

// Subsequent Pairing parameters.
struct SubsequentPairingParam {};

// Retroactive Pairing parameters.
struct RetroactivePairingParam {};

// Finish Retroactive Pairing parameters.
struct FinishRetroactivePairingParam {
  // Save the negotiated Account Key or abandon it and forget the Fast Pair
  // Device.
  bool save_account_key = false;
};

// Fast Pair Seeker API available to plugins.
class FastPairSeeker {
 public:
  virtual ~FastPairSeeker() = default;

  // Starts asynchronous initial pairing flow. This pairing flow is used with
  // devices that we see for the first time.
  //
  // Returns an error if pairing flow could not be started. Otherwise, the
  // pairing result will be returned via the `callback`.
  virtual absl::Status StartInitialPairing(const FastPairDevice& device,
                                           const InitialPairingParam& params,
                                           PairingCallback callback) = 0;

  // Starts asynchronous subsequent pairing flow. This pairing flow is used when
  // device is an already known peripheral. The user paired with that device in
  // the past, perhaps using a different seeker and `device` already has an
  // Account Key.
  //
  // Returns an error if pairing flow could not be started. Otherwise, the
  // pairing result will be returned via the `callback`.
  virtual absl::Status StartSubsequentPairing(
      const FastPairDevice& device, const SubsequentPairingParam& params,
      PairingCallback callback) = 0;

  // Starts asynchronous retroactive pairing flow. This pairing flow is used
  // when the user has paired manually with a new device and we want to
  // retroactively exchange an Account Key. See:
  // https://developers.google.com/nearby/fast-pair/specifications/extensions/retroactiveacctkey
  // The retroactive pairing flow should be started immediately after the
  // pairing event, before we had a chance to ask for the user for permission.
  // Therefore, the retroactive is split into two stages. The client should call
  // `StartRetroactivePairing()` first. When the pairing result returned via
  // `callback` is successful, the client should ask the user for permission to
  // save the account key. Finally, the client should call
  // `FinishRetroactivePairing()` with the user's decision to complete the
  // retroactive pairing flow.
  //
  // Returns an error if pairing flow could not be started. Otherwise, the
  // pairing result will be returned via the `callback`.
  virtual absl::Status StartRetroactivePairing(
      const FastPairDevice& device, const RetroactivePairingParam& param,
      PairingCallback callback) = 0;

  // Finishes the retroactive pairing flow.
  //
  // Returns an error if the flow could not be started. Otherwise, the
  // result will be returned via the `callback`.
  virtual absl::Status FinishRetroactivePairing(
      const FastPairDevice& device, const FinishRetroactivePairingParam& param,
      PairingCallback callback) = 0;
};
}  // namespace fastpair
}  // namespace nearby
#endif  // THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_SEEKER_H_
