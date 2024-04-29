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
#include "presence/implementation/mediums/advertisement_data.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {
namespace {

using FeatureFlags = ::nearby::FeatureFlags::Flags;
using BleV2MediumStatus = ::nearby::MediumEnvironment::BleV2MediumStatus;
using ScanningSession = ::nearby::api::ble_v2::BleMedium::ScanningSession;
using TxPowerLevel = ::nearby::api::ble_v2::TxPowerLevel;
using ScanningCallback = ::nearby::api::ble_v2::BleMedium::ScanningCallback;
using Uuid = ::nearby::Uuid;
using ::nearby::api::ble_v2::BleAdvertisementData;
using ::nearby::api::ble_v2::BlePeripheral;
using AdvertisingCallback =
    ::nearby::api::ble_v2::BleMedium::AdvertisingCallback;
using AdvertisingSession = ::nearby::api::ble_v2::BleMedium::AdvertisingSession;

constexpr FeatureFlags kTestCases[] = {
    FeatureFlags{},
};

class BleTest : public testing::TestWithParam<FeatureFlags> {
 public:
  constexpr static absl::Duration kWaitDuration = absl::Milliseconds(1000);

  std::string account_name_ = "Test-Name";
  constexpr static PowerMode kPowerMode = PowerMode::kBalanced;
  std::vector<nearby::internal::IdentityType> identity_types_ = {
      nearby::internal::IdentityType::IDENTITY_TYPE_CONTACTS_GROUP,
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
  absl::optional<BleV2MediumStatus> GetBleStatus(const Ble& ble) {
    return env_.GetBleV2MediumStatus(*ble.GetImpl());
  }
  nearby::MediumEnvironment& env_{nearby::MediumEnvironment::Instance()};
};

INSTANTIATE_TEST_SUITE_P(ParametrisedBleTest, BleTest,
                         ::testing::ValuesIn(kTestCases));

// Using MediumEnvironment to verify the start&stop StartScanning callback flows
// are working as intended.
TEST_P(BleTest, CanStartThenStopScanning) {
  env_.Start();
  ::nearby::BluetoothAdapter adapter;
  Ble ble(adapter);

  ScanRequest scan_request{
      .power_mode = PowerMode::kBalanced,
  };
  ScanningCallback scanning_callback;
  nearby::CountDownLatch started_scanning_latch(1);

  std::unique_ptr<ScanningSession> scannning_session = ble.StartScanning(
      scan_request, ScanningCallback{
                        .start_scanning_result =
                            [&started_scanning_latch](absl::Status status) {
                              if (status.ok()) {
                                started_scanning_latch.CountDown();
                              }
                            },
                    });

  EXPECT_TRUE(started_scanning_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(GetBleStatus(ble).has_value() &&
              GetBleStatus(ble).value().is_scanning == true);
  absl::Status stop_scanning_status = scannning_session->stop_scanning();
  EXPECT_OK(stop_scanning_status);
  EXPECT_TRUE(GetBleStatus(ble).has_value() &&
              GetBleStatus(ble).value().is_scanning == false);
  env_.Stop();
}

TEST_P(BleTest, AdvertiseAndScan) {
  // Create two Ble devices, one advertises, the other one scans, and verify
  // that the NP advertisement was sent from one to the other.
  env_.Start();
  nearby::BluetoothAdapter client_adapter;
  Ble client(client_adapter);
  nearby::BluetoothAdapter server_adapter;
  Ble server(server_adapter);
  AdvertisementData advert_data = {.is_extended_advertisement = false,
                                   .content = "my advertisement"};
  ScanRequest scan_request{
      .power_mode = PowerMode::kBalanced,
  };
  nearby::CountDownLatch advertise_latch(1);
  nearby::CountDownLatch scan_latch(1);
  std::vector<BleAdvertisementData> advertisements;
  std::unique_ptr<ScanningSession> scanning_session = client.StartScanning(
      scan_request,
      ScanningCallback{.advertisement_found_cb =
                           [&](BlePeripheral& peripheral,
                               BleAdvertisementData advertisement_data) {
                             advertisements.push_back(advertisement_data);
                             scan_latch.CountDown();
                           }});
  std::unique_ptr<nearby::api::ble_v2::BleMedium::AdvertisingSession>
      advertising_session = server.StartAdvertising(
          advert_data, PowerMode::kBalanced,
          AdvertisingCallback{
              .start_advertising_result = [&](absl::Status status) {
                advertise_latch.CountDown();
              }});

  EXPECT_TRUE(advertise_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(scan_latch.Await(kWaitDuration).result());
  EXPECT_OK(scanning_session->stop_scanning());
  EXPECT_OK(advertising_session->stop_advertising());
  ASSERT_FALSE(advertisements.empty());
  EXPECT_EQ(advertisements[0]
                .service_data.find(kPresenceServiceUuid)
                ->second.AsStringView(),
            advert_data.content);
  env_.Stop();
}

}  // namespace
}  // namespace presence
}  // namespace nearby
