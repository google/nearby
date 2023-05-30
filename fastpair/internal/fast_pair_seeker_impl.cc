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
#include "fastpair/fast_pair_events.h"
#include "fastpair/scanning/scanner_broker_impl.h"
#include "internal/platform/single_thread_executor.h"

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
  if (scanning_session_ != nullptr) {
    return absl::AlreadyExistsError("already scanning");
  }
  scanner_ = std::make_unique<ScannerBrokerImpl>(mediums_, executor_, devices_);
  scanner_->AddObserver(this);
  scanning_session_ =
      scanner_->StartScanning(Protocol::kFastPairInitialPairing);
  return absl::OkStatus();
}

absl::Status FastPairSeekerImpl::StopFastPairScan() {
  if (scanning_session_ == nullptr) {
    return absl::NotFoundError("scanner is not running");
  }
  scanning_session_.reset();
  scanner_->RemoveObserver(this);
  DestroyOnExecutor(std::move(scanner_), executor_);
  return absl::OkStatus();
}

// ScannerBroker::Observer::OnDeviceFound
void FastPairSeekerImpl::OnDeviceFound(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << "Device found: " << device;
  callbacks_.on_initial_discovery(device, InitialDiscoveryEvent{});
}

// ScannerBroker::Observer::OnDeviceLost
void FastPairSeekerImpl::OnDeviceLost(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << "Device lost: " << device;
}

}  // namespace fastpair
}  // namespace nearby
