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

#include "fastpair/internal/fast_pair_seeker_impl.h"

#include <memory>
#include <utility>

#include "absl/status/status.h"

namespace nearby {
namespace fastpair {

absl::Status FastPairSeekerImpl::StartInitialPairing(
    FastPairDevice& device, const InitialPairingParam& params,
    PairingCallback callback) {
  return absl::UnimplementedError("StartInitialPairing");
}

absl::Status FastPairSeekerImpl::StartSubsequentPairing(
    FastPairDevice& device, const SubsequentPairingParam& params,
    PairingCallback callback) {
  return absl::UnimplementedError("StartSubsequentPairing");
}

absl::Status FastPairSeekerImpl::StartRetroactivePairing(
    FastPairDevice& device, const RetroactivePairingParam& param,
    PairingCallback callback) {
  return absl::UnimplementedError("StartRetroactivePairing");
}

absl::Status FastPairSeekerImpl::StartFastPairScan() {
  // TODO(jsobczak): Replace with actual implementation
  auto device = std::make_unique<FastPairDevice>(
      "model_id", "11:22:33:44:55:66", Protocol::kFastPairInitialPairing);
  test_device_ = device.get();
  callbacks_.on_device_added(std::move(device));
  callbacks_.on_initial_discovery(*test_device_, {});
  return absl::OkStatus();
}

absl::Status FastPairSeekerImpl::StopFastPairScan() {
  // TODO(jsobczak): Replace with actual implementation
  callbacks_.on_device_lost(*test_device_);
  return absl::OkStatus();
}

}  // namespace fastpair
}  // namespace nearby
