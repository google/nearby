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

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fake_fast_pair_notification_controller_observer.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "internal/platform/count_down_latch.h"

namespace nearby {
namespace fastpair {
const int64_t kDeviceId = 10148625;
constexpr absl::string_view kModelId = "000000";
constexpr absl::string_view kAddress = "00:00:00:00:00:00";
constexpr absl::string_view kPublicKey = "test public key";
constexpr absl::string_view kInitialPairingdescription =
    "InitialPairingdescription";

namespace {
// A gMock matcher to match proto values. Use this matcher like:
// request/response proto, expected_proto;
// EXPECT_THAT(proto, MatchesProto(expected_proto));
MATCHER_P(
    MatchesProto, expected_proto,
    absl::StrCat(negation ? "does not match" : "matches",
                 testing::PrintToString(expected_proto.SerializeAsString()))) {
  return arg.SerializeAsString() == expected_proto.SerializeAsString();
}

TEST(FastPairPresenterImplTest, ShowDiscovery) {
  // Sets up proto::GetObservedDeviceResponse
  proto::GetObservedDeviceResponse response_proto;
  auto* device = response_proto.mutable_device();
  device->set_id(kDeviceId);
  auto* observed_device_strings = response_proto.mutable_strings();
  observed_device_strings->set_initial_pairing_description(
      kInitialPairingdescription);
  DeviceMetadata device_metadata(response_proto);
  FastPairDevice fast_pair_device(kModelId, kAddress,
                                  Protocol::kFastPairInitialPairing);

  fast_pair_device.SetMetadata(device_metadata);

  FastPairNotificationController notification_controller;
  CountDownLatch on_update_device_latch(1);
  CountDownLatch on_click_latch(1);
  FakeFastPairNotificationControllerObserver observer(&on_update_device_latch,
                                                      nullptr);
  notification_controller.AddObserver(&observer);
  DiscoveryAction discovery_action = DiscoveryAction::kUnknown;
  FastPairPresenterImpl fast_pair_presenter;
  fast_pair_presenter.ShowDiscovery(fast_pair_device, notification_controller,
                                    [&](DiscoveryAction action) {
                                      on_click_latch.CountDown();
                                      discovery_action = action;
                                    });
  on_update_device_latch.Await();
  EXPECT_EQ(observer.GetDevice()->GetMetadata()->GetFastPairVersion(),
            DeviceFastPairVersion::kV1);
  EXPECT_THAT(observer.GetDevice()->GetMetadata()->GetResponse(),
              MatchesProto(response_proto));
}

TEST(FastPairPresenterImplTest, ShowPairingResult) {
  // Sets up proto::GetObservedDeviceResponse
  proto::GetObservedDeviceResponse response_proto;
  auto* device = response_proto.mutable_device();
  device->set_id(kDeviceId);
  auto* observed_device_strings = response_proto.mutable_strings();
  observed_device_strings->set_initial_pairing_description(
      kInitialPairingdescription);
  DeviceMetadata device_metadata(response_proto);
  FastPairDevice fast_pair_device(kModelId, kAddress,
                                  Protocol::kFastPairInitialPairing);

  fast_pair_device.SetMetadata(device_metadata);

  FastPairNotificationController notification_controller;
  CountDownLatch on_pairing_result_latch(1);
  FakeFastPairNotificationControllerObserver observer(nullptr,
                                                      &on_pairing_result_latch);
  notification_controller.AddObserver(&observer);
  FastPairPresenterImpl fast_pair_presenter;
  fast_pair_presenter.ShowPairingResult(fast_pair_device,
                                        notification_controller, true);
  on_pairing_result_latch.Await();
  EXPECT_EQ(observer.GetDevice()->GetMetadata()->GetFastPairVersion(),
            DeviceFastPairVersion::kV1);
  EXPECT_THAT(observer.GetDevice()->GetMetadata()->GetResponse(),
              MatchesProto(response_proto));
  EXPECT_TRUE(observer.GetPairingResult().has_value());
  EXPECT_TRUE(observer.GetPairingResult().value());
}
}  // namespace
}  // namespace fastpair
}  // namespace nearby
