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

#include "fastpair/scanning/fastpair/fast_pair_discoverable_scanner_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "fastpair/scanning/fastpair/fast_pair_scanner_impl.h"
#include "fastpair/server_access/fake_fast_pair_repository.h"
#include "fastpair/testing/fast_pair_service_data_creator.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/medium_environment.h"

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
            .SetModelId(std::string(model_id))
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

class FastPairDiscoverableScannerImplTest : public testing::Test {
 public:
  void SetUp() override {
    SetUpMetadata();
    scanner_ = std::make_shared<FastPairScannerImpl>();
    adapter_ = std::make_shared<BluetoothAdapter>();
  }

  void SetUpMetadata() {
    repository_ = std::make_unique<FakeFastPairRepository>();
    proto::Device metadata;
    metadata.set_device_type(proto::DeviceType::TRUE_WIRELESS_HEADPHONES);
    repository_->SetFakeMetadata(kValidModelId, metadata);
  }

  // void TearDown() override { discoverable_scanner_.reset(); }

 protected:
  std::shared_ptr<FastPairScannerImpl> scanner_;
  std::unique_ptr<FakeFastPairRepository> repository_;
  std::shared_ptr<BluetoothAdapter> adapter_;
  DeviceCallback found_device_callback_;
  DeviceCallback lost_device_callback_;
};

TEST_F(FastPairDiscoverableScannerImplTest, ValidModelId) {
  absl::Notification found_notification;
  absl::Notification lost_notification;
  found_device_callback_ = [&found_notification](FastPairDevice& device) {
    found_notification.Notify();
  };
  lost_device_callback_ = [&lost_notification](FastPairDevice& device) {
    lost_notification.Notify();
  };
  std::unique_ptr<FastPairDiscoverableScanner>
      discoverable_scanner_from_factory =
          FastPairDiscoverableScannerImpl::Factory::Create(
              scanner_, adapter_, std::move(found_device_callback_),
              std::move(lost_device_callback_));

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->OnDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_TRUE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
  scanner_->OnDeviceLost(BlePeripheral(ble_peripheral.get()));
  EXPECT_TRUE(lost_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerImplTest, InvalidModelId) {
  absl::Notification found_notification;
  found_device_callback_ = [&found_notification](FastPairDevice& device) {
    found_notification.Notify();
  };
  std::unique_ptr<FastPairDiscoverableScanner>
      discoverable_scanner_from_factory =
          FastPairDiscoverableScannerImpl::Factory::Create(
              scanner_, adapter_, std::move(found_device_callback_),
              std::move(lost_device_callback_));

  auto ble_peripheral = std::make_unique<FakeBlePeripheral>(
      kTestBleDeviceAddress, kInvalidModelId);
  scanner_->OnDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerImplTest, NoServiceData) {
  absl::Notification found_notification;
  found_device_callback_ = [&found_notification](FastPairDevice& device) {
    found_notification.Notify();
  };
  std::unique_ptr<FastPairDiscoverableScanner>
      discoverable_scanner_from_factory =
          FastPairDiscoverableScannerImpl::Factory::Create(
              scanner_, adapter_, std::move(found_device_callback_),
              std::move(lost_device_callback_));

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, "");
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerImplTest, UnsupportedDeviceType) {
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::AUTOMOTIVE);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  absl::Notification found_notification;
  found_device_callback_ = [&found_notification](FastPairDevice& device) {
    found_notification.Notify();
  };
  std::unique_ptr<FastPairDiscoverableScanner>
      discoverable_scanner_from_factory =
          FastPairDiscoverableScannerImpl::Factory::Create(
              scanner_, adapter_, std::move(found_device_callback_),
              std::move(lost_device_callback_));

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerImplTest, UnsupportedNotifictionType) {
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::HEADPHONES);
  metadata.set_notification_type(proto::NotificationType::APP_LAUNCH);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  absl::Notification found_notification;
  found_device_callback_ = [&found_notification](FastPairDevice& device) {
    found_notification.Notify();
  };
  std::unique_ptr<FastPairDiscoverableScanner>
      discoverable_scanner_from_factory =
          FastPairDiscoverableScannerImpl::Factory::Create(
              scanner_, adapter_, std::move(found_device_callback_),
              std::move(lost_device_callback_));

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerImplTest, UnspecifiedNotificationType) {
  // Set metadata to mimic a device that doesn't specify the notification
  // or device type. Since we aren't sure what this device is, we'll show
  // the notification to be safe.
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::DEVICE_TYPE_UNSPECIFIED);
  metadata.set_notification_type(
      proto::NotificationType::NOTIFICATION_TYPE_UNSPECIFIED);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  absl::Notification found_notification;
  found_device_callback_ = [&found_notification](FastPairDevice& device) {
    found_notification.Notify();
  };
  std::unique_ptr<FastPairDiscoverableScanner>
      discoverable_scanner_from_factory =
          FastPairDiscoverableScannerImpl::Factory::Create(
              scanner_, adapter_, std::move(found_device_callback_),
              std::move(lost_device_callback_));

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_TRUE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerImplTest, V1NotificationType) {
  // Set metadata to mimic a V1 device which advertises with no device
  // type and a notification type of FAST_PAIR_ONE.
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::DEVICE_TYPE_UNSPECIFIED);
  metadata.set_notification_type(proto::NotificationType::FAST_PAIR_ONE);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  absl::Notification found_notification;
  found_device_callback_ = [&found_notification](FastPairDevice& device) {
    found_notification.Notify();
  };
  std::unique_ptr<FastPairDiscoverableScanner>
      discoverable_scanner_from_factory =
          FastPairDiscoverableScannerImpl::Factory::Create(
              scanner_, adapter_, std::move(found_device_callback_),
              std::move(lost_device_callback_));

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_TRUE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerImplTest, V2NotificationType) {
  // Set metadata to mimic a V2 device which advertises with a device
  // type of TRUE_WIRELESS_HEADPHONES and a notification type of FAST_PAIR.
  proto::Device metadata;
  metadata.set_device_type(proto::DeviceType::TRUE_WIRELESS_HEADPHONES);
  metadata.set_notification_type(proto::NotificationType::FAST_PAIR);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  absl::Notification found_notification;
  found_device_callback_ = [&found_notification](FastPairDevice& device) {
    found_notification.Notify();
  };
  std::unique_ptr<FastPairDiscoverableScanner>
      discoverable_scanner_from_factory =
          FastPairDiscoverableScannerImpl::Factory::Create(
              scanner_, adapter_, std::move(found_device_callback_),
              std::move(lost_device_callback_));

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->NotifyDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_TRUE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerImplTest, NearbyShareModelId) {
  absl::Notification found_notification;
  found_device_callback_ = [&found_notification](FastPairDevice& device) {
    found_notification.Notify();
  };
  std::unique_ptr<FastPairDiscoverableScanner>
      discoverable_scanner_from_factory =
          FastPairDiscoverableScannerImpl::Factory::Create(
              scanner_, adapter_, std::move(found_device_callback_),
              std::move(lost_device_callback_));

  auto ble_peripheral = std::make_unique<FakeBlePeripheral>(
      kTestBleDeviceAddress, kNearbyShareModelId);
  scanner_->OnDeviceFound(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(found_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST_F(FastPairDiscoverableScannerImplTest,
       DoesntInvokeLostCallbackIfDidntInvokeFound) {
  absl::Notification found_notification;
  absl::Notification lost_notification;
  found_device_callback_ = [&found_notification](FastPairDevice& device) {
    found_notification.Notify();
  };
  lost_device_callback_ = [&lost_notification](FastPairDevice& device) {
    lost_notification.Notify();
  };
  std::unique_ptr<FastPairDiscoverableScanner>
      discoverable_scanner_from_factory =
          FastPairDiscoverableScannerImpl::Factory::Create(
              scanner_, adapter_, std::move(found_device_callback_),
              std::move(lost_device_callback_));

  auto ble_peripheral =
      std::make_unique<FakeBlePeripheral>(kTestBleDeviceAddress, kValidModelId);
  scanner_->OnDeviceLost(BlePeripheral(ble_peripheral.get()));
  EXPECT_FALSE(lost_notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
