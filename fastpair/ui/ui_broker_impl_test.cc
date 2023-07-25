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

#include "fastpair/ui/ui_broker_impl.h"

#include <memory>

#include "gtest/gtest.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fake_fast_pair_presenter.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "fastpair/ui/ui_broker.h"

namespace nearby {
namespace fastpair {
namespace {

constexpr absl::string_view kModelId = "718C17";
constexpr absl::string_view kAddress = "74:74:46:01:6C:21";

class UIBrokerImplTest : public ::testing::Test, public UIBroker::Observer {
 protected:
  void SetUp() override {
    presenter_factory_ = std::make_unique<FakeFastPairPresenterFactory>();
    FastPairPresenterImpl::Factory::SetFactoryForTesting(
        presenter_factory_.get());
    ui_broker_ = std::make_unique<UIBrokerImpl>();
    ui_broker_->AddObserver(this);
  }

  void OnDiscoveryAction(FastPairDevice& device,
                         DiscoveryAction action) override {
    on_discovery_action_notified_ = true;
    discovery_action_ = action;
  }

  std::unique_ptr<UIBroker> ui_broker_;
  FastPairNotificationController notification_controller_;
  std::unique_ptr<FakeFastPairPresenterFactory> presenter_factory_;
  DiscoveryAction discovery_action_;
  bool on_discovery_action_notified_ = false;
};

TEST_F(UIBrokerImplTest, ShowDiscovery) {
  FastPairDevice device(kModelId, kAddress, Protocol::kFastPairInitialPairing);
  ui_broker_->ShowDiscovery(device, notification_controller_);
  EXPECT_TRUE(presenter_factory_->fake_fast_pair_presenter()->show_discovery());
  EXPECT_TRUE(on_discovery_action_notified_);
  EXPECT_EQ(DiscoveryAction::kPairToDevice, discovery_action_);
}

TEST_F(UIBrokerImplTest, ShowDiscoveryWithoutObserver) {
  FastPairDevice device(kModelId, kAddress, Protocol::kFastPairInitialPairing);
  ui_broker_->RemoveObserver(this);
  ui_broker_->ShowDiscovery(device, notification_controller_);
  EXPECT_TRUE(presenter_factory_->fake_fast_pair_presenter()->show_discovery());
  EXPECT_FALSE(on_discovery_action_notified_);
}

TEST_F(UIBrokerImplTest, ShowPairingResult) {
  FastPairDevice device(kModelId, kAddress, Protocol::kFastPairInitialPairing);
  ui_broker_->ShowPairingResult(device, notification_controller_, true);
  EXPECT_TRUE(
      presenter_factory_->fake_fast_pair_presenter()->pairing_result_changed());
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
