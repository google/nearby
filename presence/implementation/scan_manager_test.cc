// Copyright 2022 Google LLC
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

#include "presence/implementation/scan_manager.h"

#include <math.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/random/random.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"
#include "presence/implementation/advertisement_factory.h"
#include "presence/implementation/base_broadcast_request.h"
#include "presence/implementation/credential_manager_impl.h"
#include "presence/implementation/mediums/ble.h"
#include "presence/implementation/mediums/mediums.h"

namespace nearby {
namespace presence {
namespace {

using BleOperationStatus = location::nearby::api::ble_v2::BleOperationStatus;

class ScanManagerTest : public testing::Test {
 protected:
  void SetUp() override { env_.Start(); }
  void TearDown() override { env_.Stop(); }
  std::vector<nearby::internal::IdentityType> identity_types_ = {
      nearby::internal::IdentityType::IDENTITY_TYPE_PUBLIC,
  };
  std::vector<DataElement> extended_properties_ = {
      DataElement(ActionBit::kPresenceManagerAction)};
  std::vector<absl::variant<PresenceScanFilter, LegacyPresenceScanFilter>>
      filters_ = {PresenceScanFilter{
          .scan_type = ScanType::kPresenceScan,
          .extended_properties = extended_properties_,
      }};
  ScanRequest scan_request_ = {
      .account_name = "Test account",
      .identity_types = identity_types_,
      .scan_filters = filters_,
      .use_ble = true,
      .scan_type = ScanType::kPresenceScan,
      .power_mode = PowerMode::kBalanced,
      .scan_only_when_screen_on = true,
  };
  CredentialManagerImpl credential_manager_;
  location::nearby::MediumEnvironment& env_ = {
      location::nearby::MediumEnvironment::Instance()};
  location::nearby::api::ble_v2::AdvertiseParameters params_ = {
      .tx_power_level = Ble::TxPowerLevel::kHigh,
      .is_connectable = true,
  };
  location::nearby::CountDownLatch start_latch_{1};
  location::nearby::CountDownLatch found_latch_{1};
};

TEST_F(ScanManagerTest, CanStartThenStopScanning) {
  Mediums mediums;
  ScanManager manager(mediums, credential_manager_);
  ScanCallback scanning_callback = {
      .start_scan_cb =
          [this](Status status) {
            if (status.Ok()) {
              start_latch_.CountDown();
            }
          },
      .on_discovered_cb =
          [this](PresenceDevice pd) { found_latch_.CountDown(); }};
  // Set up advertiser
  // Create BroadcastRequest
  PresenceBroadcast::BroadcastSection section = {
      .identity = internal::IDENTITY_TYPE_PUBLIC,
      .extended_properties = extended_properties_,
      .account_name = "Test account"};
  PresenceBroadcast presence_request = {.sections = {section}};
  BroadcastRequest input = {.tx_power = 30, .variant = presence_request};

  absl::StatusOr<BaseBroadcastRequest> request =
      BaseBroadcastRequest::Create(input);
  AdvertisementFactory factory(&credential_manager_);
  absl::StatusOr<BleAdvertisementData> advertisement =
      factory.CreateAdvertisement(request.ValueOrDie());
  location::nearby::BluetoothAdapter adapter2;
  location::nearby::BleV2Medium ble2(adapter2);
  ASSERT_TRUE(ble2.StartAdvertising(advertisement.ValueOrDie(), params_));

  // Start scanning
  ScanSession scan_session =
      manager.StartScan(scan_request_, std::move(scanning_callback));
  // Ensure that we are in a good state.
  env_.Sync();
  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 1);
  ASSERT_TRUE(ble2.IsValid());
  ASSERT_TRUE(mediums.GetBle().IsAvailable());
  EXPECT_TRUE(start_latch_.Await(absl::Milliseconds(500)).result());
  EXPECT_TRUE(found_latch_.Await(absl::Milliseconds(1500)).result());
  EXPECT_TRUE(scan_session.StopScan().Ok());
  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 0);
}

TEST_F(ScanManagerTest, CannotStopScanTwice) {
  Mediums mediums;
  ScanManager manager(mediums, credential_manager_);
  ScanCallback scanning_callback = ScanCallback{
      .start_scan_cb =
          [this](Status status) {
            if (status.Ok()) {
              start_latch_.CountDown();
            }
          },
  };

  auto scan_session =
      manager.StartScan(scan_request_, std::move(scanning_callback));
  NEARBY_LOGS(INFO) << "Start scan";
  EXPECT_TRUE(start_latch_.Await(absl::Milliseconds(1000)).result());
  // Ensure that we have started scanning before we try to stop.
  env_.Sync();
  NEARBY_LOGS(INFO) << "Stop scan";
  EXPECT_TRUE(scan_session.StopScan().Ok());
  NEARBY_LOGS(INFO) << "Stop scan again";
  EXPECT_FALSE(scan_session.StopScan().Ok());
}

TEST_F(ScanManagerTest, TestNoFilter) {
  Mediums mediums;
  ScanManager manager(mediums, credential_manager_);
  ScanCallback scanning_callback = {
      .start_scan_cb =
          [this](Status status) {
            if (status.Ok()) {
              start_latch_.CountDown();
            }
          },
      .on_discovered_cb =
          [this](PresenceDevice pd) { found_latch_.CountDown(); }};
  // Set up advertiser
  // Create BroadcastRequest
  PresenceBroadcast::BroadcastSection section = {
      .identity = internal::IDENTITY_TYPE_PUBLIC,
      .extended_properties = {},
      .account_name = "Test account"};
  PresenceBroadcast presence_request = {.sections = {section}};
  BroadcastRequest input = {.tx_power = 30, .variant = presence_request};

  absl::StatusOr<BaseBroadcastRequest> request =
      BaseBroadcastRequest::Create(input);
  AdvertisementFactory factory(&credential_manager_);
  absl::StatusOr<BleAdvertisementData> advertisement =
      factory.CreateAdvertisement(request.ValueOrDie());
  location::nearby::BluetoothAdapter adapter2;
  location::nearby::BleV2Medium ble2(adapter2);
  ASSERT_TRUE(ble2.StartAdvertising(advertisement.ValueOrDie(), params_));

  // Start scanning
  ScanRequest scan_request = {
      .account_name = "Test account",
      .identity_types = identity_types_,
      .scan_filters = {},
      .use_ble = true,
      .scan_type = ScanType::kPresenceScan,
      .power_mode = PowerMode::kBalanced,
      .scan_only_when_screen_on = true,
  };
  ScanSession scan_session =
      manager.StartScan(scan_request, std::move(scanning_callback));

  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 1);
  ASSERT_TRUE(ble2.IsValid());
  ASSERT_TRUE(mediums.GetBle().IsAvailable());
  EXPECT_TRUE(start_latch_.Await(absl::Milliseconds(500)).result());
  EXPECT_TRUE(found_latch_.Await(absl::Milliseconds(1500)).result());
  EXPECT_TRUE(scan_session.StopScan().Ok());
  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 0);
}
}  // namespace
}  // namespace presence
}  // namespace nearby
