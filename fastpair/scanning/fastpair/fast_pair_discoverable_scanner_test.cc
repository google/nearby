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

#include "fastpair/scanning/fastpair/fast_pair_discoverable_scanner.h"

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "fastpair/repository/fake_fast_pair_repository.h"
#include "fastpair/repository/fast_pair_device_repository.h"
#include "fastpair/scanning/fastpair/fake_fast_pair_scanner.h"
#include "fastpair/testing/fast_pair_service_data_creator.h"
#include "internal/platform/bluetooth_adapter.h"

namespace nearby {
namespace fastpair {
namespace {

constexpr absl::Duration kWaitTimeout = absl::Milliseconds(200);
constexpr char kValidModelId[] = "718c17";
constexpr char kInvalidModelId[] = "12345";
constexpr char kNearbyShareModelId[] = "fc128e";
constexpr char kTestBleDeviceAddress[] = "11:12:13:14:15:16";

class FakeBlePeripheral : public api::BlePeripheral {
 public:
  explicit FakeBlePeripheral(absl::string_view name,
                             absl::string_view model_id) {
    name_ = std::string(name);
    const std::vector<uint8_t> service_data =
        FastPairServiceDataCreator::Builder()
            .SetModelId(model_id)
            .Build()
            ->CreateServiceData();
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

class FastPairDiscoverableScannerTest : public ::testing::Test {
 protected:
  void TearDown() override { executor_.Shutdown(); }

  SingleThreadExecutor executor_;
  FastPairDeviceRepository devices_{&executor_};
  std::unique_ptr<FakeFastPairScanner> scanner_;
  std::unique_ptr<FastPairDiscoverableScanner> discoverable_scanner_;
};

TEST_F(FastPairDiscoverableScannerTest, ValidModelId) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::TRUE_WIRELESS_HEADPHONES);
  repository->SetFakeMetadata(kValidModelId, metadata);
  absl::Notification found_notification;
  absl::Notification lost_notification;

  discoverable_scanner_ = FastPairDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  found_notification.WaitForNotification();
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  lost_notification.WaitForNotification();
}

TEST_F(FastPairDiscoverableScannerTest, InvalidModelId) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::TRUE_WIRELESS_HEADPHONES);
  repository->SetFakeMetadata(kValidModelId, metadata);
  absl::Notification found_notification;
  absl::Notification lost_notification;

  discoverable_scanner_ = FastPairDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  auto ble_peripheral = std::make_unique<FakeBlePeripheral>(
      kTestBleDeviceAddress, kInvalidModelId);
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(lost_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerTest, NoServiceData) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::TRUE_WIRELESS_HEADPHONES);
  repository->SetFakeMetadata(kValidModelId, metadata);
  absl::Notification found_notification;
  absl::Notification lost_notification;

  discoverable_scanner_ = FastPairDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, "");
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(lost_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerTest, UnsupportedDeviceType) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::AUTOMOTIVE);
  repository->SetFakeMetadata(kValidModelId, metadata);

  absl::Notification found_notification;
  absl::Notification lost_notification;
  discoverable_scanner_ = FastPairDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(lost_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerTest, UnsupportedNotifictionType) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::HEADPHONES);
  metadata.set_notification_type(proto::NotificationType::APP_LAUNCH);
  repository->SetFakeMetadata(kValidModelId, metadata);

  absl::Notification found_notification;
  absl::Notification lost_notification;
  discoverable_scanner_ = FastPairDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(lost_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerTest, UnspecifiedNotificationType) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  // Set metadata to mimic a device that doesn't specify the notification
  // or device type. Since we aren't sure what this device is, we'll show
  // the notification to be safe.
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::DEVICE_TYPE_UNSPECIFIED);
  metadata.set_notification_type(
      proto::NotificationType::NOTIFICATION_TYPE_UNSPECIFIED);
  repository->SetFakeMetadata(kValidModelId, metadata);

  absl::Notification found_notification;
  absl::Notification lost_notification;
  discoverable_scanner_ = FastPairDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  found_notification.WaitForNotification();
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  lost_notification.WaitForNotification();
}

TEST_F(FastPairDiscoverableScannerTest, V1NotificationType) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  // Set metadata to mimic a V1 device which advertises with no device
  // type and a notification type of FAST_PAIR_ONE.
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::DEVICE_TYPE_UNSPECIFIED);
  metadata.set_notification_type(proto::NotificationType::FAST_PAIR_ONE);
  repository->SetFakeMetadata(kValidModelId, metadata);

  absl::Notification found_notification;
  absl::Notification lost_notification;
  discoverable_scanner_ = FastPairDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  found_notification.WaitForNotification();
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  lost_notification.WaitForNotification();
}

TEST_F(FastPairDiscoverableScannerTest, V2NotificationType) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  // Set metadata to mimic a V2 device which advertises with a device
  // type of TRUE_WIRELESS_HEADPHONES and a notification type of FAST_PAIR.
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::TRUE_WIRELESS_HEADPHONES);
  metadata.set_notification_type(proto::NotificationType::FAST_PAIR);
  repository->SetFakeMetadata(kValidModelId, metadata);

  absl::Notification found_notification;
  absl::Notification lost_notification;
  discoverable_scanner_ = FastPairDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  found_notification.WaitForNotification();
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  lost_notification.WaitForNotification();
}

TEST_F(FastPairDiscoverableScannerTest, NearbyShareModelId) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::TRUE_WIRELESS_HEADPHONES);
  repository->SetFakeMetadata(kValidModelId, metadata);
  absl::Notification found_notification;
  absl::Notification lost_notification;
  discoverable_scanner_ = FastPairDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  auto ble_peripheral = std::make_unique<FakeBlePeripheral>(
      kTestBleDeviceAddress, kNearbyShareModelId);
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(lost_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerTest,
       DoesntInvokeLostCallbackIfDidntInvokeFound) {
  scanner_ = std::make_unique<FakeFastPairScanner>();
  auto repository = std::make_unique<FakeFastPairRepository>();
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::TRUE_WIRELESS_HEADPHONES);
  repository->SetFakeMetadata(kValidModelId, metadata);
  absl::Notification found_notification;
  absl::Notification lost_notification;
  discoverable_scanner_ = FastPairDiscoverableScanner::Factory::Create(
      *scanner_, [&](FastPairDevice& device) { found_notification.Notify(); },
      [&](FastPairDevice& device) { lost_notification.Notify(); }, &executor_,
      &devices_);

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->NotifyDeviceLost(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(lost_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
