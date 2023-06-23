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

#include "fastpair/handshake/fast_pair_gatt_service_client_impl.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/common/protocol.h"
#include "fastpair/handshake/fake_fast_pair_data_encryptor.h"
#include "fastpair/handshake/fast_pair_gatt_service_client.h"
#include "fastpair/internal/mediums/mediums.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

namespace {
using Property = nearby::api::ble_v2::GattCharacteristic::Property;
using Permission = nearby::api::ble_v2::GattCharacteristic::Permission;
using GattCharacteristic = nearby::api::ble_v2::GattCharacteristic;
using WriteType = nearby::api::ble_v2::GattClient::WriteType;

constexpr absl::Duration kGattOperationTimeout = absl::Seconds(15);
constexpr absl::string_view kMetadataId("test_id");
constexpr absl::string_view kSeekerAddress("AA:BB:CC:DD:EE:00");
constexpr Uuid kFastPairServiceUuid(0x0000FE2C00001000, 0x800000805F9B34FB);
constexpr Uuid kKeyBasedCharacteristicUuidV2(0xFE2C123483664814,
                                             0x8EB001DE32100BEA);
constexpr Uuid kPasskeyCharacteristicUuidV2(0xFE2C123583664814,
                                            0x8EB001DE32100BEA);
constexpr Uuid kAccountKeyCharacteristicUuidV2(0xFE2C123683664814,
                                               0x8EB001DE32100BEA);
// Length of advertisement byte should be 16
constexpr absl::string_view kKeyBasedCharacteristicAdvertisementByte =
    "keyBasedCharacte";
constexpr absl::string_view kPasskeyharacteristicAdvertisementByte =
    "passkeyCharacter";
constexpr Uuid kWrongServiceId(0x0000FE2B00001000, 0x800000805F9B34FB);
constexpr uint8_t kMessageType = 0x00;
constexpr uint8_t kFlags = 0x00;

constexpr uint32_t kPasskey = 123456;

constexpr std::array<uint8_t, 64> kPublicKey = {
    0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F,
    0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3,
    0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01,
    0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45,
    0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32,
    0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};
}  //  namespace

class MediumEnvironmentStarter {
 public:
  MediumEnvironmentStarter() { MediumEnvironment::Instance().Start(); }
  ~MediumEnvironmentStarter() { MediumEnvironment::Instance().Stop(); }
};

struct CharacteristicData {
  // Write result returned to the gatt client.
  absl::Status write_result;
  std::optional<std::string> notify_response;
};

class FastPairGattServiceClientTest : public testing::Test {
 public:
  FastPairGattServiceClientTest() {
    fast_pair_data_encryptor_ = std::make_unique<FakeFastPairDataEncryptor>();
    fast_pair_data_encryptor_->SetPublickey(kPublicKey);
  }

  void SetUp() override {
    gatt_server_ =
        provider_ble_.StartGattServer(/*ServerGattConnectionCallback=*/{
            .on_characteristic_write_cb =
                [&](const api::ble_v2::BlePeripheral& remote_device,
                    const api::ble_v2::GattCharacteristic& characteristic,
                    int offset, absl::string_view data,
                    BleV2Medium::ServerGattConnectionCallback::
                        WriteValueCallback callback) {
                  auto it = characteristics_.find(characteristic);
                  if (it == characteristics_.end()) {
                    callback(absl::NotFoundError("characteristic not found"));
                    return;
                  }
                  callback(it->second.write_result);
                  if (it->second.notify_response.has_value()) {
                    NEARBY_LOGS(INFO) << "Notify seeker";
                    auto ignored = gatt_server_->NotifyCharacteristicChanged(
                        characteristic, false,
                        ByteArray(*it->second.notify_response));
                  }
                }});
    provider_address_ = *gatt_server_->GetBlePeripheral().GetAddress();
    device_ = std::make_unique<FastPairDevice>(
        kMetadataId, provider_address_, Protocol::kFastPairInitialPairing);
  }

  void TearDown() override {
    executor_.Shutdown();
    key_based_characteristic_ = std::nullopt;
    passkey_characteristic_ = std::nullopt;
    accountkey_characteristic_ = std::nullopt;
    characteristics_.clear();
    initalized_failure_ = std::nullopt;
    write_failure_ = std::nullopt;
    gatt_client_.reset();
    gatt_server_->Stop();
    gatt_server_.reset();
  }

  void InsertCorrectGattCharacteristics() {
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
  }

  void InsertCharacteristicsWithWrongServiceId() {
    key_based_characteristic_ = gatt_server_->CreateCharacteristic(
        kWrongServiceId, kKeyBasedCharacteristicUuidV2, permissions_,
        properties_);
    characteristics_[*key_based_characteristic_].write_result =
        absl::OkStatus();

    passkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kWrongServiceId, kPasskeyCharacteristicUuidV2, permissions_,
        properties_);
    characteristics_[*passkey_characteristic_].write_result = absl::OkStatus();

    accountkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kWrongServiceId, kAccountKeyCharacteristicUuidV2, permissions_,
        properties_);
    characteristics_[*accountkey_characteristic_].write_result =
        absl::OkStatus();
  }

  void InitializeFastPairGattServiceClient(bool short_timeouts = false) {
    CountDownLatch latch(1);
    gatt_client_ = FastPairGattServiceClientImpl::Factory::Create(
        *device_, mediums_, &executor_);
    if (short_timeouts) {
      FastPairGattServiceClientImpl* impl =
          dynamic_cast<FastPairGattServiceClientImpl*>(gatt_client_.get());
      auto& params = impl->GetConnectionParams();
      params.gatt_operation_timeout = absl::Seconds(1);
      params.gatt_operation_timeout = absl::Seconds(1);
      params.max_back_off = absl::Milliseconds(500);
    }
    gatt_client_->InitializeGattConnection(
        [&](std::optional<PairFailure> failure) {
          initalized_failure_ = failure;
          latch.CountDown();
        });
    latch.Await();
  }

  std::optional<PairFailure> GetInitializedCallbackResult() {
    return initalized_failure_;
  }

  void WriteTestCallback(std::optional<PairFailure> failure) {
    write_failure_ = failure;
  }

  std::optional<PairFailure> GetWriteCallbackResult() { return write_failure_; }

  void WriteRequestToKeyBased() {
    CountDownLatch latch(1);
    gatt_client_->WriteRequestAsync(
        kMessageType, kFlags, provider_address_, /* Seeker Address*/ "",
        *fast_pair_data_encryptor_,
        [&](absl::string_view response, std::optional<PairFailure> failure) {
          EXPECT_FALSE(response.empty());
          WriteTestCallback(failure);
          latch.CountDown();
        });
    latch.Await();
  }

  void WriteRequestToPasskey() {
    CountDownLatch latch(1);
    gatt_client_->WritePasskeyAsync(
        kSeekerPasskey, kPasskey, *fast_pair_data_encryptor_,
        [&](absl::string_view response, std::optional<PairFailure> failure) {
          EXPECT_FALSE(response.empty());
          WriteTestCallback(failure);
          latch.CountDown();
        });
    latch.Await();
  }

  void WriteRequestToAccountkey() {
    CountDownLatch latch(1);
    gatt_client_->WriteAccountKey(*fast_pair_data_encryptor_,
                                  [&](std::optional<AccountKey> account_key,
                                      std::optional<PairFailure> failure) {
                                    EXPECT_TRUE(account_key.has_value());
                                    WriteTestCallback(failure);
                                    latch.CountDown();
                                  });
    latch.Await();
  }

  void SetKeyBaseCharacteristicsWriteResultToFailure() {
    auto it = characteristics_.find(*key_based_characteristic_);
    it->second.write_result = absl::UnknownError("Failed to write account key");
  }

  void SetPasskeyCharacteristicsWriteResultToFailure() {
    auto it = characteristics_.find(*passkey_characteristic_);
    it->second.write_result = absl::UnknownError("Failed to write account key");
  }

  void SetAccountkeyCharacteristicsWriteResultToFailure() {
    auto it = characteristics_.find(*accountkey_characteristic_);
    it->second.write_result = absl::UnknownError("Failed to write account key");
  }

 protected:
  MediumEnvironmentStarter env_;
  SingleThreadExecutor executor_;
  BluetoothAdapter provider_adapter_;
  BleV2Medium provider_ble_{provider_adapter_};
  std::unique_ptr<GattClient> internal_gatt_client_;
  std::unique_ptr<FastPairGattServiceClient> gatt_client_;
  std::unique_ptr<GattServer> gatt_server_;
  std::string provider_address_;
  std::unique_ptr<FakeFastPairDataEncryptor> fast_pair_data_encryptor_;
  std::unique_ptr<FastPairDevice> device_;
  Mediums mediums_;
  std::optional<PairFailure> initalized_failure_ = PairFailure::kUnknown;
  std::optional<PairFailure> write_failure_ = PairFailure::kUnknown;
  Property properties_ = Property::kWrite | Property::kNotify;
  Permission permissions_ = Permission::kWrite;
  std::optional<GattCharacteristic> key_based_characteristic_;
  std::optional<GattCharacteristic> passkey_characteristic_;
  std::optional<GattCharacteristic> accountkey_characteristic_;
  absl::flat_hash_map<GattCharacteristic, CharacteristicData> characteristics_;
};

TEST_F(FastPairGattServiceClientTest, SuccessInitializeGattConnection) {
  EXPECT_EQ(GetInitializedCallbackResult(), PairFailure::kUnknown);
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
}

TEST_F(FastPairGattServiceClientTest, FailedDiscoverServiceAndCharacteristics) {
  EXPECT_EQ(GetInitializedCallbackResult(), PairFailure::kUnknown);
  InsertCharacteristicsWithWrongServiceId();
  InitializeFastPairGattServiceClient();
  EXPECT_EQ(GetInitializedCallbackResult(), PairFailure::kCreateGattConnection);
}

TEST_F(FastPairGattServiceClientTest, SuccessfulWriteKeyBaseCharacteristics) {
  EXPECT_EQ(GetWriteCallbackResult(), PairFailure::kUnknown);
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  characteristics_[*key_based_characteristic_].notify_response =
      std::string(kKeyBasedCharacteristicAdvertisementByte);
  WriteRequestToKeyBased();
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
}

TEST_F(FastPairGattServiceClientTest, SuccessfulWritePasskeyCharacteristics) {
  EXPECT_EQ(GetWriteCallbackResult(), PairFailure::kUnknown);
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();

  characteristics_[*passkey_characteristic_].notify_response =
      std::string(kPasskeyharacteristicAdvertisementByte);
  WriteRequestToPasskey();

  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
}

TEST_F(FastPairGattServiceClientTest,
       SuccessfulWriteAccountKeyCharacteristics) {
  EXPECT_EQ(GetWriteCallbackResult(), PairFailure::kUnknown);
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  WriteRequestToAccountkey();
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
}

TEST_F(FastPairGattServiceClientTest, FailedToWriteKeyBaseCharacteristics) {
  EXPECT_EQ(GetWriteCallbackResult(), PairFailure::kUnknown);
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  SetKeyBaseCharacteristicsWriteResultToFailure();
  CountDownLatch latch(1);
  gatt_client_->WriteRequestAsync(
      kMessageType, kFlags, provider_address_, kSeekerAddress,
      *fast_pair_data_encryptor_,
      [&](absl::string_view response, std::optional<PairFailure> failure) {
        WriteTestCallback(failure);
        latch.CountDown();
      });
  latch.Await();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kKeyBasedPairingCharacteristicWrite);
}

TEST_F(FastPairGattServiceClientTest, FailedToWritePasskeyCharacteristics) {
  EXPECT_EQ(GetWriteCallbackResult(), PairFailure::kUnknown);
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  SetPasskeyCharacteristicsWriteResultToFailure();
  CountDownLatch latch(1);
  gatt_client_->WritePasskeyAsync(
      kSeekerPasskey, kPasskey, *fast_pair_data_encryptor_,
      [&](absl::string_view response, std::optional<PairFailure> failure) {
        WriteTestCallback(failure);
        latch.CountDown();
      });
  latch.Await();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kPasskeyPairingCharacteristicWrite);
}

TEST_F(FastPairGattServiceClientTest, FailedToWriteAccountKey) {
  EXPECT_EQ(GetWriteCallbackResult(), PairFailure::kUnknown);
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  SetAccountkeyCharacteristicsWriteResultToFailure();
  CountDownLatch latch(1);
  gatt_client_->WriteAccountKey(*fast_pair_data_encryptor_,
                                [&](std::optional<AccountKey> account_key,
                                    std::optional<PairFailure> failure) {
                                  EXPECT_FALSE(account_key.has_value());
                                  WriteTestCallback(failure);
                                  latch.CountDown();
                                });
  latch.Await();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kAccountKeyCharacteristicWrite);
}

TEST_F(FastPairGattServiceClientTest, KeyBasedPairingResponseTimeout) {
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient(/*short_timeouts=*/true);
  CountDownLatch latch(1);

  // Seeker writes to Key based pairing characteristic but the provider does not
  // respond.
  gatt_client_->WriteRequestAsync(
      kMessageType, kFlags, provider_address_, kSeekerAddress,
      *fast_pair_data_encryptor_,
      [&](absl::string_view response, std::optional<PairFailure> failure) {
        EXPECT_EQ(failure, PairFailure::kKeyBasedPairingResponseTimeout);
        latch.CountDown();
      });
  latch.Await();
}

TEST_F(FastPairGattServiceClientTest, PasskeyResponseTimeout) {
  EXPECT_EQ(GetWriteCallbackResult(), PairFailure::kUnknown);
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  CountDownLatch latch(1);
  gatt_client_->WritePasskeyAsync(
      kSeekerPasskey, kPasskey, *fast_pair_data_encryptor_,
      [&](absl::string_view response, std::optional<PairFailure> failure) {
        WriteTestCallback(failure);
        latch.CountDown();
      });
  SystemClock::Sleep(kGattOperationTimeout);
  latch.Await();
  EXPECT_EQ(GetWriteCallbackResult(), PairFailure::kPasskeyResponseTimeout);
}

}  // namespace fastpair
}  // namespace nearby
