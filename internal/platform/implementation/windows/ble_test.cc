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

#include "internal/platform/implementation/windows/ble.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace windows {
namespace {
using ::testing::Return;

constexpr uint64_t kTestServiceUuidMsb = 0x0000ABCD00001000;
constexpr uint64_t kTestServiceUuidLsb = 0x800000805F9B34FB;

constexpr Uuid kTestServiceUuid(kTestServiceUuidMsb, kTestServiceUuidLsb);

class MockBluetoothAdapter : public BluetoothAdapter {
 public:
  MOCK_METHOD(bool, SetStatus, (Status status), (override));
  MOCK_METHOD(bool, IsEnabled, (), (const, override));
  MOCK_METHOD(ScanMode, GetScanMode, (), (const, override));
  MOCK_METHOD(bool, SetScanMode, (ScanMode scan_mode), (override));
  MOCK_METHOD(std::string, GetName, (), (const, override));
  MOCK_METHOD(bool, SetName, (absl::string_view name), (override));
  MOCK_METHOD(bool, SetName, (absl::string_view name, bool persist),
              (override));
  MOCK_METHOD(std::string, GetMacAddress, (), (const, override));
};

}  // namespace

class BleMediumTest : public ::testing::Test {
 protected:
  BleMediumTest() {
    ble_medium_ = std::make_unique<BleMedium>(bluetooth_adapter_);
  }

 protected:
  size_t GetWatchersCount() {
    absl::MutexLock lock(&ble_medium_->mutex_);
    return ble_medium_->watchers_.size();
  }

  MockBluetoothAdapter bluetooth_adapter_;
  std::unique_ptr<BleMedium> ble_medium_;
};

namespace {

TEST_F(BleMediumTest, StartScanningAdapterDisabled) {
  EXPECT_CALL(bluetooth_adapter_, IsEnabled()).WillOnce(Return(false));
  Uuid service_uuid = kTestServiceUuid;
  api::ble::BleMedium::ScanCallback callback;

  EXPECT_EQ(false, ble_medium_->StartScanning(service_uuid,
                                              api::ble::TxPowerLevel::kHigh,
                                              std::move(callback)));

  EXPECT_EQ(GetWatchersCount(), 0);
}

TEST_F(BleMediumTest, StartScanningAdapterNoAdapterOnForge) {
  EXPECT_CALL(bluetooth_adapter_, IsEnabled()).WillOnce(Return(true));
  Uuid service_uuid = kTestServiceUuid;
  api::ble::BleMedium::ScanCallback callback;

  EXPECT_EQ(false, ble_medium_->StartScanning(service_uuid,
                                              api::ble::TxPowerLevel::kHigh,
                                              std::move(callback)));

  EXPECT_EQ(GetWatchersCount(), 0);
}

TEST(BleMedium, DISABLED_StartAdvertising) {
  BluetoothAdapter bluetoothAdapter;
  BleMedium ble_medium(bluetoothAdapter);

  api::ble::BleAdvertisementData advertising_data;
  api::ble::BleAdvertisementData scan_response_data;

  EXPECT_TRUE(ble_medium.StartAdvertising(
      advertising_data, {.tx_power_level = api::ble::TxPowerLevel::kHigh,
                         .is_connectable = true}));
}

TEST(BleMedium, DISABLED_StopAdvertising) {
  BluetoothAdapter bluetoothAdapter;
  BleMedium ble_medium(bluetoothAdapter);

  api::ble::BleAdvertisementData advertising_data;

  EXPECT_TRUE(ble_medium.StartAdvertising(
      advertising_data, {.tx_power_level = api::ble::TxPowerLevel::kHigh,
                         .is_connectable = true}));
  EXPECT_TRUE(ble_medium.StopAdvertising());
}

TEST(BleMedium, DISABLED_StartScanning) {
  bool scan_response_received = false;
  absl::Notification scan_response_notification;

  BluetoothAdapter bluetoothAdapter;
  BleMedium ble_medium(bluetoothAdapter);
  Uuid service_uuid;

  api::ble::BleMedium::ScanCallback callback;
  callback.advertisement_found_cb =
      [&scan_response_received, &scan_response_notification](
          api::ble::BlePeripheral::UniqueId peripheral_id,
          const api::ble::BleAdvertisementData& advertisement_data) {
        scan_response_received = true;
        scan_response_notification.Notify();
      };

  EXPECT_TRUE(ble_medium.StartScanning(
      service_uuid, api::ble::TxPowerLevel::kHigh, std::move(callback)));

  EXPECT_TRUE(scan_response_notification.WaitForNotificationWithTimeout(
      absl::Seconds(5)));
  EXPECT_TRUE(scan_response_received);
}

TEST(BleMedium, DISABLED_StopScanning) {
  BluetoothAdapter bluetoothAdapter;
  BleMedium ble_medium(bluetoothAdapter);
  Uuid service_uuid;

  api::ble::BleMedium::ScanCallback callback;
  callback.advertisement_found_cb =
      [](api::ble::BlePeripheral::UniqueId peripheral_id,
         const api::ble::BleAdvertisementData& advertisement_data) {};

  EXPECT_TRUE(ble_medium.StartScanning(
      service_uuid, api::ble::TxPowerLevel::kHigh, std::move(callback)));

  EXPECT_TRUE(ble_medium.StopScanning());
}

TEST(BleMedium, DISABLED_StartThenStopScanning) {
  bool scan_response_received = false;
  absl::Notification scan_response_notification;

  BluetoothAdapter bluetoothAdapter;
  BleMedium ble_medium(bluetoothAdapter);
  Uuid service_uuid;

  api::ble::BleMedium::ScanningCallback callback;
  callback.advertisement_found_cb =
      [&scan_response_received, &scan_response_notification](
          api::ble::BlePeripheral::UniqueId peripheral_id,
          const api::ble::BleAdvertisementData& advertisement_data) {
        scan_response_received = true;
        scan_response_notification.Notify();
      };

  auto scanning_session = ble_medium.StartScanning(
      service_uuid, api::ble::TxPowerLevel::kHigh, std::move(callback));
  EXPECT_NE(scanning_session, nullptr);
  EXPECT_TRUE(scanning_session->stop_scanning().ok());
}

}  // namespace
}  // namespace windows
}  // namespace nearby
