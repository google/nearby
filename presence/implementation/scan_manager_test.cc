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

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/types/variant.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/proto/credential.proto.h"
#include "presence/broadcast_request.h"
#include "presence/data_element.h"
#include "presence/data_types.h"
#include "presence/implementation/advertisement_factory.h"
#include "presence/implementation/base_broadcast_request.h"
#include "presence/implementation/credential_manager_impl.h"
#include "presence/implementation/mediums/advertisement_data.h"
#include "presence/implementation/mediums/ble.h"
#include "presence/implementation/mediums/mediums.h"
#include "presence/implementation/mock_credential_manager.h"
#include "presence/power_mode.h"
#include "presence/presence_action.h"
#include "presence/presence_device.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {
namespace {

using AdvertisingSession = ::nearby::api::ble_v2::BleMedium::AdvertisingSession;
using AdvertisingCallback =
    ::nearby::api::ble_v2::BleMedium::AdvertisingCallback;
using ::nearby::SingleThreadExecutor;

using CountDownLatch = ::nearby::CountDownLatch;
// using ::testing::UnorderedElementsAre;
using ::testing::Contains;

class ScanManagerTest : public testing::Test {
 protected:
  void SetUp() override { env_.Start(); }
  void TearDown() override {
    executor_.Shutdown();
    env_.Stop();
  }

  std::unique_ptr<AdvertisingSession> StartAdvertisingOn(Ble& ble) {
    PresenceBroadcast::BroadcastSection section = {
        .identity = internal::IDENTITY_TYPE_PUBLIC,
        .extended_properties = MakeDefaultExtendedProperties(),
        .account_name = "Test account"};
    PresenceBroadcast presence_request = {.sections = {section}};
    BroadcastRequest input = {.tx_power = 30, .variant = presence_request};
    absl::StatusOr<BaseBroadcastRequest> request =
        BaseBroadcastRequest::Create(input);
    EXPECT_OK(request);
    absl::StatusOr<AdvertisementData> advertisement =
        AdvertisementFactory().CreateAdvertisement(request.value());
    EXPECT_OK(advertisement);
    std::unique_ptr<AdvertisingSession> session = ble.StartAdvertising(
        advertisement.value(), PowerMode::kLowPower,
        AdvertisingCallback{.start_advertising_result = [](absl::Status) {}});
    env_.Sync();
    return session;
  }

  ScanRequest MakeDefaultScanRequest() {
    std::vector<absl::variant<PresenceScanFilter, LegacyPresenceScanFilter>>
        filters = {PresenceScanFilter{
            .scan_type = ScanType::kPresenceScan,
            .extended_properties = MakeDefaultExtendedProperties(),
        }};
    return {
        .account_name = "Test account",
        .identity_types = MakeDefaultIdentityTypes(),
        .scan_filters = filters,
        .use_ble = true,
        .scan_type = ScanType::kPresenceScan,
        .power_mode = PowerMode::kBalanced,
        .scan_only_when_screen_on = true,
    };
  }

  ScanCallback MakeDefaultScanCallback() {
    return {.start_scan_cb =
                [this](absl::Status status) {
                  if (status.ok()) {
                    start_latch_.CountDown();
                  }
                },
            .on_discovered_cb =
                [this](PresenceDevice pd) { found_latch_.CountDown(); }};
  }

  std::vector<nearby::internal::IdentityType> MakeDefaultIdentityTypes() {
    return {
        nearby::internal::IdentityType::IDENTITY_TYPE_PUBLIC,
    };
  }
  std::vector<DataElement> MakeDefaultExtendedProperties() {
    return {DataElement(ActionBit::kPresenceManagerAction)};
  }
  SingleThreadExecutor executor_;
  CredentialManagerImpl credential_manager_{&executor_};
  nearby::MediumEnvironment& env_ = {nearby::MediumEnvironment::Instance()};
  CountDownLatch start_latch_{1};
  CountDownLatch found_latch_{1};
};

TEST_F(ScanManagerTest, CanStartThenStopScanning) {
  Mediums mediums;
  ScanManager manager(mediums, credential_manager_, executor_);
  // Set up advertiser
  nearby::BluetoothAdapter server_adapter;
  Ble ble2(server_adapter);
  std::unique_ptr<AdvertisingSession> advertising_session =
      StartAdvertisingOn(ble2);

  // Start scanning
  ScanSessionId scan_session =
      manager.StartScan(MakeDefaultScanRequest(), MakeDefaultScanCallback());
  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 1);
  EXPECT_TRUE(start_latch_.Await().Ok());
  EXPECT_TRUE(found_latch_.Await().Ok());
  manager.StopScan(scan_session);
  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 0);
}

TEST_F(ScanManagerTest, CannotStopScanTwice) {
  Mediums mediums;
  ScanManager manager(mediums, credential_manager_, executor_);

  ScanSessionId scan_session =
      manager.StartScan(MakeDefaultScanRequest(), MakeDefaultScanCallback());

  NEARBY_LOGS(INFO) << "Start scan";
  EXPECT_TRUE(start_latch_.Await().Ok());
  // Ensure that we have started scanning before we try to stop.
  env_.Sync();
  NEARBY_LOGS(INFO) << "Stop scan";
  manager.StopScan(scan_session);
  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 0);
  NEARBY_LOGS(INFO) << "Stop scan again";
  manager.StopScan(scan_session);
  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 0);
}

TEST_F(ScanManagerTest, TestNoFilter) {
  Mediums mediums;
  ScanManager manager(mediums, credential_manager_, executor_);
  // Set up advertiser
  nearby::BluetoothAdapter server_adapter;
  Ble ble2(server_adapter);
  std::unique_ptr<AdvertisingSession> advertising_session =
      StartAdvertisingOn(ble2);

  // Start scanning
  ScanRequest scan_request_no_filter = MakeDefaultScanRequest();
  scan_request_no_filter.scan_filters.clear();
  ScanSessionId scan_session =
      manager.StartScan(scan_request_no_filter, MakeDefaultScanCallback());

  ASSERT_EQ(manager.ScanningCallbacksLengthForTest(), 1);
  ASSERT_TRUE(mediums.GetBle().IsAvailable());
  EXPECT_TRUE(start_latch_.Await().Ok());
  EXPECT_TRUE(found_latch_.Await().Ok());
  manager.StopScan(scan_session);
  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 0);
}

TEST_F(ScanManagerTest, PresenceMetadataIsRetained) {
  Mediums mediums;
  ScanManager manager(mediums, credential_manager_, executor_);
  // Set up advertiser
  nearby::BluetoothAdapter server_adapter;
  Ble ble2(server_adapter);
  std::unique_ptr<AdvertisingSession> advertising_session =
      StartAdvertisingOn(ble2);
  std::string address = server_adapter.GetMacAddress();
  ScanCallback callback = {
      .start_scan_cb =
          [this](absl::Status status) {
            if (status.ok()) {
              start_latch_.CountDown();
            }
          },
      .on_discovered_cb =
          [this, &address](PresenceDevice pd) {
            if (pd.GetDeviceIdentityMetadata().bluetooth_mac_address() ==
                address) {
              EXPECT_THAT(
                  pd.GetExtendedProperties(),
                  Contains(
                      DataElement(DataElement::kPublicIdentityFieldType, ""))
                      .Times(1));
              // MakeDefaultScanRequest() used kPresenceManagerAction for
              // broadcasting. Thus verify DE and action got it recorded.
              EXPECT_THAT(
                  pd.GetExtendedProperties(),
                  Contains(DataElement(ActionBit::kPresenceManagerAction))
                      .Times(1));
              EXPECT_THAT(pd.GetActions(),
                          Contains(PresenceAction{
                                       (int)ActionBit::kPresenceManagerAction})
                              .Times(1));

              found_latch_.CountDown();
            }
          }};
  // Start scanning
  ScanRequest scan_request_no_filter = MakeDefaultScanRequest();
  scan_request_no_filter.scan_filters.clear();
  auto scan_session =
      manager.StartScan(scan_request_no_filter, std::move(callback));

  ASSERT_EQ(manager.ScanningCallbacksLengthForTest(), 1);
  ASSERT_TRUE(mediums.GetBle().IsAvailable());
  EXPECT_TRUE(start_latch_.Await().Ok());
  EXPECT_TRUE(found_latch_.Await().Ok());
  manager.StopScan(scan_session);
  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 0);
}

TEST_F(ScanManagerTest, StopOneSessionFromAnotherDeadlock) {
  Mediums mediums;
  ScanManager manager(mediums, credential_manager_, executor_);
  CountDownLatch start_latch2{1};
  CountDownLatch found_latch2{1};

  // Start scanning
  std::vector<DataElement> extended_properties_mismatch = {
      DataElement(ActionBit::kInstantTetheringAction)};
  std::vector<absl::variant<PresenceScanFilter, LegacyPresenceScanFilter>>
      mismatch_filters = {PresenceScanFilter{
          .scan_type = ScanType::kPresenceScan,
          .extended_properties = extended_properties_mismatch,
      }};
  ScanRequest scan_request_mismatch = {
      .account_name = "Test account",
      .identity_types = MakeDefaultIdentityTypes(),
      .scan_filters = mismatch_filters,
      .use_ble = true,
      .scan_type = ScanType::kPresenceScan,
      .power_mode = PowerMode::kBalanced,
      .scan_only_when_screen_on = true,
  };
  // we use scan_request_mismatch so this session's discovery doesn't get
  // triggered.
  ScanSessionId scan_session =
      manager.StartScan(scan_request_mismatch, MakeDefaultScanCallback());
  ScanCallback scanning_callback2 = {.start_scan_cb =
                                         [&](absl::Status status) {
                                           if (status.ok()) {
                                             start_latch2.CountDown();
                                           }
                                         },
                                     .on_discovered_cb =
                                         [&](PresenceDevice pd) {
                                           NEARBY_LOGS(INFO)
                                               << "scansession2 found";
                                           found_latch2.CountDown();
                                           manager.StopScan(scan_session);
                                         }};
  ScanSessionId scan_session2 = manager.StartScan(
      MakeDefaultScanRequest(), std::move(scanning_callback2));

  ASSERT_EQ(manager.ScanningCallbacksLengthForTest(), 2);

  // Set up advertiser
  nearby::BluetoothAdapter server_adapter;
  Ble ble2(server_adapter);
  std::unique_ptr<AdvertisingSession> advertising_session =
      StartAdvertisingOn(ble2);

  EXPECT_TRUE(found_latch2.Await().Ok());
  // Session was stopped before, this should not be able to stop successfully.
  manager.StopScan(scan_session);
  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 1);
  ASSERT_TRUE(mediums.GetBle().IsAvailable());
  manager.StopScan(scan_session2);
  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 0);
}

// Receive a BLE advertisement after StopScan. `on_discovered_cb`
// must not be called.
TEST_F(ScanManagerTest, NoDeviceFoundAfterStopScan) {
  Mediums mediums;
  ScanManager manager(mediums, credential_manager_, executor_);
  CountDownLatch start_scan_latch{1};
  nearby::BluetoothAdapter server_adapter;
  Ble ble2(server_adapter);
  std::atomic_bool stopped = false;
  ScanSessionId scan_session = manager.StartScan(
      MakeDefaultScanRequest(),
      ScanCallback{.start_scan_cb =
                       [&start_scan_latch](absl::Status status) {
                         if (status.ok()) {
                           start_scan_latch.CountDown();
                         }
                       },
                   .on_discovered_cb =
                       [&](PresenceDevice pd) { EXPECT_FALSE(stopped); }});

  start_scan_latch.Await();
  manager.StopScan(scan_session);
  stopped = true;
  std::unique_ptr<AdvertisingSession> advertising_session =
      StartAdvertisingOn(ble2);

  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 0);
  executor_.Shutdown();
}

internal::SharedCredential GetPublicCredential() {
  // Values copied from LDT tests
  ByteArray seed({204, 219, 36, 137, 233, 252, 172, 66, 179, 147, 72,
                  184, 148, 30, 209, 154, 29,  54,  14, 117, 224, 152,
                  200, 193, 94, 107, 28,  194, 182, 32, 205, 57});
  ByteArray known_mac({0xB4, 0xC5, 0x9F, 0xA5, 0x99, 0x24, 0x1B, 0x81,
                       0x75, 0x8D, 0x97, 0x6B, 0x5A, 0x62, 0x1C, 0x05,
                       0x23, 0x2F, 0xE1, 0xBF, 0x89, 0xAE, 0x59, 0x87,
                       0xCA, 0x25, 0x4C, 0x35, 0x54, 0xDC, 0xE5, 0x0E});
  internal::SharedCredential public_credential;
  public_credential.set_key_seed(seed.AsStringView());
  public_credential.set_metadata_encryption_key_tag_v0(
      known_mac.AsStringView());
  return public_credential;
}

std::vector<internal::SharedCredential> BuildSharedCredentials() {
  return {GetPublicCredential()};
}

internal::LocalCredential CreateLocalCredential(
    internal::IdentityType identity_type) {
  // Values copied from LDT tests
  ByteArray seed({204, 219, 36, 137, 233, 252, 172, 66, 179, 147, 72,
                  184, 148, 30, 209, 154, 29,  54,  14, 117, 224, 152,
                  200, 193, 94, 107, 28,  194, 182, 32, 205, 57});
  ByteArray metadata_key(
      {205, 104, 63, 225, 161, 209, 248, 70, 84, 61, 10, 19, 212, 174});

  internal::LocalCredential private_credential;
  private_credential.set_identity_type(identity_type);
  private_credential.set_key_seed(seed.AsStringView());
  private_credential.set_metadata_encryption_key_v0(
      metadata_key.AsStringView());
  return private_credential;
}

TEST_F(ScanManagerTest, ScanningE2EWithEncryptedAdvertisementAndCredentials) {
  Mediums mediums;
  auto mock_credential_manager = MockCredentialManager();
  EXPECT_CALL(mock_credential_manager, GetPublicCredentials)
      .WillOnce([&](const CredentialSelector& credential_selector,
                    PublicCredentialType public_credential_type,
                    GetPublicCredentialsResultCallback callback) {
        callback.credentials_fetched_cb(BuildSharedCredentials());
      });
  ScanManager manager(mediums, mock_credential_manager, executor_);

  // Set up advertiser to broadcast a private identity adv
  nearby::BluetoothAdapter server_adapter;
  Ble ble2(server_adapter);
  PresenceBroadcast::BroadcastSection section = {
      .identity = internal::IdentityType::IDENTITY_TYPE_PRIVATE,
      .extended_properties = MakeDefaultExtendedProperties(),
      .account_name = "Test account"};
  PresenceBroadcast presence_request = {.sections = {section}};
  BroadcastRequest input = {.tx_power = 30, .variant = presence_request};
  absl::StatusOr<BaseBroadcastRequest> request =
      BaseBroadcastRequest::Create(input);
  EXPECT_OK(request);
  absl::StatusOr<AdvertisementData> advertisement =
      AdvertisementFactory().CreateAdvertisement(
          request.value(),
          CreateLocalCredential(internal::IdentityType::IDENTITY_TYPE_PRIVATE));
  EXPECT_OK(advertisement);
  std::unique_ptr<AdvertisingSession> session = ble2.StartAdvertising(
      advertisement.value(), PowerMode::kLowPower,
      AdvertisingCallback{.start_advertising_result = [](absl::Status) {}});
  env_.Sync();

  auto scan_request = MakeDefaultScanRequest();
  scan_request.identity_types = {
      nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE};

  // Start scanning
  ScanSessionId scan_session =
      manager.StartScan(scan_request, MakeDefaultScanCallback());
  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 1);
  EXPECT_TRUE(start_latch_.Await().Ok());
  EXPECT_TRUE(found_latch_.Await().Ok());
  manager.StopScan(scan_session);
  EXPECT_EQ(manager.ScanningCallbacksLengthForTest(), 0);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
