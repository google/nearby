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

#include "fastpair/internal/mediums/robust_gatt_client.h"

#include <memory>
#include <optional>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {
namespace {

using ::testing::status::StatusIs;
using Property = nearby::api::ble_v2::GattCharacteristic::Property;
using Permission = nearby::api::ble_v2::GattCharacteristic::Permission;
using GattCharacteristic = nearby::api::ble_v2::GattCharacteristic;

// Short timeout for operation that we expect to timeout.
constexpr absl::Duration kFailureTimeout = absl::Milliseconds(100);
constexpr absl::string_view kModelId = "123456";
constexpr Uuid kFastPairServiceUuid(0x0000FE2C00001000, 0x800000805F9B34FB);
constexpr Uuid kKeyBasedCharacteristicUuidV1(0x0000123400001000,
                                             0x800000805F9B34FB);
constexpr Uuid kKeyBasedCharacteristicUuidV2(0xFE2C123483664814,
                                             0x8EB001DE32100BEA);
constexpr Uuid kPasskeyCharacteristicUuidV1(0x0000123500001000,
                                            0x800000805F9B34FB);
constexpr Uuid kPasskeyCharacteristicUuidV2(0xFE2C123583664814,
                                            0x8EB001DE32100BEA);
constexpr Uuid kAccountKeyCharacteristicUuidV1(0x0000123600001000,
                                               0x800000805F9B34FB);
constexpr Uuid kAccountKeyCharacteristicUuidV2(0xFE2C123683664814,
                                               0x8EB001DE32100BEA);

constexpr Uuid kModelIdCharacteristics(0xFE2C123383664814, 0x8EB001DE32100BEA);
class MediumEnvironmentStarter {
 public:
  MediumEnvironmentStarter() { MediumEnvironment::Instance().Start({}); }
  ~MediumEnvironmentStarter() { MediumEnvironment::Instance().Stop(); }
};

struct CharacteristicData {
  // Write result returned to the gatt client.
  absl::Status write_result =
      absl::PermissionDeniedError("can't write characteristic");
  std::string written_data;
  absl::StatusOr<std::string> read_result =
      absl::PermissionDeniedError("can't read characteristic");
  std::optional<std::string> notify_response;
};

class RobustGattClientTest : public testing::Test {
 protected:
  void SetUp() override { StartGattServer(); }

  void StartGattServer() {
    gatt_server_ =
        provider_ble_.StartGattServer(/*ServerGattConnectionCallback=*/{
            .on_characteristic_read_cb =
                [&](const api::ble_v2::BlePeripheral& remote_device,
                    const api::ble_v2::GattCharacteristic& characteristic,
                    int offset,
                    BleV2Medium::ServerGattConnectionCallback::ReadValueCallback
                        callback) {
                  MutexLock lock(&mutex_);
                  auto it = characteristics_.find(characteristic);
                  if (it == characteristics_.end()) {
                    callback(absl::NotFoundError("characteristic not found"));
                    return;
                  }
                  callback(it->second.read_result);
                },
            .on_characteristic_write_cb =
                [&](const api::ble_v2::BlePeripheral& remote_device,
                    const api::ble_v2::GattCharacteristic& characteristic,
                    int offset, absl::string_view data,
                    BleV2Medium::ServerGattConnectionCallback::
                        WriteValueCallback callback) {
                  MutexLock lock(&mutex_);
                  auto it = characteristics_.find(characteristic);
                  if (it == characteristics_.end()) {
                    callback(absl::NotFoundError("characteristic not found"));
                    return;
                  }
                  it->second.written_data = data;
                  if (it->second.write_result.code() ==
                      absl::StatusCode::kDeadlineExceeded) {
                    absl::SleepFor(kFailureTimeout);
                  }
                  callback(it->second.write_result);
                  if (it->second.notify_response.has_value()) {
                    auto ignored = gatt_server_->NotifyCharacteristicChanged(
                        characteristic, false,
                        ByteArray(*it->second.notify_response));
                  }
                },
        });
    provider_address_ = *gatt_server_->GetBlePeripheral().GetAddress();
  }

  void InsertCorrectV2GattCharacteristics() {
    MutexLock lock(&mutex_);
    key_based_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kKeyBasedCharacteristicUuidV2, permissions_,
        properties_);
    characteristics_[*key_based_characteristic_].write_result =
        absl::OkStatus();

    passkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kPasskeyCharacteristicUuidV2, permissions_,
        properties_);
    characteristics_[*passkey_characteristic_].write_result = absl::OkStatus();

    accountkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kAccountKeyCharacteristicUuidV2, permissions_,
        properties_);
    characteristics_[*accountkey_characteristic_].write_result =
        absl::OkStatus();
    model_id_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kModelIdCharacteristics, Permission::kRead,
        Property::kRead);
    characteristics_[*model_id_characteristic_].read_result = kModelId;
  }

  void InsertCorrectV1GattCharacteristics() {
    MutexLock lock(&mutex_);
    key_based_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kKeyBasedCharacteristicUuidV1, permissions_,
        properties_);
    characteristics_[*key_based_characteristic_].write_result =
        absl::OkStatus();

    passkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kPasskeyCharacteristicUuidV1, permissions_,
        properties_);
    characteristics_[*passkey_characteristic_].write_result = absl::OkStatus();

    accountkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kAccountKeyCharacteristicUuidV1, permissions_,
        properties_);
    characteristics_[*accountkey_characteristic_].write_result =
        absl::OkStatus();
  }

  absl::string_view GetWrittenData(GattCharacteristic characteristic) {
    MutexLock lock(&mutex_);
    CHECK(characteristics_.find(characteristic) != characteristics_.end());
    return characteristics_[characteristic].written_data;
  }

  void SetWriteResult(GattCharacteristic characteristic, absl::Status status) {
    MutexLock lock(&mutex_);
    CHECK(characteristics_.find(characteristic) != characteristics_.end());
    characteristics_[characteristic].write_result = status;
  }

  void SetNotifyResponse(GattCharacteristic characteristic,
                         absl::string_view response) {
    MutexLock lock(&mutex_);
    CHECK(characteristics_.find(characteristic) != characteristics_.end());
    characteristics_[characteristic].notify_response = response;
  }

  MediumEnvironmentStarter env_;
  Mutex mutex_;
  BluetoothAdapter provider_adapter_;
  BleV2Medium provider_ble_{provider_adapter_};
  BluetoothAdapter seeker_adapter_;
  BleV2Medium seeker_ble_{seeker_adapter_};
  std::unique_ptr<GattServer> gatt_server_;
  std::string provider_address_;
  absl::flat_hash_map<GattCharacteristic, CharacteristicData> characteristics_
      ABSL_GUARDED_BY(mutex_);
  Property properties_ = Property::kWrite | Property::kNotify;
  Permission permissions_ = Permission::kWrite;
  std::optional<GattCharacteristic> key_based_characteristic_;
  std::optional<GattCharacteristic> passkey_characteristic_;
  std::optional<GattCharacteristic> accountkey_characteristic_;
  std::optional<GattCharacteristic> model_id_characteristic_;
};

TEST_F(RobustGattClientTest, Constructor) {
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  RobustGattClient gatt_client(seeker_ble_, provider, {});
}

TEST_F(RobustGattClientTest, ConnectToProviderWithoutServiceFails) {
  CountDownLatch latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});
  params.connect_timeout = absl::Seconds(1);
  params.discovery_timeout = absl::Seconds(1);

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.WriteCharacteristic(
      0, "hello", api::ble_v2::GattClient::WriteType::kWithResponse,
      [&](absl::Status status) {
        EXPECT_THAT(status, StatusIs(absl::StatusCode::kDeadlineExceeded));
        latch.CountDown();
      });
  EXPECT_TRUE(latch.Await().Ok());
}

TEST_F(RobustGattClientTest, DiscoveryRetryWorks) {
  CountDownLatch latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.WriteCharacteristic(
      0, "hello", api::ble_v2::GattClient::WriteType::kWithResponse,
      [&](absl::Status status) {
        EXPECT_OK(status);
        latch.CountDown();
      });
  // The Gatt client is re-trying to discover the characteristics.
  InsertCorrectV2GattCharacteristics();

  EXPECT_TRUE(latch.Await().Ok());
}

TEST_F(RobustGattClientTest, SuccessfulWriteToPrimaryUuid) {
  constexpr absl::string_view kData = "hello";
  CountDownLatch latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV2GattCharacteristics();
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.WriteCharacteristic(
      0, kData, api::ble_v2::GattClient::WriteType::kWithResponse,
      [&](absl::Status status) {
        EXPECT_OK(status);
        latch.CountDown();
      });

  EXPECT_TRUE(latch.Await().Ok());
  EXPECT_EQ(GetWrittenData(*key_based_characteristic_), kData);
}

TEST_F(RobustGattClientTest, SuccessfulWriteToFallbackUuid) {
  constexpr absl::string_view kData = "hello";
  CountDownLatch latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV1GattCharacteristics();
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.WriteCharacteristic(
      0, kData, api::ble_v2::GattClient::WriteType::kWithResponse,
      [&](absl::Status status) {
        EXPECT_OK(status);
        latch.CountDown();
      });

  EXPECT_TRUE(latch.Await().Ok());
  EXPECT_EQ(GetWrittenData(*key_based_characteristic_), kData);
}

TEST_F(RobustGattClientTest, RejectedWrite) {
  constexpr absl::string_view kData = "hello";
  CountDownLatch latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV2GattCharacteristics();
  SetWriteResult(*key_based_characteristic_,
                 absl::UnauthenticatedError("write rejected"));
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});
  params.gatt_operation_timeout = kFailureTimeout;

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.WriteCharacteristic(
      0, kData, api::ble_v2::GattClient::WriteType::kWithResponse,
      [&](absl::Status status) {
        EXPECT_THAT(status, StatusIs(absl::StatusCode::kUnavailable));
        latch.CountDown();
      });

  EXPECT_TRUE(latch.Await().Ok());
}

TEST_F(RobustGattClientTest, SuccessfulCallRemoteFunctionToPrimaryUuid) {
  constexpr absl::string_view kRequest = "request";
  constexpr absl::string_view kResponse = "response";
  CountDownLatch latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV2GattCharacteristics();
  SetNotifyResponse(*key_based_characteristic_, kResponse);
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.CallRemoteFunction(
      0, kRequest, [&](absl::StatusOr<absl::string_view> response) {
        EXPECT_OK(response);
        EXPECT_EQ(*response, kResponse);
        latch.CountDown();
      });

  EXPECT_TRUE(latch.Await().Ok());
  EXPECT_EQ(GetWrittenData(*key_based_characteristic_), kRequest);
}

TEST_F(RobustGattClientTest, SuccessfulCallRemoteFunctionToFallbackUuid) {
  constexpr absl::string_view kRequest = "request";
  constexpr absl::string_view kResponse = "response";
  CountDownLatch latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV1GattCharacteristics();
  SetNotifyResponse(*key_based_characteristic_, kResponse);
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.CallRemoteFunction(
      0, kRequest, [&](absl::StatusOr<absl::string_view> response) {
        EXPECT_OK(response);
        EXPECT_EQ(*response, kResponse);
        latch.CountDown();
      });

  EXPECT_TRUE(latch.Await().Ok());
  EXPECT_EQ(GetWrittenData(*key_based_characteristic_), kRequest);
}

TEST_F(RobustGattClientTest, CallRemoteFunctionRejectedWrite) {
  constexpr absl::string_view kRequest = "request";
  CountDownLatch latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV2GattCharacteristics();
  SetWriteResult(*key_based_characteristic_,
                 absl::InternalError("write rejected"));
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});
  params.gatt_operation_timeout = kFailureTimeout;

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.CallRemoteFunction(
      0, kRequest, [&](absl::StatusOr<absl::string_view> response) {
        EXPECT_THAT(response.status(),
                    StatusIs(absl::StatusCode::kUnavailable));
        latch.CountDown();
      });

  EXPECT_TRUE(latch.Await().Ok());
}

TEST_F(RobustGattClientTest, CallRemoteFunctionNoResponseTimesOut) {
  constexpr absl::string_view kRequest = "request";
  CountDownLatch latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV2GattCharacteristics();
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});
  params.gatt_operation_timeout = kFailureTimeout;

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.CallRemoteFunction(
      0, kRequest, [&](absl::StatusOr<absl::string_view> response) {
        EXPECT_THAT(response.status(),
                    StatusIs(absl::StatusCode::kDeadlineExceeded));
        latch.CountDown();
      });

  EXPECT_TRUE(latch.Await().Ok());
}

TEST_F(RobustGattClientTest, WriteTimeout) {
  constexpr absl::string_view kData = "hello";
  CountDownLatch latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV2GattCharacteristics();
  SetWriteResult(*key_based_characteristic_,
                 absl::DeadlineExceededError("write time out"));
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});
  params.gatt_operation_timeout = kFailureTimeout;

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.WriteCharacteristic(
      0, kData, api::ble_v2::GattClient::WriteType::kWithResponse,
      [&](absl::Status status) {
        EXPECT_THAT(status, StatusIs(absl::StatusCode::kUnavailable));
        latch.CountDown();
      });

  EXPECT_TRUE(latch.Await().Ok());
}

TEST_F(RobustGattClientTest, ReconnectWorks) {
  constexpr absl::string_view kData = "hello";
  CountDownLatch latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV2GattCharacteristics();
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.WriteCharacteristic(
      0, "first", api::ble_v2::GattClient::WriteType::kWithResponse,
      [&](absl::Status status) {
        NEARBY_LOGS(INFO) << "First write completed with: " << status;
        EXPECT_OK(status);
        latch.CountDown();
      });
  EXPECT_TRUE(latch.Await().Ok());
  CountDownLatch write_after_reconnect(1);
  gatt_client.WriteCharacteristic(
      0, kData, api::ble_v2::GattClient::WriteType::kWithResponse,
      [&](absl::Status status) {
        NEARBY_LOGS(INFO) << "Write after reconnect completed with: " << status;
        EXPECT_OK(status);
        write_after_reconnect.CountDown();
      });
  gatt_server_->Stop();
  gatt_server_.reset();
  StartGattServer();
  InsertCorrectV2GattCharacteristics();
  EXPECT_TRUE(write_after_reconnect.Await().Ok());
  EXPECT_EQ(GetWrittenData(*key_based_characteristic_), kData);
}

TEST_F(RobustGattClientTest, SuccessfulSubscribeToPrimaryUuid) {
  constexpr absl::string_view kData = "hello";
  constexpr int kKeyBasedCharacteristicIndex = 0;
  CountDownLatch write_latch(1);
  CountDownLatch notify_latch(1);
  absl::StatusOr<std::string> notify_data;
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV2GattCharacteristics();
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.Subscribe(kKeyBasedCharacteristicIndex,
                        [&](absl::StatusOr<absl::string_view> data) {
                          notify_data = data;
                          notify_latch.CountDown();
                        });
  gatt_client.WriteCharacteristic(
      0, "", api::ble_v2::GattClient::WriteType::kWithResponse,
      [&](absl::Status status) { write_latch.CountDown(); });
  EXPECT_TRUE(write_latch.Await().Ok());
  EXPECT_OK(gatt_server_->NotifyCharacteristicChanged(
      *key_based_characteristic_,
      /*confirm=*/true, ByteArray(std::string(kData))));

  EXPECT_TRUE(notify_latch.Await().Ok());
  EXPECT_OK(notify_data);
  EXPECT_EQ(*notify_data, kData);
}

TEST_F(RobustGattClientTest, SuccessfulSubscribeToFallbackUuid) {
  constexpr absl::string_view kData = "hello";
  constexpr int kKeyBasedCharacteristicIndex = 0;
  CountDownLatch write_latch(1);
  CountDownLatch notify_latch(1);
  absl::StatusOr<std::string> notify_data;
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV1GattCharacteristics();
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.Subscribe(kKeyBasedCharacteristicIndex,
                        [&](absl::StatusOr<absl::string_view> data) {
                          notify_data = data;
                          notify_latch.CountDown();
                        });
  gatt_client.WriteCharacteristic(
      0, "", api::ble_v2::GattClient::WriteType::kWithResponse,
      [&](absl::Status status) { write_latch.CountDown(); });
  EXPECT_TRUE(write_latch.Await().Ok());
  EXPECT_OK(gatt_server_->NotifyCharacteristicChanged(
      *key_based_characteristic_,
      /*confirm=*/true, ByteArray(std::string(kData))));

  EXPECT_TRUE(notify_latch.Await().Ok());
  EXPECT_OK(notify_data);
  EXPECT_EQ(*notify_data, kData);
}

TEST_F(RobustGattClientTest, NoNotifyAfterUnsubscribe) {
  constexpr absl::string_view kData = "hello";
  constexpr int kKeyBasedCharacteristicIndex = 0;
  CountDownLatch write_latch(1);
  CountDownLatch notify_latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV2GattCharacteristics();
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});
  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.Subscribe(kKeyBasedCharacteristicIndex,
                        [&](absl::StatusOr<absl::string_view> data) {
                          NEARBY_LOGS(INFO)
                              << "Notified " << data.status() << ", " << *data;
                          notify_latch.CountDown();
                        });
  gatt_client.WriteCharacteristic(
      0, "", api::ble_v2::GattClient::WriteType::kWithResponse,
      [&](absl::Status status) { write_latch.CountDown(); });
  EXPECT_TRUE(write_latch.Await().Ok());

  gatt_client.Unsubscribe(kKeyBasedCharacteristicIndex);
  // Depending on the timing, the characteristic may still be subscribed on the
  // server. `NotifyCharacteristicChanged()` may be successful or may fail.
  // Neither is an error.
  auto ignored = gatt_server_->NotifyCharacteristicChanged(
      *key_based_characteristic_,
      /*confirm=*/true, ByteArray(std::string(kData)));

  EXPECT_FALSE(notify_latch.Await(kFailureTimeout).result());
}

TEST_F(RobustGattClientTest, SuccessfulRead) {
  CountDownLatch latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV2GattCharacteristics();
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});
  params.characteristic_uuids.push_back({kModelIdCharacteristics, Uuid()});
  constexpr int kModelIdIndex = 1;

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  gatt_client.ReadCharacteristic(kModelIdIndex,
                                 [&](absl::StatusOr<absl::string_view> value) {
                                   EXPECT_OK(value);
                                   EXPECT_EQ(*value, kModelId);
                                   latch.CountDown();
                                 });

  EXPECT_TRUE(latch.Await().Ok());
}

TEST_F(RobustGattClientTest, ReadFromWrongCharacteristicFails) {
  CountDownLatch latch(1);
  BleV2Peripheral provider = seeker_ble_.GetRemotePeripheral(provider_address_);
  InsertCorrectV2GattCharacteristics();
  RobustGattClient::ConnectionParams params;
  params.tx_power_level = api::ble_v2::TxPowerLevel::kMedium;
  params.service_uuid = kFastPairServiceUuid;
  params.characteristic_uuids.push_back(
      {kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1});
  params.characteristic_uuids.push_back({kModelIdCharacteristics, Uuid()});
  params.gatt_operation_timeout = kFailureTimeout;
  constexpr int kKeyBasedCharacteristicIndex = 0;

  RobustGattClient gatt_client(seeker_ble_, provider, params);
  // Key based pairing characteristic is write only, so read will fail.
  gatt_client.ReadCharacteristic(
      kKeyBasedCharacteristicIndex,
      [&](absl::StatusOr<absl::string_view> value) {
        EXPECT_THAT(value.status(), StatusIs(absl::StatusCode::kUnavailable));
        latch.CountDown();
      });

  EXPECT_TRUE(latch.Await().Ok());
}

}  // namespace

}  // namespace fastpair
}  // namespace nearby
