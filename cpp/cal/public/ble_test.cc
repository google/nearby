// Copyright 2021 Google LLC
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

#include "third_party/nearby/cpp/cal/api/ble.h"

#include <memory>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "third_party/nearby/cpp/cal/base/ble_types.h"
#include "third_party/nearby/cpp/cal/public/ble.h"

namespace nearby {
namespace cal {
namespace {

class MockBlePeripheral : public api::BlePeripheral {
  MOCK_METHOD(std::string, GetName, (), (const override));
  MOCK_METHOD(std::string, GetAddress, (), (const override));
  MOCK_METHOD(ByteArray, GetAdvertisementBytes, (), (const override));
};

class MockGattCharacteristic : public api::GattCharacteristic {
  MOCK_METHOD(std::string, GetServiceUuid, (), (override));
};

class MockClientGattConnection : public api::ClientGattConnection {
 public:
  MOCK_METHOD(api::BlePeripheral&, GetPeripheral, (), (override));
  MOCK_METHOD(bool, DiscoverServices, (), (override));
  MOCK_METHOD(absl::optional<api::GattCharacteristic*>, GetCharacteristic,
              (absl::string_view service_uuid,
               absl::string_view characteristic_uuid),
              (override));
  MOCK_METHOD(absl::optional<ByteArray>, ReadCharacteristic,
              (const api::GattCharacteristic& characteristic), (override));
  MOCK_METHOD(bool, WriteCharacteristic,
              (const api::GattCharacteristic& characteristic,
               const ByteArray& value),
              (override));
  MOCK_METHOD(void, Disconnect, (), (override));
  MOCK_METHOD(bool, SetCharacteristicNotification,
              (const api::GattCharacteristic& characteristic, bool enable),
              (override));
};

class MockServerGattConnection : public api::ServerGattConnection {
 public:
  MOCK_METHOD(bool, SendCharacteristic,
              (const api::GattCharacteristic& characteristic,
               const ByteArray& value),
              (override));
};

class MockClientGattConnectionLifeCycleCallback
    : public api::ClientGattConnectionLifeCycleCallback {
 public:
  MOCK_METHOD(void, OnDisconnected, (api::ClientGattConnection * connection),
              (override));
  MOCK_METHOD(void, onConnectionStateChange,
              (api::ClientGattConnection * connection), (override));
  MOCK_METHOD(void, onCharacteristicRead,
              (api::ClientGattConnection * connection), (override));
};

class MockServerGattConnectionLifeCycleCallback
    : public api::ServerGattConnectionLifeCycleCallback {
 public:
  MOCK_METHOD(void, OnCharacteristicSubscription,
              (api::ServerGattConnection * connection,
               const api::GattCharacteristic& characteristic),
              (override));
  MOCK_METHOD(void, OnCharacteristicUnsubscription,
              (api::ServerGattConnection * connection,
               const api::GattCharacteristic& characteristic),
              (override));
};

class MockGattServer : public api::GattServer {
 public:
  MOCK_METHOD(std::unique_ptr<api::GattCharacteristic>, CreateCharacteristic,
              (absl::string_view service_uuid,
               absl::string_view characteristic_uuid,
               const std::set<api::GattCharacteristic::Permission>& permissions,
               const std::set<api::GattCharacteristic::Property>& properties),
              (override));
  MOCK_METHOD(bool, UpdateCharacteristic,
              (const api::GattCharacteristic& characteristic,
               const ByteArray& value),
              (override));
  MOCK_METHOD(void, Stop, (), (override));
};

class MockBleSocket : public api::BleSocket {
 public:
  MOCK_METHOD(api::BlePeripheral&, GetRemotePeripheral, (), (override));
  MOCK_METHOD(Exception, Write, (const ByteArray& message), (override));
  MOCK_METHOD(Exception, Close, (), (override));
};

class MockBleSocketLifeCycleCallback : public api::BleSocketLifeCycleCallback {
 public:
  MOCK_METHOD(void, OnMessageReceived,
              (api::BleSocket * socket, const ByteArray& message), (override));
  MOCK_METHOD(void, OnDisconnected, (api::BleSocket * socket), (override));
};

class MockServerBleSocketLifeCycleCallback
    : public api::ServerBleSocketLifeCycleCallback {
 public:
  MOCK_METHOD(void, OnSocketEstablished, (api::BleSocket * socket), (override));
};

class MockBleMedium : public api::BleMedium {
 public:
  MOCK_METHOD(bool, StartAdvertising,
              (const BleAdvertisementData& advertisement_data), (override));
  MOCK_METHOD(void, StopAdvertising, (const std::string& service_id),
              (override));
  class MockScanCallback : public api::BleMedium::ScanCallback {
   public:
    MOCK_METHOD(void, OnAdvertisementFound,
                (const api::ScanResult& scan_result,
                 const BleAdvertisementData& advertisement_data),
                (override));
  };
  MOCK_METHOD(bool, StartScanning,
              (const std::set<std::string>& service_uuids, PowerMode power_mode,
               const ScanCallback& scan_callback),
              (override));
  MOCK_METHOD(void, StopScanning, (), (override));
  MOCK_METHOD(std::unique_ptr<api::GattServer>, StartGattServer,
              (const api::ServerGattConnectionLifeCycleCallback& callback),
              (override));
  MOCK_METHOD(bool, StartListeningForIncomingBleSockets,
              (const api::ServerBleSocketLifeCycleCallback& callback),
              (override));
  MOCK_METHOD(void, StopListeningForIncomingBleSockets, (), (override));
  MOCK_METHOD(std::unique_ptr<api::ClientGattConnection>, ConnectToGattServer,
              (api::BlePeripheral * peripheral, Mtu mtu, PowerMode power_mode,
               const api::ClientGattConnectionLifeCycleCallback& callback),
              (override));
  MOCK_METHOD(std::unique_ptr<api::BleSocket>, EstablishBleSocket,
              (api::BlePeripheral * peripheral,
               const api::BleSocketLifeCycleCallback& callback),
              (override));
};

class BleTest : public ::testing::Test {
 public:
  ::nearby::cal::BleMedium ble_medium_{std::make_unique<MockBleMedium>()};
  const std::string kServiceId = "BleTest";
  const AdvertiseSettings kAdvertiseSettings = AdvertiseSettings{};
  const ByteArray kAdvertisementBytes = ByteArray{"BleTestBytes"};
  const std::string kServiceUuid = "0x2fec";
  const BleAdvertisementData kBleAdvertisementData = {
      kAdvertiseSettings, {{kServiceUuid, kAdvertisementBytes}}};
};

TEST_F(BleTest, ConstructorWorks) { EXPECT_TRUE(ble_medium_.IsValid()); }

TEST_F(BleTest, StartAdvertising) {
  MockBleMedium* mock_ble_medium =
      static_cast<MockBleMedium*>(&ble_medium_.GetImpl());
  EXPECT_CALL(*mock_ble_medium, StartAdvertising).Times(1);
  ble_medium_.StartAdvertising(kServiceId, kAdvertiseSettings,
                               kAdvertisementBytes, kServiceId);
}

}  // namespace
}  // namespace cal
}  // namespace nearby
