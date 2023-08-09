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
#include "absl/strings/str_format.h"
#include "fastpair/common/account_key.h"
#include "fastpair/fast_pair_controller.h"
#include "fastpair/fast_pair_events.h"
#include "fastpair/pairing/pairer_broker_impl.h"
#include "fastpair/scanning/scanner_broker_impl.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/pending_job_registry.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr absl::Duration kCleanupTimeout = absl::Seconds(3);
}  // namespace

FastPairSeekerImpl::FastPairSeekerImpl(ServiceCallbacks callbacks,
                                       SingleThreadExecutor* executor,
                                       AccountManager* account_manager,
                                       FastPairDeviceRepository* devices,
                                       FastPairRepository* repository)
    : callbacks_(std::move(callbacks)),
      executor_(executor),
      account_manager_(account_manager),
      devices_(devices),
      repository_(repository) {
  pairer_broker_ =
      std::make_unique<PairerBrokerImpl>(mediums_, executor_, account_manager_);
  pairer_broker_->AddObserver(this);
  mediums_.GetBluetoothClassic().AddObserver(this);
  retro_detector_ = std::make_unique<RetroactivePairingDetectorImpl>(
      mediums_, devices, account_manager_, executor);
  retro_detector_->AddObserver(this);
}

FastPairSeekerImpl::~FastPairSeekerImpl() {
  NEARBY_LOGS(INFO) << "~FastPairSeekerImpl start";
  pairer_broker_->RemoveObserver(this);
  mediums_.GetBluetoothClassic().RemoveObserver(this);
  auto unused = StopFastPairScan();
  CountDownLatch latch(1);
  executor_->Execute("~FastPairSeekerImpl", [this, latch]() mutable {
    FinishPairing(absl::AbortedError("Pairing terminated"));
    pairer_broker_.reset();
    executor_->Execute("sync", [latch]() mutable { latch.CountDown(); });
  });
  if (!latch.Await(kCleanupTimeout)) {
    NEARBY_LOGS(WARNING) << "Cleanup didn't finish in " << kCleanupTimeout;
  }
  PendingJobRegistry::GetInstance().ListAllJobs();
  NEARBY_LOGS(INFO) << "~FastPairSeekerImpl done";
}

absl::Status FastPairSeekerImpl::StartInitialPairing(
    const FastPairDevice& device, const InitialPairingParam& params,
    PairingCallback callback) {
  if (pairer_broker_->IsPairing()) {
    return absl::AlreadyExistsError("Already pairing");
  }

  pairing_callback_ = std::make_unique<PairingCallback>(std::move(callback));
  device_under_pairing_ = &const_cast<FastPairDevice&>(device);
  device_under_pairing_->StartedPairing(true);
  pairer_broker_->PairDevice(*device_under_pairing_);
  return absl::OkStatus();
}

absl::Status FastPairSeekerImpl::StartSubsequentPairing(
    const FastPairDevice& device, const SubsequentPairingParam& params,
    PairingCallback callback) {
  return absl::UnimplementedError("StartSubsequentPairing");
}

absl::Status FastPairSeekerImpl::StartRetroactivePairing(
    const FastPairDevice& device, const RetroactivePairingParam& param,
    PairingCallback callback) {
  if (pairer_broker_->IsPairing()) {
    return absl::AlreadyExistsError("Already pairing");
  }

  device_under_pairing_ = &const_cast<FastPairDevice&>(device);
  device_under_pairing_->StartedPairing(true);
  controller_ = std::make_unique<FastPairController>(
      &mediums_, device_under_pairing_, executor_);
  retroactive_pair_ = std::make_unique<Retroactive>(controller_.get());
  pairing_callback_ = std::make_unique<PairingCallback>(std::move(callback));
  retroactive_pair_->Pair().AddListener(
      [this](ExceptionOr<absl::Status> result) {
        // TODO(jsobczak): We could/should keep controller_ alive until the
        // client calls FinishRetroactivePairing(), verify that they called
        // Finish for the correct device, and if the user has given consent, we
        // should keep the MessageStream connection open.
        retroactive_pair_.reset();
        controller_.reset();
        FinishPairing(result.result());
      },
      executor_);
  return absl::OkStatus();
}

absl::Status FastPairSeekerImpl::FinishRetroactivePairing(
    const FastPairDevice& device, const FinishRetroactivePairingParam& param,
    PairingCallback callback) {
  if (device.GetProtocol() != Protocol::kFastPairRetroactivePairing) {
    return absl::InvalidArgumentError(
        "Fast Pair Device is not a retroactive pairing device");
  }
  if (!device.GetAccountKey()) {
    return absl::InvalidArgumentError(
        "Fast Pair Device does not have an account key");
  }
  if (!param.save_account_key) {
    executor_->Execute(
        "abandon retro device",
        [this, &device, callback = std::move(callback)]() mutable {
          NEARBY_LOGS(INFO) << "Abandon device on retroactive pairing path";
          callback.on_pairing_result(device, absl::OkStatus());
          devices_->RemoveDevice(&device);
        });
  } else {
    executor_->Execute(
        "save account key",
        [this, &device, callback = std::move(callback)]() mutable {
          NEARBY_LOGS(INFO) << "Save account key for " << device;
          repository_->WriteAccountAssociationToFootprints(
              const_cast<FastPairDevice&>(device),
              [&device,
               callback = std::move(callback)](absl::Status status) mutable {
                callback.on_pairing_result(device, status);
              });
        });
  }
  return absl::OkStatus();
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
  if (IsDeviceUnderPairing(device)) {
    FinishPairing(absl::UnavailableError("Device lost during pairing"));
  }
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
  if (!IsDeviceUnderPairing(device)) {
    NEARBY_LOGS(WARNING) << "unexpected on pair complete callback";
    return;
  }
  FinishPairing(absl::OkStatus());
}

// PairerBroker:Observer::OnPairFailure
void FastPairSeekerImpl::OnPairFailure(FastPairDevice& device,
                                       PairFailure failure) {
  NEARBY_LOGS(INFO) << __func__ << ": " << device
                    << " with PairFailure: " << failure;
  if (!IsDeviceUnderPairing(device)) {
    NEARBY_LOGS(WARNING) << "unexpected on pair failure callback";
    return;
  }
  FinishPairing(
      absl::InternalError(absl::StrFormat("Pairing failed with %v", failure)));
}

bool FastPairSeekerImpl::IsDeviceUnderPairing(const FastPairDevice& device) {
  return device_under_pairing_ == &device;
}

void FastPairSeekerImpl::FinishPairing(absl::Status result) {
  if (pairing_callback_ && device_under_pairing_ != nullptr) {
    pairing_callback_->on_pairing_result(*device_under_pairing_, result);
  }
  pairing_callback_.reset();
  device_under_pairing_ = nullptr;
}

void FastPairSeekerImpl::SetIsScreenLocked(bool locked) {
  NEARBY_LOGS(INFO) << __func__ << ": Screen lock state changed. ( "
                    << std::boolalpha << locked << ")";
  is_screen_locked_ = locked;
  callbacks_.on_screen_event(ScreenEvent{.is_locked = locked});
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

void FastPairSeekerImpl::DeviceAdded(BluetoothDevice& device) {
  NEARBY_LOGS(VERBOSE) << __func__ << "(" << device.GetMacAddress() << ")";
}

void FastPairSeekerImpl::DeviceRemoved(BluetoothDevice& device) {
  NEARBY_LOGS(VERBOSE) << __func__ << "(" << device.GetMacAddress() << ")";
}

void FastPairSeekerImpl::DeviceAddressChanged(BluetoothDevice& device,
                                              absl::string_view old_address) {
  NEARBY_LOGS(VERBOSE) << __func__ << "(" << device.GetMacAddress() << ", "
                       << old_address << ")";
}

void FastPairSeekerImpl::DevicePairedChanged(BluetoothDevice& device,
                                             bool new_paired_status) {
  NEARBY_LOGS(VERBOSE) << __func__ << "(" << device.GetMacAddress() << ", "
                       << new_paired_status << ")";
  // Note, the FP service will be notified about paired events from the
  // retroactive pairing path if `device` is an FP device.
  // TODO(jsobczak): Notify service about unpair events if `device` is a known
  // FP device.
}

void FastPairSeekerImpl::DeviceConnectedStateChanged(BluetoothDevice& device,
                                                     bool connected) {
  NEARBY_LOGS(VERBOSE) << __func__ << "(" << device.GetMacAddress() << ", "
                       << connected << ")";
}

void FastPairSeekerImpl::OnRetroactivePairFound(FastPairDevice& device) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": " << device;
  callbacks_.on_pair_event(device, PairEvent{.is_paired = true});
}

void FastPairSeekerImpl::ForgetDeviceByAccountKey(
    const AccountKey& account_key) {
  NEARBY_LOGS(VERBOSE) << __func__;
  auto opt_device = devices_->FindDevice(account_key);
  if (!opt_device.has_value()) {
    NEARBY_LOGS(INFO) << __func__ << "No FP device matching the account key.";
  } else {
    devices_->RemoveDevice(opt_device.value());
  }

  repository_->DeleteAssociatedDeviceByAccountKey(
      account_key, [&](absl::Status success) {
        if (!success.ok()) return;
        NEARBY_LOGS(VERBOSE) << "Deleted associated devcie by account key";
        // Temporary solution to refresh the saved_devices_sheet.
        repository_->GetUserSavedDevices();
      });
}
}  // namespace fastpair
}  // namespace nearby
