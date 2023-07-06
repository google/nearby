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

#include <unistd.h>

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fastpair/fast_pair_events.h"
#include "fastpair/message_stream/fake_gatt_callbacks.h"
#include "fastpair/message_stream/fake_provider.h"
#include "fastpair/repository/fake_fast_pair_repository.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr absl::string_view kServiceID{"Fast Pair"};
constexpr absl::string_view kFastPairServiceUuid{
    "0000FE2C-0000-1000-8000-00805F9B34FB"};
constexpr absl::string_view kModelId{"718c17"};
constexpr absl::string_view kBobPrivateKey =
    "02B437B0EDD6BBD429064A4E529FCBF1C48D0D624924D592274B7ED81193D763";
constexpr absl::string_view kBobPublicKey =
    "F7D496A62ECA416351540AA343BC690A6109F551500666B83B1251FB84FA2860795EBD63D3"
    "B8836F44A9A3E28BB34017E015F5979305D849FDF8DE10123B61D2";
constexpr absl::string_view kPasskey = "123456";

constexpr absl::Duration kTaskWaitTimeout = absl::Milliseconds(100);

using ::testing::status::StatusIs;

class MediumEnvironmentStarter {
 public:
  MediumEnvironmentStarter() { MediumEnvironment::Instance().Start(); }
  ~MediumEnvironmentStarter() { MediumEnvironment::Instance().Stop(); }
};

class FastPairSeekerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    repository_ = FakeFastPairRepository::Create(
        kModelId, absl::HexStringToBytes(kBobPublicKey));
  }

  void TearDown() override { executor_.Shutdown(); }

  MediumEnvironmentStarter env_;
  SingleThreadExecutor executor_;
  FastPairDeviceRepository devices_{&executor_};
  std::unique_ptr<FakeFastPairRepository> repository_;
  std::unique_ptr<FastPairSeekerImpl> fast_pair_seeker_;
  FakeGattCallbacks fake_gatt_callbacks_;
};

TEST_F(FastPairSeekerImplTest, StartAndStopFastPairScan) {
  fast_pair_seeker_ = std::make_unique<FastPairSeekerImpl>(
      FastPairSeekerImpl::ServiceCallbacks{}, &executor_, &devices_);

  EXPECT_OK(fast_pair_seeker_->StartFastPairScan());
  EXPECT_OK(fast_pair_seeker_->StopFastPairScan());
}

TEST_F(FastPairSeekerImplTest, DiscoverDevice) {
  FakeProvider provider;
  CountDownLatch latch(1);
  fast_pair_seeker_ = std::make_unique<FastPairSeekerImpl>(
      FastPairSeekerImpl::ServiceCallbacks{
          .on_initial_discovery =
              [&](const FastPairDevice& device, InitialDiscoveryEvent event) {
                EXPECT_EQ(device.GetModelId(), kModelId);
                latch.CountDown();
              }},
      &executor_, &devices_);

  EXPECT_OK(fast_pair_seeker_->StartFastPairScan());
  provider.StartDiscoverableAdvertisement(kModelId);

  // Waits for the device to be discovered
  latch.Await();
}

TEST_F(FastPairSeekerImplTest, StartFastPairScanTwiceFails) {
  fast_pair_seeker_ = std::make_unique<FastPairSeekerImpl>(
      FastPairSeekerImpl::ServiceCallbacks{}, &executor_, &devices_);
  EXPECT_OK(fast_pair_seeker_->StartFastPairScan());

  EXPECT_THAT(fast_pair_seeker_->StartFastPairScan(),
              StatusIs(absl::StatusCode::kAlreadyExists));
}

TEST_F(FastPairSeekerImplTest, StopFastPairScanTwiceFails) {
  fast_pair_seeker_ = std::make_unique<FastPairSeekerImpl>(
      FastPairSeekerImpl::ServiceCallbacks{}, &executor_, &devices_);
  EXPECT_OK(fast_pair_seeker_->StartFastPairScan());

  EXPECT_OK(fast_pair_seeker_->StopFastPairScan());
  EXPECT_THAT(fast_pair_seeker_->StopFastPairScan(),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(FastPairSeekerImplTest, ScreenLocksDuringAdvertising) {
  CountDownLatch latch(2);
  fast_pair_seeker_ = std::make_unique<FastPairSeekerImpl>(
      FastPairSeekerImpl::ServiceCallbacks{
          .on_initial_discovery =
              [&](const FastPairDevice& device, InitialDiscoveryEvent event) {
                latch.CountDown();
              },
          .on_screen_event =
              [&](ScreenEvent event) {
                EXPECT_TRUE(event.is_locked);
                latch.CountDown();
              }},
      &executor_, &devices_);
  // Create Advertiser and startAdvertising
  Mediums mediums_2;
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{absl::HexStringToBytes(kModelId)};
  std::string fast_pair_service_uuid(kFastPairServiceUuid);
  mediums_2.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes, fast_pair_service_uuid);
  EXPECT_OK(fast_pair_seeker_->StartFastPairScan());

  fast_pair_seeker_->SetIsScreenLocked(true);

  EXPECT_TRUE(latch.Await().Ok());
}

TEST_F(FastPairSeekerImplTest, InitialPairing) {
  NEARBY_LOG_SET_SEVERITY(VERBOSE);
  FakeProvider provider;
  CountDownLatch discover_latch(1);
  CountDownLatch pair_latch(1);
  fast_pair_seeker_ = std::make_unique<FastPairSeekerImpl>(
      FastPairSeekerImpl::ServiceCallbacks{
          .on_initial_discovery =
              [&](const FastPairDevice& device, InitialDiscoveryEvent event) {
                EXPECT_EQ(device.GetModelId(), kModelId);
                EXPECT_OK(fast_pair_seeker_->StartInitialPairing(
                    device, {},
                    {.on_pairing_result = [&](const FastPairDevice& device,
                                              absl::Status status) {
                      EXPECT_EQ(device.GetBleAddress(),
                                provider.GetMacAddress());
                      EXPECT_OK(status);
                      pair_latch.CountDown();
                    }}));
                discover_latch.CountDown();
              }},
      &executor_, &devices_);

  EXPECT_OK(fast_pair_seeker_->StartFastPairScan());
  provider.PrepareForInitialPairing(
      {
          .private_key = absl::HexStringToBytes(kBobPrivateKey),
          .public_key = absl::HexStringToBytes(kBobPublicKey),
          .model_id = std::string(kModelId),
          .pass_key = std::string(kPasskey),
      },
      &fake_gatt_callbacks_);

  discover_latch.Await();
  pair_latch.Await();
  auto fp_device = devices_.FindDevice(provider.GetMacAddress());
  ASSERT_TRUE(fp_device.has_value());
  EXPECT_EQ(provider.GetAccountKey(), fp_device.value()->GetAccountKey());
}

TEST_F(FastPairSeekerImplTest, RetroactivePairing) {
  NEARBY_LOG_SET_SEVERITY(VERBOSE);
  FakeProvider provider;
  CountDownLatch pair_latch(1);
  CountDownLatch retro_latch(1);
  fast_pair_seeker_ = std::make_unique<FastPairSeekerImpl>(
      FastPairSeekerImpl::ServiceCallbacks{
          .on_pair_event =
              [&](const FastPairDevice& device, PairEvent event) {
                NEARBY_LOGS(INFO) << "Pair callback";
                pair_latch.CountDown();
                EXPECT_OK(fast_pair_seeker_->StartRetroactivePairing(
                    device, RetroactivePairingParam{},
                    {.on_pairing_result = [&](const FastPairDevice&,
                                              absl::Status status) {
                      EXPECT_OK(status);
                      retro_latch.CountDown();
                    }}));
              }},
      &executor_, &devices_);

  provider.PrepareForRetroactivePairing(
      {.private_key = absl::HexStringToBytes(kBobPrivateKey),
       .public_key = absl::HexStringToBytes(kBobPublicKey),
       .model_id = std::string(kModelId)},
      &fake_gatt_callbacks_);

  EXPECT_TRUE(pair_latch.Await().Ok());
  EXPECT_TRUE(retro_latch.Await().Ok());
  auto fp_device = devices_.FindDevice(provider.GetMacAddress());
  ASSERT_TRUE(fp_device.has_value());
  EXPECT_EQ(provider.GetAccountKey(), fp_device.value()->GetAccountKey());
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
