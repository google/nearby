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

#include "fastpair/repository/fast_pair_repository_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/proto/data.proto.h"
#include "fastpair/proto/fast_pair_string.proto.h"
#include "fastpair/proto/proto_builder.h"
#include "fastpair/server_access/fake_fast_pair_client.h"
#include "internal/platform/count_down_latch.h"

namespace nearby {
namespace fastpair {
namespace {
constexpr absl::string_view kHexModelId = "718C17";
constexpr absl::string_view kBleAddress = "11:22:33:44:55:66";
constexpr absl::string_view kPublicAddress = "20:64:DE:40:F8:93";
constexpr absl::string_view kDisplayName = "Test Device";
constexpr absl::string_view kAccountKey = "04b85786180add47fb81a04a8ce6b0de";
constexpr absl::string_view kInitialPairingdescription =
    "InitialPairingdescription";
constexpr absl::string_view kExpectedSha256Hash =
    "6353c0075a35b7d81bb30a6190ab246da4b8c55a6111d387400579133c090ed8";
constexpr absl::Duration kWaitTimeout = absl::Milliseconds(200);

// A gMock matcher to match proto values. Use this matcher like:
// request/response proto, expected_proto;
// EXPECT_THAT(proto, MatchesProto(expected_proto));
MATCHER_P(
    MatchesProto, expected_proto,
    absl::StrCat(negation ? "does not match" : "matches",
                 testing::PrintToString(expected_proto.SerializeAsString()))) {
  return arg.SerializeAsString() == expected_proto.SerializeAsString();
}

class FastPairRepositoryObserver : public FastPairRepository::Observer {
 public:
  explicit FastPairRepositoryObserver(CountDownLatch* latch) {
    latch_ = latch;
    opt_in_status_ = proto::OptInStatus::OPT_IN_STATUS_UNKNOWN;
    devices_ = std::vector<proto::FastPairDevice>();
  }

  void OnGetUserSavedDevices(
      const proto::OptInStatus& opt_in_status,
      const std::vector<proto::FastPairDevice>& devices) override {
    for (const auto& device : devices) {
      devices_.push_back(device);
    }
    opt_in_status_ = opt_in_status;
    latch_->CountDown();
  }

  CountDownLatch* latch_ = nullptr;
  proto::OptInStatus opt_in_status_;
  std::vector<proto::FastPairDevice> devices_;
};

TEST(FastPairRepositoryImplTest, MetadataDownloadSuccess) {
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  // Sets up proto::GetObservedDeviceResponse
  proto::GetObservedDeviceResponse response_proto;
  auto* device = response_proto.mutable_device();
  int64_t device_id;
  CHECK(absl::SimpleHexAtoi(kHexModelId, &device_id));
  device->set_id(device_id);
  auto* observed_device_strings = response_proto.mutable_strings();
  observed_device_strings->set_initial_pairing_description(
      kInitialPairingdescription);
  fake_fast_pair_client.SetGetObservedDeviceResponse(response_proto);

  CountDownLatch latch(1);
  fast_pair_repository->GetDeviceMetadata(
      kHexModelId, [&](std::optional<DeviceMetadata> device_metadata) {
        EXPECT_TRUE(device_metadata.has_value());
        // Verifies proto::GetObservedDeviceResponse is as expected
        proto::GetObservedDeviceResponse response =
            device_metadata->GetResponse();
        EXPECT_THAT(response, MatchesProto(response_proto));
        EXPECT_EQ(response.device().id(), device_id);
        EXPECT_EQ(response.strings().initial_pairing_description(),
                  kInitialPairingdescription);
        latch.CountDown();
      });
  latch.Await();

  // Verifies proto::GetObservedDeviceRequest is as expected.
  proto::GetObservedDeviceRequest expected_request =
      fake_fast_pair_client.get_observer_device_request();
  EXPECT_EQ(expected_request.device_id(), device_id);
  EXPECT_EQ(expected_request.mode(),
            proto::GetObservedDeviceRequest::MODE_RELEASE);
}

TEST(FastPairRepositoryImplTest, FailedToDownloadMetadata) {
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  fake_fast_pair_client.SetGetObservedDeviceResponse(
      absl::InternalError("No response"));

  CountDownLatch latch(1);
  fast_pair_repository->GetDeviceMetadata(
      kHexModelId, [&](std::optional<DeviceMetadata> device_metadata) {
        EXPECT_FALSE(device_metadata.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairRepositoryImplTest, GetUserSavedDevicesSuccess) {
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  // Sets up two devices to proto::UserReadDevicesResponse.
  // Adds device 1.
  proto::UserReadDevicesResponse response_proto;
  FastPairDevice device_1(kHexModelId, kBleAddress,
                          Protocol::kFastPairInitialPairing);
  AccountKey account_key(absl::HexStringToBytes(kAccountKey));
  device_1.SetAccountKey(account_key);
  device_1.SetPublicAddress(kPublicAddress);
  device_1.SetDisplayName(kDisplayName);
  proto::GetObservedDeviceResponse get_observed_device_response_1;
  auto* observed_device_strings_1 =
      get_observed_device_response_1.mutable_strings();
  observed_device_strings_1->set_initial_pairing_description(
      kInitialPairingdescription);
  DeviceMetadata device_metadata_1(get_observed_device_response_1);
  device_1.SetMetadata(device_metadata_1);
  auto* fast_pair_info_1 = response_proto.add_fast_pair_info();
  BuildFastPairInfo(fast_pair_info_1, device_1);
  // Adds device 2.
  auto* fast_pair_info_2 = response_proto.add_fast_pair_info();
  fast_pair_info_2->set_opt_in_status(
      proto::OptInStatus::OPT_IN_STATUS_OPTED_IN);
  fake_fast_pair_client.SetUserReadDevicesResponse(response_proto);

  // Adds FastPairRepository observer.
  CountDownLatch latch(1);
  FastPairRepositoryObserver observer(&latch);
  EXPECT_EQ(observer.devices_.size(), 0);
  EXPECT_EQ(observer.opt_in_status_, proto::OptInStatus::OPT_IN_STATUS_UNKNOWN);
  fast_pair_repository->AddObserver(&observer);

  // Get user's saved device from footprints.
  fast_pair_repository->GetUserSavedDevices();
  latch.Await();

  // Verifies user's saved devices are as expected.
  EXPECT_EQ(observer.opt_in_status_,
            proto::OptInStatus::OPT_IN_STATUS_OPTED_IN);
  EXPECT_EQ(observer.devices_.size(), 1);
  proto::FastPairDevice saved_device = observer.devices_.front();
  EXPECT_EQ(saved_device.account_key(), account_key.GetAsBytes());
  EXPECT_EQ(
      absl::BytesToHexString(saved_device.sha256_account_key_public_address()),
      kExpectedSha256Hash);
  proto::StoredDiscoveryItem stored_discovery_item;
  EXPECT_TRUE(stored_discovery_item.ParseFromString(
      saved_device.discovery_item_bytes()));
  EXPECT_EQ(stored_discovery_item.title(), kDisplayName);
  proto::FastPairStrings fast_pair_strings =
      stored_discovery_item.fast_pair_strings();
  EXPECT_EQ(fast_pair_strings.initial_pairing_description(),
            kInitialPairingdescription);
  fast_pair_repository->RemoveObserver(&observer);
}

TEST(FastPairRepositoryImplTest, FailedToGetUserSavedDevices) {
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  fake_fast_pair_client.SetUserReadDevicesResponse(
      absl::InternalError("No response"));

  // Adds FastPairRepository observer.
  CountDownLatch latch(1);
  FastPairRepositoryObserver observer(&latch);
  fast_pair_repository->AddObserver(&observer);

  // Get user's saved device from footprints.
  fast_pair_repository->GetUserSavedDevices();
  EXPECT_FALSE(latch.Await(kWaitTimeout).result());
}

TEST(FastPairRepositoryImplTest, WriteAccountAssociationToFootprintsSuccess) {
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  // Sets up proto::UserWriteDeviceResponse.
  proto::UserWriteDeviceResponse response_proto;
  fake_fast_pair_client.SetUserWriteDeviceResponse(response_proto);

  // Sets up device info to be saved to footprints.
  FastPairDevice fast_pair_device(kHexModelId, kBleAddress,
                                  Protocol::kFastPairInitialPairing);
  AccountKey account_key(absl::HexStringToBytes(kAccountKey));
  fast_pair_device.SetAccountKey(account_key);
  fast_pair_device.SetPublicAddress(kPublicAddress);
  fast_pair_device.SetDisplayName(kDisplayName);
  proto::GetObservedDeviceResponse get_observed_device_response;
  auto* observed_device_strings =
      get_observed_device_response.mutable_strings();
  observed_device_strings->set_initial_pairing_description(
      kInitialPairingdescription);
  DeviceMetadata device_metadata(get_observed_device_response);
  fast_pair_device.SetMetadata(device_metadata);

  CountDownLatch latch(1);
  fast_pair_repository->WriteAccountAssociationToFootprints(
      fast_pair_device, [&](absl::Status status) {
        // Verifies successfully write device to footprints.
        EXPECT_OK(status);
        latch.CountDown();
      });
  latch.Await();

  // Verifies proto::UserWriteDeviceRequest is as expected.
  proto::UserWriteDeviceRequest expected_request =
      fake_fast_pair_client.write_device_request();
  EXPECT_TRUE(expected_request.has_fast_pair_info());
  auto fast_proto_info = expected_request.fast_pair_info();
  EXPECT_TRUE(fast_proto_info.has_device());
  auto device = fast_proto_info.device();
  EXPECT_EQ(device.account_key(), account_key.GetAsBytes());
  EXPECT_EQ(absl::BytesToHexString(device.sha256_account_key_public_address()),
            kExpectedSha256Hash);
  proto::StoredDiscoveryItem stored_discovery_item;
  EXPECT_TRUE(
      stored_discovery_item.ParseFromString(device.discovery_item_bytes()));
  EXPECT_EQ(stored_discovery_item.title(), kDisplayName);
  proto::FastPairStrings fast_pair_strings =
      stored_discovery_item.fast_pair_strings();
  EXPECT_EQ(fast_pair_strings.initial_pairing_description(),
            kInitialPairingdescription);
}

TEST(FastPairRepositoryImplTest, FailedToWriteAccountAssociationToFootprints) {
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  fake_fast_pair_client.SetUserWriteDeviceResponse(
      absl::InternalError("No response"));

  // Sets up device info to be saved to footprints.
  FastPairDevice fast_pair_device(kHexModelId, kBleAddress,
                                  Protocol::kFastPairInitialPairing);
  AccountKey account_key(absl::HexStringToBytes(kAccountKey));
  fast_pair_device.SetAccountKey(account_key);
  fast_pair_device.SetPublicAddress(kPublicAddress);
  fast_pair_device.SetDisplayName(kDisplayName);
  proto::GetObservedDeviceResponse get_observed_device_response;
  auto* observed_device_strings =
      get_observed_device_response.mutable_strings();
  observed_device_strings->set_initial_pairing_description(
      kInitialPairingdescription);
  DeviceMetadata device_metadata(get_observed_device_response);
  fast_pair_device.SetMetadata(device_metadata);

  CountDownLatch latch(1);
  fast_pair_repository->WriteAccountAssociationToFootprints(
      fast_pair_device, [&](absl::Status status) {
        // Failed to write device to footprints.
        EXPECT_FALSE(status.ok());
        EXPECT_EQ(status.message(), "No response");
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairRepositoryImplTest, DeleteAssociatedDeviceByAccountKeySuccess) {
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  // Sets up proto::UserDeleteDeviceResponse
  proto::UserDeleteDeviceResponse response_proto;
  response_proto.set_success(true);
  fake_fast_pair_client.SetUserDeleteDeviceResponse(response_proto);

  // AccountKey of the device that will be removed from the footprint.
  AccountKey account_key(absl::HexStringToBytes(kAccountKey));

  CountDownLatch latch(1);
  fast_pair_repository->DeleteAssociatedDeviceByAccountKey(
      account_key, [&](absl::Status status) {
        // Verifies successfully delete device from the footprints.
        EXPECT_OK(status);
        latch.CountDown();
      });
  latch.Await();

  // Verifies proto::UserDeleteDeviceRequest is as expected.
  proto::UserDeleteDeviceRequest expected_request =
      fake_fast_pair_client.delete_device_request();
  std::string hex_account_key = std::string(kAccountKey);
  absl::AsciiStrToUpper(&hex_account_key);
  EXPECT_EQ(expected_request.hex_account_key(), hex_account_key);
}

TEST(FastPairRepositoryImplTest, FailedToDeleteAssociatedDeviceWithNoResponse) {
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  fake_fast_pair_client.SetUserDeleteDeviceResponse(
      absl::InternalError("No response"));

  // AccountKey of the device that will be removed from the footprint.
  AccountKey account_key(absl::HexStringToBytes(kAccountKey));

  CountDownLatch latch(1);
  fast_pair_repository->DeleteAssociatedDeviceByAccountKey(
      account_key, [&](absl::Status status) {
        // Failed to  delete device from the footprints.
        EXPECT_FALSE(status.ok());
        EXPECT_EQ(status.message(), "No response");
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairRepositoryImplTest, FailedToDeleteAssociatedDeviceWithError) {
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  // Sets up proto::UserDeleteDeviceResponse
  proto::UserDeleteDeviceResponse response_proto;
  response_proto.set_success(false);
  fake_fast_pair_client.SetUserDeleteDeviceResponse(response_proto);

  // AccountKey of the device that will be removed from the footprint.
  AccountKey account_key(absl::HexStringToBytes(kAccountKey));

  CountDownLatch latch(1);
  fast_pair_repository->DeleteAssociatedDeviceByAccountKey(
      account_key, [&](absl::Status status) {
        // Failed to delete device from the footprints.
        EXPECT_FALSE(status.ok());
        EXPECT_EQ(status.message(), "Failed to delete device.");
        latch.CountDown();
      });
  latch.Await();
}

// Test data comes from:
// https://developers.google.com/nearby/fast-pair/specifications/appendix/testcases#test_cases
TEST(FastPairRepositoryImplTest, DeviceAssociatedWithCurrentAccountSuccess) {
  const std::vector<uint8_t> filter{0x02, 0x0C, 0x80, 0x2A};
  const std::vector<uint8_t> salt{0xC7, 0xC8};
  const std::vector<uint8_t> account_key_vec{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                             0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                             0xCC, 0xDD, 0xEE, 0xFF};
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  // Sets up two devices to proto::UserReadDevicesResponse.
  proto::UserReadDevicesResponse response_proto;
  // Adds device 1.
  auto* fast_pair_info_1 = response_proto.add_fast_pair_info();
  fast_pair_info_1->set_opt_in_status(
      proto::OptInStatus::OPT_IN_STATUS_OPTED_IN);

  // Adds device 2.
  FastPairDevice device_2(kHexModelId, kBleAddress,
                          Protocol::kFastPairInitialPairing);
  AccountKey account_key(account_key_vec);
  device_2.SetAccountKey(account_key);
  device_2.SetPublicAddress(kPublicAddress);
  device_2.SetDisplayName(kDisplayName);
  proto::GetObservedDeviceResponse get_observed_device_response_1;
  auto* observed_device_strings_1 =
      get_observed_device_response_1.mutable_strings();
  observed_device_strings_1->set_initial_pairing_description(
      kInitialPairingdescription);
  DeviceMetadata device_metadata_1(get_observed_device_response_1);
  device_2.SetMetadata(device_metadata_1);
  auto* fast_pair_info_2 = response_proto.add_fast_pair_info();
  BuildFastPairInfo(fast_pair_info_2, device_2);

  fake_fast_pair_client.SetUserReadDevicesResponse(response_proto);

  AccountKeyFilter account_key_filter(filter, salt);

  CountDownLatch latch(1);
  // Get user's saved device from footprints.
  fast_pair_repository->CheckIfAssociatedWithCurrentAccount(
      account_key_filter, [&](std::optional<AccountKey> cb_account_key,
                              std::optional<absl::string_view> cb_model_id) {
        ASSERT_TRUE(cb_account_key.has_value());
        ASSERT_TRUE(cb_model_id.has_value());
        EXPECT_EQ(cb_account_key.value(), account_key);
        EXPECT_EQ(cb_model_id.value(), kHexModelId);
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairRepositoryImplTest, DeviceNotAssociatedWithCurrentAccount) {
  const std::vector<uint8_t> filter{0x02, 0x0C, 0x80, 0x2A};
  const std::vector<uint8_t> salt{0xC7, 0xC8};
  const std::vector<uint8_t> account_key_vec{0x11, 0x11, 0x22, 0x22, 0x33, 0x33,
                                             0x44, 0x44, 0x55, 0x55, 0x66, 0x66,
                                             0x77, 0x77, 0x88, 0x88};

  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  // Sets up two devices to proto::UserReadDevicesResponse.
  // Adds device 1.
  proto::UserReadDevicesResponse response_proto;
  FastPairDevice device(kHexModelId, kBleAddress,
                        Protocol::kFastPairInitialPairing);
  AccountKey account_key(account_key_vec);
  device.SetAccountKey(account_key);
  device.SetPublicAddress(kPublicAddress);
  device.SetDisplayName(kDisplayName);
  proto::GetObservedDeviceResponse get_observed_device_response;
  DeviceMetadata device_metadata(get_observed_device_response);
  device.SetMetadata(device_metadata);
  auto* fast_pair_info = response_proto.add_fast_pair_info();
  BuildFastPairInfo(fast_pair_info, device);
  fake_fast_pair_client.SetUserReadDevicesResponse(response_proto);

  AccountKeyFilter account_key_filter(filter, salt);

  CountDownLatch latch(1);
  // Get user's saved device from footprints.
  fast_pair_repository->CheckIfAssociatedWithCurrentAccount(
      account_key_filter, [&](std::optional<AccountKey> cb_account_key,
                              std::optional<absl::string_view> cb_model_id) {
        EXPECT_FALSE(cb_account_key.has_value());
        EXPECT_FALSE(cb_model_id.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairRepositoryImplTest, DeviceIsSavedToCurrentAccount) {
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  // Sets up two devices to proto::UserReadDevicesResponse.
  proto::UserReadDevicesResponse response_proto;
  // Device 1
  FastPairDevice device_1(kHexModelId, kBleAddress,
                          Protocol::kFastPairInitialPairing);
  AccountKey account_key_1(absl::HexStringToBytes(kAccountKey));
  device_1.SetAccountKey(account_key_1);
  device_1.SetPublicAddress(kPublicAddress);
  proto::GetObservedDeviceResponse get_observed_device_response;
  DeviceMetadata device_metadata_1(get_observed_device_response);
  device_1.SetMetadata(device_metadata_1);
  auto* fast_pair_info_1 = response_proto.add_fast_pair_info();
  BuildFastPairInfo(fast_pair_info_1, device_1);
  // Device 2
  auto* fast_pair_info_2 = response_proto.add_fast_pair_info();
  fast_pair_info_2->set_opt_in_status(
      proto::OptInStatus::OPT_IN_STATUS_OPTED_IN);
  fake_fast_pair_client.SetUserReadDevicesResponse(response_proto);

  CountDownLatch latch(1);
  fast_pair_repository->IsDeviceSavedToAccount(kPublicAddress,
                                               [&](absl::Status status) {
                                                 EXPECT_OK(status);
                                                 latch.CountDown();
                                               });
  latch.Await();
}

TEST(FastPairRepositoryImplTest, DeviceIsNotSavedToCurrentAccount) {
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  // Sets up two devices to proto::UserReadDevicesResponse.
  proto::UserReadDevicesResponse response_proto;
  FastPairDevice device(kHexModelId, kBleAddress,
                        Protocol::kFastPairInitialPairing);
  AccountKey account_key(absl::HexStringToBytes(kAccountKey));
  device.SetAccountKey(account_key);
  device.SetPublicAddress(kPublicAddress);
  proto::GetObservedDeviceResponse get_observed_device_response;
  DeviceMetadata device_metadata(get_observed_device_response);
  device.SetMetadata(device_metadata);
  auto* fast_pair_info = response_proto.add_fast_pair_info();
  BuildFastPairInfo(fast_pair_info, device);
  fake_fast_pair_client.SetUserReadDevicesResponse(response_proto);

  CountDownLatch latch(1);
  fast_pair_repository->IsDeviceSavedToAccount(
      "11:22:33:44:55:66", [&](absl::Status status) {
        EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairRepositoryImplTest, FailedToCheckDeviceIsSavedToCurrentAccount) {
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  CountDownLatch latch(1);
  fast_pair_repository->IsDeviceSavedToAccount(kPublicAddress,
                                               [&](absl::Status status) {
                                                 EXPECT_FALSE(status.ok());
                                                 latch.CountDown();
                                               });
  latch.Await();
}
}  // namespace
}  // namespace fastpair
}  // namespace nearby
