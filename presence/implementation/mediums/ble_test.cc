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

#include "presence/implementation/mediums/ble.h"

#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/uuid.h"
#include "presence/data_element.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

using FeatureFlags = location::nearby::FeatureFlags::Flags;
using BleOperationStatus = location::nearby::api::ble_v2::BleOperationStatus;
using BleV2MediumStatus =
    location::nearby::MediumEnvironment::BleV2MediumStatus;
using ScanningSession =
    location::nearby::api::ble_v2::BleMedium::ScanningSession;
using TxPowerLevel = location::nearby::api::ble_v2::TxPowerLevel;
using ScanningCallback =
    location::nearby::api::ble_v2::BleMedium::ScanningCallback;
using Uuid = location::nearby::Uuid;

constexpr FeatureFlags kTestCases[] = {
    FeatureFlags{},
};

class BleTest : public testing::TestWithParam<FeatureFlags> {
 public:
  class MockBleMedium {
   public:
    explicit MockBleMedium(location::nearby::BluetoothAdapter& adapter){}

    MOCK_METHOD((std::unique_ptr<ScanningSession>), StartScanning,
                (const Uuid& service_uuid, TxPowerLevel tx_power_level,
                 ScanningCallback callback));
  };
  constexpr static absl::Duration kWaitDuration = absl::Milliseconds(1000);

  std::string account_name_ = "Test-Name";
  constexpr static PowerMode kPowerMode = PowerMode::kBalanced;
  std::vector<nearby::internal::IdentityType> identity_types_ = {
      nearby::internal::IdentityType::IDENTITY_TYPE_TRUSTED,
  };
  std::vector<DataElement> extended_properties_ = {
      DataElement{DataElement::kTxPowerFieldType, "-10"}};
  std::vector<absl::variant<PresenceScanFilter, LegacyPresenceScanFilter> >
      filters_ = {PresenceScanFilter{
          .scan_type = ScanType::kPresenceScan,
          .extended_properties = extended_properties_,
      }};
  constexpr static bool kUseBle = true;
  constexpr static ScanType kScanType = ScanType::kPresenceScan;
  constexpr static bool kScanOnlyWhenScreenOn = true;

  ScanRequest scan_request_ = {
      .account_name = account_name_,
      .identity_types = identity_types_,
      .scan_filters = filters_,
      .use_ble = kUseBle,
      .scan_type = kScanType,
      .power_mode = kPowerMode,
      .scan_only_when_screen_on = kScanOnlyWhenScreenOn,
  };

 protected:
  BleTest() { env_.Stop(); }
  absl::optional<BleV2MediumStatus> GetBleStatus(
      const Ble<location::nearby::BleV2Medium>& ble) {
    return env_.GetBleV2MediumStatus(*ble.medium_->GetImpl());
  }
  MockBleMedium* GetMedium(const Ble<MockBleMedium>& ble) {
    return ble.medium_.get();
  }
  location::nearby::MediumEnvironment& env_{
      location::nearby::MediumEnvironment::Instance()};
};

INSTANTIATE_TEST_SUITE_P(ParametrisedBleTest, BleTest,
                         ::testing::ValuesIn(kTestCases));

// Using MediumEnvironment to verify the start&stop StartScanning callback flows
// are working as intended.
TEST_P(BleTest, CanStartThenStopScanning) {
  env_.Start();
  ::location::nearby::BluetoothAdapter adapter;
  Ble<location::nearby::BleV2Medium> ble(adapter);

  ScanRequest scan_request{
      .power_mode = PowerMode::kBalanced,
  };
  ScanningCallback scanning_callback;
  location::nearby::CountDownLatch started_scanning_latch(1);

  std::unique_ptr<ScanningSession> scannning_session = ble.StartScanning(
      scan_request,
      ScanningCallback{
          .start_scanning_result =
              [&started_scanning_latch](BleOperationStatus status) {
                if (status == BleOperationStatus::kSucceeded) {
                  started_scanning_latch.CountDown();
                }
              },
      });

  EXPECT_TRUE(started_scanning_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(GetBleStatus(ble).has_value() &&
              GetBleStatus(ble).value().is_scanning == true);
  BleOperationStatus stop_scanning_status = scannning_session->stop_scanning();
  EXPECT_EQ(BleOperationStatus::kSucceeded, stop_scanning_status);
  EXPECT_TRUE(GetBleStatus(ble).has_value() &&
              GetBleStatus(ble).value().is_scanning == false);
  env_.Stop();
}

// Using MockBleMedium to verify StartScanning is using the expected parameters
// with underneath BleMedium.
TEST_P(BleTest, VerifyStartScanning) {
  env_.Start();
  ::location::nearby::BluetoothAdapter adapter;
  Ble<MockBleMedium> ble(adapter);
  EXPECT_CALL(*GetMedium(ble), StartScanning(kPresenceServiceUuid,
                                             TxPowerLevel::kMedium, testing::_))
      .Times(1);

  std::unique_ptr<ScanningSession> scanning_session =
      ble.StartScanning(scan_request_, ScanningCallback{});
  env_.Stop();
}

}  // namespace presence
}  // namespace nearby
