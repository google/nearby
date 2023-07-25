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

#include "fastpair/keyed_service/fast_pair_mediator.h"

#include <ios>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/fast_pair_prefs.h"
#include "fastpair/common/protocol.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/pairing/pairer_broker_impl.h"
#include "fastpair/repository/fast_pair_device_repository.h"
#include "fastpair/repository/fast_pair_repository_impl.h"
#include "fastpair/scanning/scanner_broker_impl.h"
#include "fastpair/server_access/fast_pair_client_impl.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "fastpair/ui/ui_broker_impl.h"
#include "internal/account/account_manager_impl.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/logging.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/task_runner_impl.h"
#include "internal/preferences/preferences_manager.h"

namespace nearby {
namespace fastpair {
namespace {
constexpr char kFastPairPreferencesFilePath[] = "Google/Nearby/FastPair";
constexpr FeatureFlags::Flags fast_pair_feature_flags = FeatureFlags::Flags{
    .enable_scan_for_fast_pair_advertisement = true,
};
}

Mediator::Mediator(
    std::unique_ptr<SingleThreadExecutor> executor,
    std::unique_ptr<Mediums> mediums, std::unique_ptr<UIBroker> ui_broker,
    std::unique_ptr<FastPairNotificationController> notification_controller,
    std::unique_ptr<auth::AuthenticationManager> authentication_manager,
    std::unique_ptr<network::HttpClient> http_client,
    std::unique_ptr<DeviceInfo> device_info)
    : executor_(std::move(executor)),
      mediums_(std::move(mediums)),
      ui_broker_(std::move(ui_broker)),
      notification_controller_(std::move(notification_controller)),
      authentication_manager_(std::move(authentication_manager)),
      http_client_(std::move(http_client)),
      device_info_(std::move(device_info)) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      platform::config_package_nearby::nearby_platform_feature::
          kEnableBleV2Gatt,
      true);
  const_cast<FeatureFlags&>(FeatureFlags::GetInstance())
      .SetFlags(fast_pair_feature_flags);

  devices_ = std::make_unique<FastPairDeviceRepository>(executor_.get());
  scanner_broker_ = std::make_unique<ScannerBrokerImpl>(
      *mediums_, executor_.get(), devices_.get());
  pairer_broker_ =
      std::make_unique<PairerBrokerImpl>(*mediums_, executor_.get());
  task_runner_ = std::make_unique<TaskRunnerImpl>(1);
  preferences_manager_ = std::make_unique<preferences::PreferencesManager>(
      kFastPairPreferencesFilePath);
  account_manager_ = AccountManagerImpl::Factory::Create(
      preferences_manager_.get(), prefs::kNearbyFastPairUsersName,
      authentication_manager_.get(), task_runner_.get());
  fast_pair_client_ = std::make_unique<FastPairClientImpl>(
      authentication_manager_.get(), account_manager_.get(), http_client_.get(),
      &fast_pair_http_notifier_, device_info_.get());
  fast_pair_repository_ =
      std::make_unique<FastPairRepositoryImpl>(fast_pair_client_.get());
  scanner_broker_->AddObserver(this);
  ui_broker_->AddObserver(this);
  pairer_broker_->AddObserver(this);
}

void Mediator::OnDeviceFound(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": " << device;
  if (device.ShouldShowUiNotification().value_or(false)) {
    NEARBY_LOGS(INFO) << __func__ << ": Ignoring because show UI flag is false";
    return;
  }
  if (foreground_currently_showing_notification_) {
    NEARBY_LOGS(VERBOSE)
        << __func__
        << ": Already showing a notification for a different device= ";
    return;
  }
  // Show discovery notification
  foreground_currently_showing_notification_ = true;
  ui_broker_->ShowDiscovery(device, *notification_controller_);
}

void Mediator::OnDeviceLost(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": " << device;
}

void Mediator::OnDiscoveryAction(FastPairDevice& device,
                                 DiscoveryAction action) {
  switch (action) {
    case DiscoveryAction::kPairToDevice:
      NEARBY_LOGS(INFO) << __func__ << ": Action =  kPairToDevice";
      pairer_broker_->PairDevice(device);
      break;
    case DiscoveryAction::kDismissedByOs:
      NEARBY_LOGS(INFO) << __func__ << ": Action =  kDismissedByOs";
      break;
    case DiscoveryAction::kDismissedByUser:
      // When the user explicitly dismisses the discovery notification, update
      // the device's block-list value accordingly.
      NEARBY_LOGS(INFO) << __func__ << ": Action =  kDismissedByUser";
      foreground_currently_showing_notification_ = false;
      // TODO(b/285453663): update discovery block list
      [[fallthrough]];
    case DiscoveryAction::kDismissedByTimeout:
      NEARBY_LOGS(INFO) << __func__ << ": Action =  kDismissedByTimeout";
      foreground_currently_showing_notification_ = false;
      break;
    case DiscoveryAction::kLearnMore:
      NEARBY_LOGS(INFO) << __func__ << ": Action =  kLearnMore";
      break;
    case DiscoveryAction::kDone:
      NEARBY_LOGS(INFO) << __func__ << ": Action =  kDone";
      foreground_currently_showing_notification_ = false;
      break;
    default:
      NEARBY_LOGS(INFO) << __func__ << ": Action =  Unknown";
      break;
  }
}

void Mediator::OnDevicePaired(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": " << device;
  ui_broker_->ShowPairingResult(device, *notification_controller_, true);
}

void Mediator::OnAccountKeyWrite(FastPairDevice& device,
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

void Mediator::OnPairingComplete(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": " << device;
}

void Mediator::OnPairFailure(FastPairDevice& device, PairFailure failure) {
  NEARBY_LOGS(INFO) << __func__ << ": " << device
                    << " with PairFailure: " << failure;
  ui_broker_->ShowPairingResult(device, *notification_controller_, false);
}

void Mediator::StartScanning() {
  NEARBY_LOGS(VERBOSE) << __func__;
  if (IsFastPairEnabled()) {
    if (scanning_session_ != nullptr) {
      return;
    }
    scanner_broker_ = std::make_unique<ScannerBrokerImpl>(
        *mediums_, executor_.get(), devices_.get());
    scanner_broker_->AddObserver(this);
    scanning_session_ =
        scanner_broker_->StartScanning(Protocol::kFastPairInitialPairing);
    return;
  }
  scanning_session_.reset();
}

void Mediator::StopScanning() {
  NEARBY_LOGS(VERBOSE) << __func__;
  if (scanning_session_ == nullptr) {
    return;
  }
  scanning_session_.reset();
  scanner_broker_->RemoveObserver(this);
  DestroyOnExecutor(std::move(scanner_broker_), executor_.get());
}

bool Mediator::IsFastPairEnabled() {
  // TODO(b/275452353): Add feature_status_tracker IsFastPairEnabled()
  // Currently default to true.
  NEARBY_LOGS(VERBOSE) << __func__ << ": " << true;
  return true;
}

void Mediator::SetIsScreenLocked(bool locked) {
  executor_->Execute(
      "on_lock_state_changed",
      [this, locked]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
        NEARBY_LOGS(INFO) << __func__ << ": Screen lock state changed. ("
                          << std::boolalpha << locked << ")";
        is_screen_locked_ = locked;
        InvalidateScanningState();
      });
}

void Mediator::InvalidateScanningState() {
  NEARBY_LOGS(INFO) << __func__;
  // Stop scanning when screen is off.
  if (is_screen_locked_) {
    StopScanning();
    NEARBY_LOGS(VERBOSE) << __func__
                         << ": Stopping scanning because the screen is locked.";
    return;
  }

  // TODO(b/275452353): Check if bluetooth and fast pair is enabled

  // Screen is on, Bluetooth is enabled, and Fast Pair is enabled, start
  // scanning.
  StartScanning();
}

}  // namespace fastpair
}  // namespace nearby
