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

#include "fastpair/ui/fast_pair/fast_pair_presenter_impl.h"

#include <optional>

#include "gtest/gtest.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/server_access/fake_fast_pair_repository.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fake_fast_pair_notification_controller_observer.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "internal/platform/count_down_latch.h"

namespace nearby {
namespace fastpair {
constexpr absl::string_view kModelId = "000000";
constexpr absl::string_view kAddress = "00:00:00:00:00:00";
constexpr absl::string_view kPublicKey = "test public key";

namespace {
TEST(FastPairPresenterImplTest, ShowDiscoveryForV1Version) {
  // Setup repository with v1 version device metadata
  FakeFastPairRepository repository;
  proto::Device v1_version_device;
  repository.SetFakeMetadata(kModelId, v1_version_device);
  FastPairDevice device(kModelId, kAddress, Protocol::kFastPairInitialPairing);

  // Register FastPairNotificationControllerObserver
  auto latch_1 = std::make_optional<CountDownLatch>(1);
  FastPairNotificationController controller;
  FakeFastPairNotificationControllerObserver notification_controller_observer(
      latch_1);
  controller.AddObserver(&notification_controller_observer);
  EXPECT_EQ(notification_controller_observer.on_update_device_count(), 0);

  // FastPairPresenter ShowDiscovery
  CountDownLatch latch_2(1);
  FastPairPresenterImpl fast_pair_presenter;
  DiscoveryAction discovery_action = DiscoveryAction::kUnknown;
  fast_pair_presenter.ShowDiscovery(device, controller,
                                    [&](DiscoveryAction action) {
                                      discovery_action = action;
                                      latch_2.CountDown();
                                    });
  latch_1->Await();
  EXPECT_EQ(notification_controller_observer.on_update_device_count(), 1);
  EXPECT_EQ(device.GetVersion(), DeviceFastPairVersion::kV1);
  controller.OnDiscoveryClicked(DiscoveryAction::kPairToDevice);
  latch_2.Await();
  EXPECT_EQ(discovery_action, DiscoveryAction::kPairToDevice);
}

TEST(FastPairPresenterImplTest, ShowDiscoveryForHigherThanV1Version) {
  // Setup repository with HigherThanV1Version device metadata
  FakeFastPairRepository repository;
  proto::Device higher_than_v1_version_device;
  higher_than_v1_version_device.mutable_anti_spoofing_key_pair()
      ->set_public_key(kPublicKey);
  repository.SetFakeMetadata(kModelId, higher_than_v1_version_device);
  FastPairDevice device(kModelId, kAddress, Protocol::kFastPairInitialPairing);

  // Register FastPairNotificationControllerObserver
  auto latch_1 = std::make_optional<CountDownLatch>(1);
  FastPairNotificationController controller;
  FakeFastPairNotificationControllerObserver notification_controller_observer(
      latch_1);
  controller.AddObserver(&notification_controller_observer);
  EXPECT_EQ(notification_controller_observer.on_update_device_count(), 0);

  // FastPairPresenter ShowDiscovery
  CountDownLatch latch_2(1);
  FastPairPresenterImpl fast_pair_presenter;
  DiscoveryAction discovery_action = DiscoveryAction::kUnknown;
  fast_pair_presenter.ShowDiscovery(device, controller,
                                    [&](DiscoveryAction action) {
                                      discovery_action = action;
                                      latch_2.CountDown();
                                    });
  latch_1->Await();
  EXPECT_EQ(notification_controller_observer.on_update_device_count(), 1);
  EXPECT_EQ(device.GetVersion(), DeviceFastPairVersion::kHigherThanV1);
  controller.OnDiscoveryClicked(DiscoveryAction::kDismissedByUser);
  latch_2.Await();
  EXPECT_EQ(discovery_action, DiscoveryAction::kDismissedByUser);
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
