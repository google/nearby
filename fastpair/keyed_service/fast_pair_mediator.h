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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_KEYED_SERVICE_FAST_PAIR_MEDIATOR_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_KEYED_SERVICE_FAST_PAIR_MEDIATOR_H_

#include <memory>
#include <optional>
#include <utility>

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/pairing/pairer_broker.h"
#include "fastpair/repository/fast_pair_device_repository.h"
#include "fastpair/scanning/scanner_broker.h"
#include "fastpair/server_access/fast_pair_client.h"
#include "fastpair/server_access/fast_pair_http_notifier.h"
#include "fastpair/repository/fast_pair_repository.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "fastpair/ui/ui_broker.h"
#include "internal/account/account_manager.h"
#include "internal/auth/authentication_manager.h"
#include "internal/network/http_client.h"
#include "internal/platform/device_info.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/task_runner.h"
#include "internal/preferences/preferences_manager.h"

namespace nearby {
namespace fastpair {

// Implements the Mediator design pattern for the components in the Fast Pair
class Mediator final : public ScannerBroker::Observer,
                       public UIBroker::Observer,
                       public PairerBroker::Observer {
 public:
  Mediator(
      std::unique_ptr<SingleThreadExecutor> executor,
      std::unique_ptr<Mediums> mediums, std::unique_ptr<UIBroker> ui_broker,
      std::unique_ptr<FastPairNotificationController> notification_controller,
      std::unique_ptr<auth::AuthenticationManager> authentication_manager,
      std::unique_ptr<network::HttpClient> http_client,
      std::unique_ptr<DeviceInfo> device_info);
  Mediator(const Mediator&) = delete;
  Mediator& operator=(const Mediator&) = delete;
  ~Mediator() override {
    if (scanning_session_ == nullptr) {
      NEARBY_LOGS(ERROR) << __func__ << "scanner is not running";
    }
    scanning_session_.reset();
    scanner_broker_->RemoveObserver(this);
    DestroyOnExecutor(std::move(scanner_broker_), executor_.get());
    scanner_broker_.reset();
    ui_broker_->RemoveObserver(this);
    ui_broker_.reset();
  };

  AccountManager* GetAccountManager() { return account_manager_.get(); }

  FastPairNotificationController* GetNotificationController() {
    return notification_controller_.get();
  }

  FastPairRepository* GetFastPairRepository() {
    return fast_pair_repository_.get();
  }

  void StartScanning();
  void StopScanning();

  // ScannerBroker::Observer
  void OnDeviceFound(FastPairDevice& device) override;
  void OnDeviceLost(FastPairDevice& device) override;

  // UIBroker::Observer
  void OnDiscoveryAction(FastPairDevice& device,
                         DiscoveryAction action) override;

  // PairBroker:Observer
  void OnDevicePaired(FastPairDevice& device) override;
  void OnAccountKeyWrite(FastPairDevice& device,
                         std::optional<PairFailure> error) override;
  void OnPairingComplete(FastPairDevice& device) override;
  void OnPairFailure(FastPairDevice& device, PairFailure failure) override;

  // Handle the state changes of screen lock.
  void SetIsScreenLocked(bool is_locked);

 private:
  bool IsFastPairEnabled();
  void InvalidateScanningState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  bool IsDeviceCurrentlyShowingNotification(const FastPairDevice& device);

  bool foreground_currently_showing_notification_ = false;
  FastPairHttpNotifier fast_pair_http_notifier_;
  std::unique_ptr<SingleThreadExecutor> executor_;
  std::unique_ptr<Mediums> mediums_;
  std::unique_ptr<ScannerBroker> scanner_broker_;
  std::unique_ptr<ScannerBroker::ScanningSession> scanning_session_;
  std::unique_ptr<UIBroker> ui_broker_;
  std::unique_ptr<PairerBroker> pairer_broker_;
  std::unique_ptr<FastPairNotificationController> notification_controller_;
  std::unique_ptr<FastPairRepository> fast_pair_repository_;
  std::unique_ptr<TaskRunner> task_runner_;
  std::unique_ptr<FastPairDeviceRepository> devices_;
  std::unique_ptr<AccountManager> account_manager_;
  std::unique_ptr<preferences::PreferencesManager> preferences_manager_;
  std::unique_ptr<auth::AuthenticationManager> authentication_manager_;
  std::unique_ptr<FastPairClient> fast_pair_client_;
  std::unique_ptr<network::HttpClient> http_client_;
  std::unique_ptr<DeviceInfo> device_info_;
  bool is_screen_locked_ = false;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_KEYED_SERVICE_FAST_PAIR_MEDIATOR_H_
