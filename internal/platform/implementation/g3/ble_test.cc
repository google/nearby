// Copyright 2025 Google LLC
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

#include "internal/platform/implementation/g3/ble.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "third_party/gloop/thread/fiber/fiber.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/g3/bluetooth_adapter.h"
#include "internal/platform/mac_address.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace g3 {
namespace {

using ::testing::Return;

constexpr absl::string_view kTestUuid = "00000000-0000-0000-0000-000000000000";
constexpr absl::string_view kTestServiceId = "test_service_id";

class MockBluetoothAdapter : public BluetoothAdapter {
 public:
  MOCK_METHOD(bool, IsValid, (), (const));
  MOCK_METHOD(bool, SetStatus, (api::BluetoothAdapter::Status status),
              (override));
  MOCK_METHOD(bool, IsEnabled, (), (const, override));
  MOCK_METHOD(api::BluetoothAdapter::Status, GetStatus, (), (const));
  MOCK_METHOD(bool, SetName, (absl::string_view name, bool persist),
              (override));
  MOCK_METHOD(std::string, GetName, (), (const, override));
  MOCK_METHOD(MacAddress, GetMacAddress, (), (const, override));
  MOCK_METHOD(bool, SetScanMode, (api::BluetoothAdapter::ScanMode scan_mode),
              (override));
  MOCK_METHOD(api::BluetoothAdapter::ScanMode, GetScanMode, (),
              (const, override));
  MOCK_METHOD(int, GetNumSlots, (), (const));
  MOCK_METHOD(api::ble::BlePeripheral::UniqueId, GetUniqueId, (), (const));
  MOCK_METHOD(void, SetBleMedium, (api::ble::BleMedium * medium));
};

TEST(BleMediumTest, StartAdvertising) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleMedium ble_medium(mock_bluetooth_adapter);
  api::ble::BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = false;
  advertising_data.service_data.insert(
      {Uuid(kTestUuid), ByteArray("test_service_data")});

  auto advertising_session = ble_medium.StartAdvertising(
      advertising_data,
      {.tx_power_level = api::ble::TxPowerLevel::kLow, .is_connectable = true},
      api::ble::BleMedium::AdvertisingCallback{
          .start_advertising_result = [](absl::Status) {},
      });
  EXPECT_TRUE(advertising_session != nullptr);
}

TEST(BleMediumTest, StopAdvertising) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleMedium ble_medium(mock_bluetooth_adapter);
  EXPECT_TRUE(ble_medium.StopAdvertising());
}

TEST(BleMediumTest, StartScanning) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleMedium ble_medium(mock_bluetooth_adapter);
  auto scanning_session = ble_medium.StartScanning(
      Uuid(kTestUuid), api::ble::TxPowerLevel::kLow,
      api::ble::BleMedium::ScanningCallback{
          .start_scanning_result = [](absl::Status) {},
          .advertisement_found_cb = [](api::ble::BlePeripheral::UniqueId,
                                       api::ble::BleAdvertisementData) {},
      });
  EXPECT_TRUE(scanning_session != nullptr);
}

TEST(BleMediumTest, StopScanning) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleMedium ble_medium(mock_bluetooth_adapter);
  EXPECT_TRUE(ble_medium.StopScanning());
}

TEST(BleMediumTest, StartGattServer) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleMedium ble_medium(mock_bluetooth_adapter);
  auto gatt_server = ble_medium.StartGattServer({});
  EXPECT_TRUE(gatt_server != nullptr);
}

TEST(BleMediumTest, ConnectToGattServer) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleMedium ble_medium(mock_bluetooth_adapter);
  auto gatt_client = ble_medium.ConnectToGattServer(
      /*peripheral_id=*/1234, api::ble::TxPowerLevel::kLow, {});
  EXPECT_TRUE(gatt_client == nullptr);
}

TEST(BleMediumTest, OpenServerSocket) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleMedium ble_medium(mock_bluetooth_adapter);
  auto server_socket = ble_medium.OpenServerSocket(std::string(kTestServiceId));
  EXPECT_TRUE(server_socket != nullptr);
}

TEST(BleMediumTest, Connect) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleMedium ble_medium(mock_bluetooth_adapter);
  auto socket = ble_medium.Connect(
      std::string(kTestServiceId), api::ble::TxPowerLevel::kLow,
      /*remote_peripheral_id=*/1234, /*cancellation_flag=*/nullptr);
  EXPECT_TRUE(socket == nullptr);
}

TEST(BleMediumTest, OpenL2capServerSocket) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleMedium ble_medium(mock_bluetooth_adapter);
  auto server_socket =
      ble_medium.OpenL2capServerSocket(std::string(kTestServiceId));
  EXPECT_TRUE(server_socket == nullptr);
}

TEST(BleMediumTest, IsExtendedAdvertisementsAvailable) {
  MockBluetoothAdapter mock_bluetooth_adapter;
  EXPECT_CALL(mock_bluetooth_adapter, IsEnabled()).WillRepeatedly(Return(true));

  BleMedium ble_medium(mock_bluetooth_adapter);
  EXPECT_FALSE(ble_medium.IsExtendedAdvertisementsAvailable());
}

TEST(BleMediumTest, CancelConnectOverL2capDoesNotCloseRemoteServerSocket) {
  MediumEnvironment::Instance().Start();
  MediumEnvironment::Instance().SetBleL2capAvailable(true);
  BluetoothAdapter server_adapter;
  BluetoothAdapter client1_adapter;
  BluetoothAdapter client2_adapter;
  BleMedium server_medium(server_adapter);
  BleMedium client1_medium(client1_adapter);
  BleMedium client2_medium(client2_adapter);

  std::unique_ptr<api::ble::BleL2capServerSocket> server_socket =
      server_medium.OpenL2capServerSocket(std::string(kTestServiceId));
  ASSERT_NE(server_socket, nullptr);
  int psm = server_socket->GetPSM();

  CancellationFlag cancel_flag1;
  absl::Notification client1_started;
  absl::Notification client1_done;
  std::unique_ptr<api::ble::BleL2capSocket> client1_socket;
  thread::Fiber client1_fiber([&]() {
    client1_started.Notify();
    client1_socket = client1_medium.ConnectOverL2cap(
        psm, std::string(kTestServiceId), api::ble::TxPowerLevel::kHigh,
        server_adapter.GetUniqueId(), &cancel_flag1);
    client1_done.Notify();
  });

  ASSERT_TRUE(client1_started.WaitForNotificationWithTimeout(absl::Seconds(1)));
  absl::SleepFor(absl::Milliseconds(50));
  cancel_flag1.Cancel();
  EXPECT_TRUE(client1_done.WaitForNotificationWithTimeout(absl::Seconds(1)));
  client1_fiber.Join();
  EXPECT_EQ(client1_socket, nullptr);

  // Subsequent client connection still succeeds because cancelling client1 did
  // not close the server socket.
  std::unique_ptr<api::ble::BleL2capSocket> accepted_socket;
  absl::Notification accept_done;
  thread::Fiber accept_fiber([&]() {
    accepted_socket = server_socket->Accept();
    accept_done.Notify();
  });

  CancellationFlag cancel_flag2;
  std::unique_ptr<api::ble::BleL2capSocket> client2_socket =
      client2_medium.ConnectOverL2cap(
          psm, std::string(kTestServiceId), api::ble::TxPowerLevel::kHigh,
          server_adapter.GetUniqueId(), &cancel_flag2);
  EXPECT_TRUE(accept_done.WaitForNotificationWithTimeout(absl::Seconds(1)));
  accept_fiber.Join();

  ASSERT_NE(client2_socket, nullptr);
  ASSERT_NE(accepted_socket, nullptr);
  client2_socket->Close();
  accepted_socket->Close();
  server_socket->Close();
  MediumEnvironment::Instance().Stop();
}

TEST(BleMediumTest, DestroyL2capServerSocketDuringPendingConnectOverL2cap) {
  MediumEnvironment::Instance().Start();
  MediumEnvironment::Instance().SetBleL2capAvailable(true);
  BluetoothAdapter server_adapter;
  BluetoothAdapter client_adapter;
  BleMedium server_medium(server_adapter);
  BleMedium client_medium(client_adapter);

  std::unique_ptr<api::ble::BleL2capServerSocket> server_socket =
      server_medium.OpenL2capServerSocket(std::string(kTestServiceId));
  ASSERT_NE(server_socket, nullptr);
  int psm = server_socket->GetPSM();

  CancellationFlag cancel_flag;
  absl::Notification client_started;
  absl::Notification client_done;
  std::unique_ptr<api::ble::BleL2capSocket> client_socket;
  thread::Fiber client_fiber([&]() {
    client_started.Notify();
    client_socket = client_medium.ConnectOverL2cap(
        psm, std::string(kTestServiceId), api::ble::TxPowerLevel::kHigh,
        server_adapter.GetUniqueId(), &cancel_flag);
    client_done.Notify();
  });

  ASSERT_TRUE(client_started.WaitForNotificationWithTimeout(absl::Seconds(1)));
  absl::SleepFor(absl::Milliseconds(50));

  // Destroy the server socket while ConnectOverL2cap is blocked, then trigger
  // cancellation as well.
  server_socket.reset();
  cancel_flag.Cancel();

  EXPECT_TRUE(client_done.WaitForNotificationWithTimeout(absl::Seconds(1)));
  client_fiber.Join();
  EXPECT_EQ(client_socket, nullptr);
  MediumEnvironment::Instance().Stop();
}

TEST(BleMediumTest, ConnectL2capRechecksCancellationAfterAcceptWakesUp) {
  MediumEnvironment::Instance().Start();
  BluetoothAdapter server_adapter;
  BluetoothAdapter client_adapter;
  BleMedium server_medium(server_adapter);
  BleMedium client_medium(client_adapter);
  BleL2capServerSocket server_socket(&server_adapter, 0x0080);
  auto client_socket = std::make_unique<BleL2capSocket>(&client_adapter);
  CancellationFlag cancel_flag;

  absl::Notification client_done;
  bool connected = true;
  bool client_socket_closed = false;
  thread::Fiber client_fiber([&]() {
    connected = server_socket.Connect(*client_socket, &cancel_flag);
    client_socket_closed = client_socket->IsClosed();
    if (!connected) {
      client_socket.reset();
    }
    client_done.Notify();
  });

  server_socket.WaitForPendingConnection();

  // Simulate cancellation firing before Connect() wakes from Accept()'s signal
  // without CancelConnect() having closed the pending socket.
  cancel_flag.Cancel();
  std::unique_ptr<api::ble::BleL2capSocket> accepted_socket =
      server_socket.Accept();
  ASSERT_NE(accepted_socket, nullptr);

  EXPECT_TRUE(client_done.WaitForNotificationWithTimeout(absl::Seconds(1)));
  client_fiber.Join();
  EXPECT_FALSE(connected);
  EXPECT_TRUE(client_socket_closed);
  EXPECT_EQ(client_socket, nullptr);
  EXPECT_EQ(accepted_socket->GetRemotePeripheralId(),
            client_adapter.GetUniqueId());
  accepted_socket->Close();
  server_socket.Close();
  MediumEnvironment::Instance().Stop();
}

}  // namespace
}  // namespace g3
}  // namespace nearby
