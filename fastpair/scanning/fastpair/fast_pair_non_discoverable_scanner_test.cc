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

#include "fastpair/scanning/fastpair/fast_pair_non_discoverable_scanner.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "fastpair/common/account_key.h"
#include "fastpair/repository/fake_fast_pair_repository.h"
#include "fastpair/scanning/fastpair/fake_fast_pair_scanner.h"
#include "fastpair/testing/fast_pair_service_data_creator.h"

namespace nearby {
namespace fastpair {
namespace {
constexpr int kNotDiscoverableAdvHeader = 0b00000110;
constexpr int kAccountKeyFilterHeader = 0b01100000;
constexpr int kAccountKeyFilterNoNotificationHeader = 0b01100010;
constexpr int kSaltHeader = 0b00010001;
constexpr absl::string_view kModelId("aabbcc");
constexpr absl::string_view kBleAddress("11:12:13:14:15:16");
constexpr absl::string_view kAccountKeyFilter("112233445566");
constexpr absl::string_view kSalt("01");

// Short timeout for operation that we expect to timeout.
constexpr absl::Duration kFailureTimeout = absl::Milliseconds(100);

class FakeBlePeripheral : public api::BlePeripheral {
 public:
  explicit FakeBlePeripheral(absl::string_view name,
                             std::vector<uint8_t> service_data) {
    name_ = std::string(name);
    ByteArray advertisement_bytes(
        std::string(service_data.begin(), service_data.end()));
    advertisement_data_ = advertisement_bytes;
  }

  FakeBlePeripheral(const FakeBlePeripheral&) = default;
  ~FakeBlePeripheral() override = default;

  std::string GetName() const override { return name_; }

  ByteArray GetAdvertisementBytes(
      const std::string& service_id) const override {
    return advertisement_data_;
  }

  void SetName(const std::string& name) { name_ = name; }

  void SetAdvertisementBytes(ByteArray advertisement_bytes) {
    advertisement_data_ = advertisement_bytes;
  }

 private:
  std::string name_;
  ByteArray advertisement_data_;
};

class FastPairNonDiscoverableScannerTest : public ::testing::Test {
 protected:
  void TearDown() override {
    executor_.Shutdown();
  }

  std::vector<uint8_t> GetAdvServicedata() {
    return FastPairServiceDataCreator::Builder()
        .SetHeader(kNotDiscoverableAdvHeader)
        .SetModelId(kModelId)
        .AddExtraFieldHeader(kAccountKeyFilterHeader)
        .AddExtraField(kAccountKeyFilter)
        .AddExtraFieldHeader(kSaltHeader)
        .AddExtraField(kSalt)
        .Build()
        ->CreateServiceData();
  }

  std::vector<uint8_t> GetAdvNoUiServicedata() {
    return FastPairServiceDataCreator::Builder()
        .SetHeader(kNotDiscoverableAdvHeader)
        .SetModelId(kModelId)
        .AddExtraFieldHeader(kAccountKeyFilterNoNotificationHeader)
        .AddExtraField(kAccountKeyFilter)
        .AddExtraFieldHeader(kSaltHeader)
        .AddExtraField(kSalt)
        .Build()
        ->CreateServiceData();
  }

  std::vector<uint8_t> GetAdvWrongServicedata() {
    return FastPairServiceDataCreator::Builder()
        .SetHeader(kNotDiscoverableAdvHeader)
        .SetModelId(kModelId)
        .AddExtraFieldHeader /*InvalidType*/ (0b01100001)
        .AddExtraField(kAccountKeyFilter)
        .AddExtraFieldHeader(kSaltHeader)
        .AddExtraField(kSalt)
        .Build()
        ->CreateServiceData();
  }

  SingleThreadExecutor executor_;
  FastPairDeviceRepository devices_{&executor_};
  std::unique_ptr<FakeFastPairScanner> scanner_;
  std::unique_ptr<FastPairNonDiscoverableScanner> non_discoverable_scanner_;
};

TEST_F(FastPairNonDiscoverableScannerTest, FoundAssociatedDevice) {
  const std::vector<uint8_t> account_key_vec{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                             0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                             0xCC, 0xDD, 0xEE, 0xFF};
  scanner_ = std::make_unique<FakeFastPairScanner>();
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  repository->SetFakeMetadata(kModelId, metadata);
  AccountKey account_key(account_key_vec);
  repository->SetResultOfCheckIfAssociatedWithCurrentAccount(account_key,
                                                             kModelId);

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kBleAddress, GetAdvServicedata());

  absl::Notification found_notification;
  absl::Notification lost_notification;
  non_discoverable_scanner_ = FastPairNonDiscoverableScanner::Factory::Create(
      *scanner_,
      [&](FastPairDevice& device) {
        EXPECT_EQ(device.GetModelId(), kModelId);
        EXPECT_EQ(device.GetBleAddress(), kBleAddress);
        EXPECT_EQ(device.GetProtocol(), Protocol::kFastPairSubsequentPairing);
        EXPECT_EQ(device.GetAccountKey(), account_key);
        EXPECT_TRUE(device.ShouldShowUiNotification());
        found_notification.Notify();
      },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  found_notification.WaitForNotification();
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  lost_notification.WaitForNotification();
}

TEST_F(FastPairNonDiscoverableScannerTest, FoundNonAssociatedDevice) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  repository->SetFakeMetadata(kModelId, metadata);
  repository->SetResultOfCheckIfAssociatedWithCurrentAccount(std::nullopt,
                                                             std::nullopt);

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kBleAddress, GetAdvServicedata());

  absl::Notification found_notification;
  absl::Notification lost_notification;
  non_discoverable_scanner_ = FastPairNonDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(
      found_notification.WaitForNotificationWithTimeout(kFailureTimeout));
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(
      lost_notification.WaitForNotificationWithTimeout(kFailureTimeout));
}

TEST_F(FastPairNonDiscoverableScannerTest, FoundNoUIDevice) {
  const std::vector<uint8_t> account_key_vec{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                             0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                             0xCC, 0xDD, 0xEE, 0xFF};
  scanner_ = std::make_unique<FakeFastPairScanner>();
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  repository->SetFakeMetadata(kModelId, metadata);
  AccountKey account_key(account_key_vec);
  repository->SetResultOfCheckIfAssociatedWithCurrentAccount(account_key,
                                                             kModelId);

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kBleAddress, GetAdvNoUiServicedata());

  absl::Notification found_notification;
  absl::Notification lost_notification;
  non_discoverable_scanner_ = FastPairNonDiscoverableScanner::Factory::Create(
      *scanner_,
      [&](FastPairDevice& device) {
        EXPECT_EQ(device.GetModelId(), kModelId);
        EXPECT_EQ(device.GetBleAddress(), kBleAddress);
        EXPECT_EQ(device.GetProtocol(), Protocol::kFastPairSubsequentPairing);
        EXPECT_EQ(device.GetAccountKey(), account_key);
        EXPECT_FALSE(device.ShouldShowUiNotification().value());
        found_notification.Notify();
      },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  found_notification.WaitForNotification();
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  lost_notification.WaitForNotification();
}

TEST_F(FastPairNonDiscoverableScannerTest, FailedToGetDeviceMetadata) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  auto repository = std::make_unique<FakeFastPairRepository>();
  repository->SetResultOfCheckIfAssociatedWithCurrentAccount(AccountKey(),
                                                             kModelId);

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kBleAddress, GetAdvServicedata());

  absl::Notification found_notification;
  absl::Notification lost_notification;
  non_discoverable_scanner_ = FastPairNonDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(
      found_notification.WaitForNotificationWithTimeout(kFailureTimeout));
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(
      lost_notification.WaitForNotificationWithTimeout(kFailureTimeout));
}

TEST_F(FastPairNonDiscoverableScannerTest, NoServiceData) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  repository->SetFakeMetadata(kModelId, metadata);
  repository->SetResultOfCheckIfAssociatedWithCurrentAccount(AccountKey(),
                                                             kModelId);

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kBleAddress, std::vector<uint8_t>());

  absl::Notification found_notification;
  absl::Notification lost_notification;
  non_discoverable_scanner_ = FastPairNonDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(
      found_notification.WaitForNotificationWithTimeout(kFailureTimeout));
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(
      lost_notification.WaitForNotificationWithTimeout(kFailureTimeout));
}

TEST_F(FastPairNonDiscoverableScannerTest, FailedToParseNonDiscoverableAdv) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  repository->SetFakeMetadata(kModelId, metadata);
  repository->SetResultOfCheckIfAssociatedWithCurrentAccount(AccountKey(),
                                                             kModelId);

  auto ble_peripheral = std::make_unique<FakeBlePeripheral>(
      kBleAddress, GetAdvWrongServicedata());

  absl::Notification found_notification;
  absl::Notification lost_notification;
  non_discoverable_scanner_ = FastPairNonDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(
      found_notification.WaitForNotificationWithTimeout(kFailureTimeout));
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(
      lost_notification.WaitForNotificationWithTimeout(kFailureTimeout));
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
