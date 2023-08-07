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

#include "fastpair/keyed_service/fast_pair_mediator.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "fastpair/testing/fast_pair_service_data_creator.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "fastpair/ui/mock_ui_broker.h"
#include "fastpair/ui/ui_broker.h"
#include "internal/account/fake_account_manager.h"
#include "internal/network/http_client_impl.h"
#include "internal/platform/device_info_impl.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_http_client.h"
#include "internal/test/google3_only/fake_authentication_manager.h"

namespace nearby {
namespace fastpair {
namespace {

constexpr absl::string_view kModelId = "718c17";
constexpr absl::string_view kServiceID = "Fast Pair";
constexpr absl::string_view kFastPairServiceUuid =
    "0000FE2C-0000-1000-8000-00805F9B34FB";
constexpr int kNotDiscoverableAdvHeader = 0b00000110;
constexpr int kAccountKeyFilterHeader = 0b01100000;
constexpr int kSaltHeader = 0b00010001;
constexpr absl::string_view kAccountKeyFilter("112233445566");
constexpr absl::string_view kSalt("01");

constexpr absl::string_view kModelId2 = "9adb11";
constexpr absl::string_view kAddress = "74:74:46:01:6C:21";

class MediatorTest : public testing::Test {
 public:
  MediatorTest() {
    AccountManagerImpl::Factory::SetFactoryForTesting(
        &account_manager_factory_);
    http_client_ = std::make_unique<network::FakeHttpClient>();
    device_info_ = std::make_unique<FakeDeviceInfo>();
    authentication_manager_ =
        std::make_unique<nearby::FakeAuthenticationManager>();
  }
  void SetUp() override {
    env_.Start();
    GetAuthManager()->EnableSyncMode();
    mediums_ = std::make_unique<Mediums>();
    ui_broker_ = std::make_unique<MockUIBroker>();
    mock_ui_broker_ = static_cast<MockUIBroker*>(ui_broker_.get());

    notification_controller_ =
        std::make_unique<FastPairNotificationController>();

    executor_ = std::make_unique<SingleThreadExecutor>();
  }

  void TearDown() override {
    executor_.reset();
    mediums_.reset();
    ui_broker_.reset();
    mock_ui_broker_ = nullptr;
    notification_controller_.reset();
    mediator_.reset();
    env_.Stop();
  }

  nearby::FakeAuthenticationManager* GetAuthManager() {
    return reinterpret_cast<nearby::FakeAuthenticationManager*>(
        authentication_manager_.get());
  }

  network::FakeHttpClient* GetHttpClient() {
    return reinterpret_cast<network::FakeHttpClient*>(http_client_.get());
  }

  void SetUpDeviceMetadata() {
    proto::GetObservedDeviceResponse response_proto;
    auto* device = response_proto.mutable_device();
    int64_t device_id;
    CHECK(absl::SimpleHexAtoi(kModelId, &device_id));
    device->set_id(device_id);
    network::HttpResponse response;
    response.SetStatusCode(network::HttpStatusCode::kHttpOk);
    response.SetBody(response_proto.SerializeAsString());
    GetHttpClient()->SetResponseForSyncRequest(response);
  }

 protected:
  MediumEnvironment& env_{MediumEnvironment::Instance()};
  std::unique_ptr<Mediums> mediums_;
  std::unique_ptr<UIBroker> ui_broker_;
  std::unique_ptr<FastPairNotificationController> notification_controller_;
  std::unique_ptr<SingleThreadExecutor> executor_;
  MockUIBroker* mock_ui_broker_;
  std::unique_ptr<Mediator> mediator_;
  FakeAccountManager::Factory account_manager_factory_;
  std::unique_ptr<auth::AuthenticationManager> authentication_manager_;
  std::unique_ptr<network::HttpClient> http_client_;
  std::unique_ptr<DeviceInfo> device_info_;
};

TEST_F(MediatorTest, StartScanningFoundDevice) {
  SetUpDeviceMetadata();
  // Create Fast Pair Mediator
  mediator_ = std::make_unique<Mediator>(
      std::move(executor_), std::move(mediums_), std::move(ui_broker_),
      std::move(notification_controller_), std::move(authentication_manager_),
      std::move(http_client_), std::move(device_info_));
  absl::Notification done;
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).WillOnce([&done] {
    done.Notify();
  });

  // Create Advertiser and startAdvertising
  Mediums mediums_advertiser;
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{absl::HexStringToBytes(kModelId)};
  std::string fast_pair_service_uuid(kFastPairServiceUuid);
  mediums_advertiser.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes, fast_pair_service_uuid);

  mediator_->StartScanning();
  done.WaitForNotification();
}

TEST_F(MediatorTest, StartScanningFoundDifferentDeviceWhenDisplaying) {
  SetUpDeviceMetadata();
  // Create Fast Pair Mediator
  mediator_ = std::make_unique<Mediator>(
      std::move(executor_), std::move(mediums_), std::move(ui_broker_),
      std::move(notification_controller_), std::move(authentication_manager_),
      std::move(http_client_), std::move(device_info_));
  absl::Notification done;
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1).WillOnce([&done] {
    done.Notify();
  });

  // Create Advertiser and startAdvertising
  Mediums mediums_advertiser;
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{absl::HexStringToBytes(kModelId)};
  std::string fast_pair_service_uuid(kFastPairServiceUuid);
  mediums_advertiser.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes, fast_pair_service_uuid);

  // Create a different Advertiser and startAdvertising to cause confliction
  Mediums mediums_advertiser2;
  ByteArray advertisement_bytes2{absl::HexStringToBytes(kModelId2)};
  mediums_advertiser2.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes2, fast_pair_service_uuid);

  mediator_->StartScanning();
  done.WaitForNotification();
}

TEST_F(MediatorTest, StartScanningFoundSameDeviceWhenDisplaying) {
  SetUpDeviceMetadata();
  // Create Fast Pair Mediator
  mediator_ = std::make_unique<Mediator>(
      std::move(executor_), std::move(mediums_), std::move(ui_broker_),
      std::move(notification_controller_), std::move(authentication_manager_),
      std::move(http_client_), std::move(device_info_));
  absl::Notification done;
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1).WillOnce([&done] {
    done.Notify();
  });

  // Create Advertiser and startAdvertising
  Mediums mediums_advertiser;
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{absl::HexStringToBytes(kModelId)};
  std::string fast_pair_service_uuid(kFastPairServiceUuid);
  mediums_advertiser.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes, fast_pair_service_uuid);

  // Create another same Advertiser and startAdvertising to cause confliction
  Mediums mediums_advertiser2;
  ByteArray advertisement_bytes2{absl::HexStringToBytes(kModelId)};
  mediums_advertiser2.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes2, fast_pair_service_uuid);

  mediator_->StartScanning();
  done.WaitForNotification();
}

TEST_F(MediatorTest,
       StartScanningForSubsequentPairingFoundSameDeviceWhenDisplaying) {
  SetUpDeviceMetadata();
  // Create Fast Pair Mediator
  mediator_ = std::make_unique<Mediator>(
      std::move(executor_), std::move(mediums_), std::move(ui_broker_),
      std::move(notification_controller_), std::move(authentication_manager_),
      std::move(http_client_), std::move(device_info_));
  absl::Notification done;
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1).WillOnce([&done] {
    done.Notify();
  });

  // Create Advertiser and advertising discoverable advertisement
  Mediums mediums_advertiser;
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{absl::HexStringToBytes(kModelId)};
  std::string fast_pair_service_uuid(kFastPairServiceUuid);
  mediums_advertiser.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes, fast_pair_service_uuid);

  // Create another same Advertiser and advertising non-discoverable
  // advertisement to cause confliction
  Mediums mediums_advertiser2;
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kSaltHeader)
                                   .AddExtraField(kSalt)
                                   .Build()
                                   ->CreateServiceData();
  ByteArray advertisement_bytes2{std::string(bytes.begin(), bytes.end())};
  mediums_advertiser2.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes2, fast_pair_service_uuid);

  mediator_->StartScanning();
  done.WaitForNotification();
}

TEST_F(MediatorTest, OnDiscoveryActionClicked) {
  SetUpDeviceMetadata();
  SetUpDeviceMetadata();
  // Create Fast Pair Mediator
  mediator_ = std::make_unique<Mediator>(
      std::move(executor_), std::move(mediums_), std::move(ui_broker_),
      std::move(notification_controller_), std::move(authentication_manager_),
      std::move(http_client_), std::move(device_info_));

  absl::Notification done;
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(2).WillOnce([&done] {
    done.Notify();
  });

  // Create Advertiser and startAdvertising
  Mediums mediums_advertiser;
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{absl::HexStringToBytes(kModelId)};
  std::string fast_pair_service_uuid(kFastPairServiceUuid);
  mediums_advertiser.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes, fast_pair_service_uuid);

  mediator_->StartScanning();
  done.WaitForNotification();

  FastPairDevice device(kModelId, kAddress, Protocol::kFastPairInitialPairing);
  mock_ui_broker_->NotifyDiscoveryAction(device,
                                         DiscoveryAction::kDismissedByTimeout);
  // Create another same Advertiser and startAdvertising to cause confliction
  Mediums mediums_advertiser2;
  ByteArray advertisement_bytes2{absl::HexStringToBytes(kModelId)};
  mediums_advertiser2.GetBle().GetMedium().StartAdvertising(
      service_id, advertisement_bytes2, fast_pair_service_uuid);

  done.WaitForNotification();
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby