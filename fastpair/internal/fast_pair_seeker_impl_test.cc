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
#include "fastpair/message_stream/fake_provider.h"
#include "fastpair/server_access/fake_fast_pair_repository.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr absl::string_view kServiceID{"Fast Pair"};
constexpr absl::string_view kFastPairServiceUuid{
    "0000FE2C-0000-1000-8000-00805F9B34FB"};
constexpr absl::string_view kModelId{"718c17"};
constexpr absl::string_view kPublicAntiSpoof =
    "Wuyr48lD3txnUhGiMF1IfzlTwRxxe+wMB1HLzP+"
    "0wVcljfT3XPoiy1fntlneziyLD5knDVAJSE+RM/zlPRP/Jg==";
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
    repository_ = FakeFastPairRepository::Create(kModelId, kPublicAntiSpoof);
  }

  void TearDown() override { executor_.Shutdown(); }

  MediumEnvironmentStarter env_;
  SingleThreadExecutor executor_;
  FastPairDeviceRepository devices_{&executor_};
  std::unique_ptr<FakeFastPairRepository> repository_;
  std::unique_ptr<FastPairSeekerImpl> fast_pair_seeker_;
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
  CountDownLatch latch(1);
  fast_pair_seeker_ = std::make_unique<FastPairSeekerImpl>(
      FastPairSeekerImpl::ServiceCallbacks{
          .on_initial_discovery =
              [&](const FastPairDevice& device, InitialDiscoveryEvent event) {
                latch.CountDown();
              }},
      &executor_, &devices_);

  EXPECT_OK(fast_pair_seeker_->StartFastPairScan());
  EXPECT_THAT(fast_pair_seeker_->StartFastPairScan(),
              StatusIs(absl::StatusCode::kAlreadyExists));

  fast_pair_seeker_->SetIsScreenLocked(true);
  // Create Advertiser and startAdvertising
  Mediums mediums_2;
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{absl::HexStringToBytes(kModelId)};
  std::string fast_pair_service_uuid(kFastPairServiceUuid);
  mediums_2.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes, fast_pair_service_uuid);

  EXPECT_FALSE(latch.Await(kTaskWaitTimeout).result());
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
