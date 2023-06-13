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

#include <ios>
#include <memory>
#include <optional>
#include <utility>

#include "absl/status/status.h"
#include "fastpair/fast_pair_events.h"
#include "fastpair/pairing/pairer_broker_impl.h"
#include "fastpair/scanning/scanner_broker_impl.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

FastPairSeekerImpl::FastPairSeekerImpl(ServiceCallbacks callbacks,
                                       SingleThreadExecutor* executor,
                                       FastPairDeviceRepository* devices)
    : callbacks_(std::move(callbacks)), executor_(executor), devices_(devices) {
  pairer_broker_ = std::make_unique<PairerBrokerImpl>(mediums_, executor_);
  pairer_broker_->AddObserver(this);
}

absl::Status FastPairSeekerImpl::StartInitialPairing(
    const FastPairDevice& device, const InitialPairingParam& params,
    PairingCallback callback) {
  return absl::UnimplementedError("StartInitialPairing");
}

absl::Status FastPairSeekerImpl::StartSubsequentPairing(
    const FastPairDevice& device, const SubsequentPairingParam& params,
    PairingCallback callback) {
  return absl::UnimplementedError("StartSubsequentPairing");
}

absl::Status FastPairSeekerImpl::StartRetroactivePairing(
    const FastPairDevice& device, const RetroactivePairingParam& param,
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

// PairerBroker:Observer::OnDevicePaired
void FastPairSeekerImpl::OnDevicePaired(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": " << device;
}

// PairerBroker:Observer::OnAccountKeyWrite
void FastPairSeekerImpl::OnAccountKeyWrite(FastPairDevice& device,
                                           std::optional<PairFailure> error) {
  if (error.has_value()) {
    NEARBY_LOGS(INFO) << __func__ << ": Device=" << device
                      << ",Error=" << error.value();
    return;
  }

  NEARBY_LOGS(INFO) << __func__ << ": Device=" << device;
  if (device.GetProtocol() == Protocol::kFastPairRetroactivePairing) {
    // TODO: UI ShowAssociateAccount
  }
}

// PairerBroker:Observer::OnPairingComplete
void FastPairSeekerImpl::OnPairingComplete(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": " << device;
}

// PairerBroker:Observer::OnPairFailure
void FastPairSeekerImpl::OnPairFailure(FastPairDevice& device,
                                       PairFailure failure) {
  NEARBY_LOGS(INFO) << __func__ << ": " << device
                    << " with PairFailure: " << failure;
}

void FastPairSeekerImpl::SetIsScreenLocked(bool locked) {
  executor_->Execute(
      "on_lock_state_changed",
      [this, locked]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
        NEARBY_LOGS(INFO) << __func__ << ": Screen lock state changed. ( "
                          << std::boolalpha << locked << ")";
        is_screen_locked_ = locked;
        InvalidateScanningState();
      });
}

void FastPairSeekerImpl::InvalidateScanningState() {
  // Stop scanning when screen is off.
  if (is_screen_locked_) {
    absl::Status status = StopFastPairScan();
    NEARBY_LOGS(VERBOSE) << __func__
                         << ": Stopping scanning because the screen is locked.";
    return;
  }

  // TODO(b/275452353): Check if bluetooth and fast pair is enabled

  // Screen is on, Bluetooth is enabled, and Fast Pair is enabled, start
  // scanning.
  absl::Status status = StartFastPairScan();
}
}  // namespace fastpair
}  // namespace nearby
