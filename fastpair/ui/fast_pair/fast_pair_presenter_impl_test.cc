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

#include <memory>
#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/server_access/fake_fast_pair_repository.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fake_fast_pair_notification_controller_observer.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"

namespace nearby {
namespace fastpair {

constexpr absl::string_view kModelId = "718C17";
constexpr absl::string_view kAddress = "74:74:46:01:6C:21";
constexpr absl::string_view kDeviceName = "Pixel Buds A-Series";

namespace {
class FastPairPresenterImplTest : public ::testing::Test {
 public:
  FastPairPresenterImplTest() {
    proto::Device device;
    repository_.SetFakeMetadata(kModelId, device);
    controller_.AddObserver(&notification_controller_observer_);
  }

  void OnDiscoveryAction(DiscoveryAction action) { discovery_action_ = action; }

 protected:
  DiscoveryAction discovery_action_;

  FakeFastPairRepository repository_;
  FastPairPresenterImpl fast_pair_presenter_;
  FastPairNotificationController controller_;
  FakeFastPairNotificationControllerObserver notification_controller_observer_;
};

TEST_F(FastPairPresenterImplTest, ShowDiscovery) {
  FastPairDevice device(kModelId.data(), kAddress.data(),
                        Protocol::kFastPairInitialPairing);

  EXPECT_EQ(0, notification_controller_observer_.on_update_device_count());
  fast_pair_presenter_.ShowDiscovery(
      device, controller_,
      [this](DiscoveryAction action) { OnDiscoveryAction(action); });
  EXPECT_EQ(1, notification_controller_observer_.on_update_device_count());
  controller_.OnDiscoveryClicked(DiscoveryAction::kPairToDevice);
  EXPECT_EQ(DiscoveryAction::kPairToDevice, discovery_action_);
}
}  // namespace
}  // namespace fastpair
}  // namespace nearby
