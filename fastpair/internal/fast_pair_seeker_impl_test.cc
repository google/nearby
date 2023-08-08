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
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/fast_pair_prefs.h"
#include "fastpair/fast_pair_events.h"
#include "fastpair/fast_pair_seeker.h"
#include "fastpair/message_stream/fake_gatt_callbacks.h"
#include "fastpair/message_stream/fake_provider.h"
#include "fastpair/proto/data.proto.h"
#include "fastpair/proto/enum.proto.h"
#include "fastpair/repository/fake_fast_pair_repository.h"
#include "fastpair/repository/fast_pair_device_repository.h"
#include "fastpair/repository/fast_pair_repository.h"
#include "internal/account/fake_account_manager.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/task_runner_impl.h"
#include "internal/test/google3_only/fake_authentication_manager.h"

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
constexpr absl::string_view kFastPairPreferencesFilePath =
    "Google/Nearby/FastPair";
constexpr absl::string_view kTestAccountId = "test_account_id";

using ::testing::status::StatusIs;

class MediumEnvironmentStarter {
 public:
  MediumEnvironmentStarter() { MediumEnvironment::Instance().Start(); }
  ~MediumEnvironmentStarter() { MediumEnvironment::Instance().Stop(); }
};

class FastPairRepositoryObserver : public FastPairRepository::Observer {
 public:
  explicit FastPairRepositoryObserver(CountDownLatch* latch) { latch_ = latch; }

  void OnGetUserSavedDevices(
      const proto::OptInStatus& opt_in_status,
      const std::vector<proto::FastPairDevice>& devices) override {
    latch_->CountDown();
  }

  CountDownLatch* latch_ = nullptr;
};

class FastPairSeekerImplTest : public testing::Test {
 protected:
  FastPairSeekerImplTest() {
    task_runner_ = std::make_unique<TaskRunnerImpl>(1);
    preferences_manager_ = std::make_unique<preferences::PreferencesManager>(
        kFastPairPreferencesFilePath);
    authentication_manager_ = std::make_unique<FakeAuthenticationManager>();
  }

  void SetUp() override {
    NEARBY_LOG_SET_SEVERITY(VERBOSE);
    repository_ = FakeFastPairRepository::Create(
        kModelId, absl::HexStringToBytes(kBobPublicKey));
    repository_->SetResultOfIsDeviceSavedToAccount(
        absl::NotFoundError("not found"));
    account_manager_ = std::make_unique<FakeAccountManager>(
        preferences_manager_.get(), prefs::kNearbyFastPairUsersName,
        authentication_manager_.get(), task_runner_.get());
    AccountManager::Account account;
    account.id = kTestAccountId;
    account_manager_->SetAccount(account);
  }

  void TearDown() override { executor_.Shutdown(); }

  void WaitForBackgroundTasks() {
    CountDownLatch latch(1);
    executor_.Execute([&]() { latch.CountDown(); });
    latch.Await();
  }

  MediumEnvironmentStarter env_;
  SingleThreadExecutor executor_;
  std::unique_ptr<preferences::PreferencesManager> preferences_manager_;
  std::unique_ptr<auth::AuthenticationManager> authentication_manager_;
  std::unique_ptr<TaskRunner> task_runner_;
  std::unique_ptr<FakeAccountManager> account_manager_;
  FastPairDeviceRepository devices_{&executor_};
  std::unique_ptr<FakeFastPairRepository> repository_;
  std::unique_ptr<FastPairSeekerImpl> fast_pair_seeker_;
  FakeGattCallbacks fake_gatt_callbacks_;
};

TEST_F(FastPairSeekerImplTest, StartAndStopFastPairScan) {
  fast_pair_seeker_ = std::make_unique<FastPairSeekerImpl>(
      FastPairSeekerImpl::ServiceCallbacks{}, &executor_,
      account_manager_.get(), &devices_, repository_.get());

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
      &executor_, account_manager_.get(), &devices_, repository_.get());

  EXPECT_OK(fast_pair_seeker_->StartFastPairScan());
  provider.StartDiscoverableAdvertisement(kModelId);

  // Waits for the device to be discovered
  latch.Await();
}

TEST_F(FastPairSeekerImplTest, StartFastPairScanTwiceFails) {
  fast_pair_seeker_ = std::make_unique<FastPairSeekerImpl>(
      FastPairSeekerImpl::ServiceCallbacks{}, &executor_,
      account_manager_.get(), &devices_, repository_.get());
  EXPECT_OK(fast_pair_seeker_->StartFastPairScan());

  EXPECT_THAT(fast_pair_seeker_->StartFastPairScan(),
              StatusIs(absl::StatusCode::kAlreadyExists));
}

TEST_F(FastPairSeekerImplTest, StopFastPairScanTwiceFails) {
  fast_pair_seeker_ = std::make_unique<FastPairSeekerImpl>(
      FastPairSeekerImpl::ServiceCallbacks{}, &executor_,
      account_manager_.get(), &devices_, repository_.get());
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
      &executor_, account_manager_.get(), &devices_, repository_.get());
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
      &executor_, account_manager_.get(), &devices_, repository_.get());

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
  fast_pair_seeker_.reset();
}

TEST_F(FastPairSeekerImplTest, ForgetDeviceByAccountKey) {
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
      &executor_, account_manager_.get(), &devices_, repository_.get());

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

  // Adds FastPairRepository observer.
  CountDownLatch repository_latch(1);
  FastPairRepositoryObserver observer(&repository_latch);
  repository_->AddObserver(&observer);
  // Adds FastPairDeviceRepository observer.
  CountDownLatch devices_latch(1);
  FastPairDeviceRepository::RemoveDeviceCallback callback =
      [&](const FastPairDevice& device) { devices_latch.CountDown(); };
  devices_.AddObserver(&callback);

  fast_pair_seeker_->ForgetDeviceByAccountKey(
      fp_device.value()->GetAccountKey());
  repository_latch.Await();
  devices_latch.Await();
  EXPECT_FALSE(devices_.FindDevice(provider.GetMacAddress()).has_value());
  fast_pair_seeker_.reset();
}

TEST_F(FastPairSeekerImplTest, RetroactivePairingWithUserConsent) {
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
      &executor_, account_manager_.get(), &devices_, repository_.get());

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

  // The client asks the user for consent, the user grants it.
  CountDownLatch finish_latch(1);
  EXPECT_OK(fast_pair_seeker_->FinishRetroactivePairing(
      **fp_device, FinishRetroactivePairingParam{.save_account_key = true},
      {.on_pairing_result = [&](const FastPairDevice&, absl::Status status) {
        EXPECT_OK(status);
        finish_latch.CountDown();
      }}));
  EXPECT_TRUE(finish_latch.Await());
}

TEST_F(FastPairSeekerImplTest, RetroactivePairingNoUserConsent) {
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
      &executor_, account_manager_.get(), &devices_, repository_.get());

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

  // The client asks the user for consent, the user rejects it.
  CountDownLatch finish_latch(1);
  EXPECT_OK(fast_pair_seeker_->FinishRetroactivePairing(
      **fp_device, FinishRetroactivePairingParam{.save_account_key = false},
      {.on_pairing_result = [&](const FastPairDevice&, absl::Status status) {
        EXPECT_OK(status);
        finish_latch.CountDown();
      }}));
  EXPECT_TRUE(finish_latch.Await());
  // The device should be deleted.
  WaitForBackgroundTasks();
  EXPECT_FALSE(devices_.FindDevice(provider.GetMacAddress()).has_value());
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
