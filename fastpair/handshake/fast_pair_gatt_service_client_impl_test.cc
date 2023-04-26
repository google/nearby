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
#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {

namespace {
using Property = nearby::api::ble_v2::GattCharacteristic::Property;
using Permission = nearby::api::ble_v2::GattCharacteristic::Permission;
using GattCharacteristic = nearby::api::ble_v2::GattCharacteristic;
using WriteType = nearby::api::ble_v2::GattClient::WriteType;

constexpr absl::Duration kGattOperationTimeout = absl::Seconds(15);
constexpr absl::string_view kMetadataId("test_id");
constexpr absl::string_view kProviderAddress("11:22:33:44:55:66");
constexpr absl::string_view kSeekerAddress("AA:BB:CC:DD:EE:00");
constexpr Uuid kFastPairServiceUuid(0x0000FE2C00001000, 0x800000805F9B34FB);
constexpr Uuid kKeyBasedCharacteristicUuidV2(0xFE2C123483664814,
                                             0x8EB001DE32100BEA);
constexpr Uuid kPasskeyCharacteristicUuidV2(0xFE2C123583664814,
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

class FastPairGattServiceClientTest : public testing::Test {
 public:
  FastPairGattServiceClientTest() {
    fast_pair_data_encryptor_ = std::make_unique<FakeFastPairDataEncryptor>();
    fast_pair_data_encryptor_->SetPublickey(kPublicKey);
  }

  void SetUp() override {
    env_.Start({.use_simulated_clock = true});
    BluetoothAdapter adapter;
    BleV2Medium ble(adapter);
    gatt_server_ = ble.StartGattServer(/*ServerGattConnectionCallback=*/{});
  }

  void TearDown() override {
    key_based_characteristic_ = std::nullopt;
    passkey_characteristic_ = std::nullopt;
    initalized_failure_ = std::nullopt;
    write_failure_ = std::nullopt;
    gatt_client_.reset();
    gatt_server_->Stop();
    gatt_server_.reset();
    env_.Stop();
  }

  void InsertCorrectGattCharacteristics() {
    key_based_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kKeyBasedCharacteristicUuidV2, permissions_,
        properties_);
    gatt_server_->UpdateCharacteristic(
        key_based_characteristic_.value(),
        ByteArray(std::string(kKeyBasedCharacteristicAdvertisementByte)));

    passkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kPasskeyCharacteristicUuidV2, permissions_,
        properties_);
    gatt_server_->UpdateCharacteristic(
        passkey_characteristic_.value(),
        ByteArray(std::string(kPasskeyharacteristicAdvertisementByte)));
  }

  void InsertCharacteristicsWithWrongServiceId() {
    key_based_characteristic_ = gatt_server_->CreateCharacteristic(
        kWrongServiceId, kKeyBasedCharacteristicUuidV2, permissions_,
        properties_);
    gatt_server_->UpdateCharacteristic(
        key_based_characteristic_.value(),
        ByteArray(std::string(kKeyBasedCharacteristicAdvertisementByte)));

    passkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kWrongServiceId, kPasskeyCharacteristicUuidV2, permissions_,
        properties_);
    gatt_server_->UpdateCharacteristic(
        passkey_characteristic_.value(),
        ByteArray(std::string(kPasskeyharacteristicAdvertisementByte)));
  }

  void InsertKeyBasedGattCharacteristicsWithEmptyValue() {
    key_based_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kKeyBasedCharacteristicUuidV2, permissions_,
        properties_);
    gatt_server_->UpdateCharacteristic(key_based_characteristic_.value(),
                                       ByteArray(""));

    passkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kPasskeyCharacteristicUuidV2, permissions_,
        properties_);
    gatt_server_->UpdateCharacteristic(
        passkey_characteristic_.value(),
        ByteArray(std::string(kPasskeyharacteristicAdvertisementByte)));
  }

  void InsertPasskeyGattCharacteristicsWithEmptyValue() {
    key_based_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kKeyBasedCharacteristicUuidV2, permissions_,
        properties_);
    gatt_server_->UpdateCharacteristic(
        key_based_characteristic_.value(),
        ByteArray(std::string(kKeyBasedCharacteristicAdvertisementByte)));

    passkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kPasskeyCharacteristicUuidV2, permissions_,
        properties_);
    gatt_server_->UpdateCharacteristic(passkey_characteristic_.value(),
                                       ByteArray(""));
  }

  void InitializeFastPairGattServiceClient() {
    FastPairDevice device(kMetadataId, kProviderAddress,
                          Protocol::kFastPairInitialPairing);
    gatt_client_ = FastPairGattServiceClientImpl::Factory::Create(device);
    gatt_client_->InitializeGattConnection(
        [this](std::optional<PairFailure> failure) {
          initalized_failure_ = failure;
        });
  }

  void RemoveDiscoveredKeyBasedCharacteristic() {
    env_.EraseBleV2MediumGattCharacteristicsForDiscovery(
        key_based_characteristic_.value());
  }

  void RemoveDiscoveredPasskeyCharacteristic() {
    env_.EraseBleV2MediumGattCharacteristicsForDiscovery(
        passkey_characteristic_.value());
  }

  bool UnsubceibeKeyBasedCharacteristic() {
    return env_.SetBleV2MediumGattCharacteristicSubscription(
        key_based_characteristic_.value(), false, {});
  }

  bool UnsubceibePasskeyCharacteristic() {
    return env_.SetBleV2MediumGattCharacteristicSubscription(
        passkey_characteristic_.value(), false, {});
  }

  std::optional<PairFailure> GetInitializedCallbackResult() {
    return initalized_failure_;
  }

  void WriteTestCallback(absl::string_view response,
                         std::optional<PairFailure> failure) {
    write_failure_ = failure;
  }

  std::optional<PairFailure> GetWriteCallbackResult() { return write_failure_; }

  void WriteRequestToKeyBased() {
    gatt_client_->WriteRequestAsync(
        kMessageType, kFlags, kProviderAddress, /* Seeker Address*/ "",
        *fast_pair_data_encryptor_,
        [&](absl::string_view response, std::optional<PairFailure> failure) {
          WriteTestCallback(response, failure);
        });
  }

  void WriteRequestToPasskey() {
    gatt_client_->WritePasskeyAsync(
        kSeekerPasskey, kPasskey, *fast_pair_data_encryptor_,
        [this](absl::string_view response, std::optional<PairFailure> failure) {
          WriteTestCallback(response, failure);
        });
  }

  absl::Status TriggerKeyBasedGattChanged() {
    return gatt_server_->NotifyCharacteristicChanged(
        key_based_characteristic_.value(), false,
        ByteArray(std::string(kKeyBasedCharacteristicAdvertisementByte)));
  }

  absl::Status TriggerPasskeyGattChanged() {
    return gatt_server_->NotifyCharacteristicChanged(
        passkey_characteristic_.value(), false,
        ByteArray(std::string(kPasskeyharacteristicAdvertisementByte)));
  }

 protected:
  MediumEnvironment& env_{MediumEnvironment::Instance()};
  std::unique_ptr<GattClient> internal_gatt_client_;
  std::unique_ptr<FastPairGattServiceClient> gatt_client_;
  std::unique_ptr<GattServer> gatt_server_;
  std::unique_ptr<FakeFastPairDataEncryptor> fast_pair_data_encryptor_;

 private:
  std::optional<GattCharacteristic> key_based_characteristic_;
  std::optional<GattCharacteristic> passkey_characteristic_;
  std::optional<PairFailure> initalized_failure_;
  std::optional<PairFailure> write_failure_;
  Property properties_ = Property::kWrite | Property::kNotify;
  Permission permissions_ = Permission::kWrite;
};

TEST_F(FastPairGattServiceClientTest, SuccessInitializeGattConnection) {
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  EXPECT_EQ(GetInitializedCallbackResult(), std::nullopt);
}

TEST_F(FastPairGattServiceClientTest, FailedDiscoverServiceAndCharacteristics) {
  InsertCharacteristicsWithWrongServiceId();
  InitializeFastPairGattServiceClient();
  EXPECT_EQ(GetInitializedCallbackResult(), PairFailure::kCreateGattConnection);
}

TEST_F(FastPairGattServiceClientTest, FailedGetKeyBasedCharacteristics) {
  InsertKeyBasedGattCharacteristicsWithEmptyValue();
  InitializeFastPairGattServiceClient();
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kKeyBasedPairingCharacteristicDiscovery);
}

TEST_F(FastPairGattServiceClientTest, FailedToGetPasskeyCharacteristics) {
  InsertPasskeyGattCharacteristicsWithEmptyValue();
  InitializeFastPairGattServiceClient();
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kPasskeyCharacteristicDiscovery);
}

TEST_F(FastPairGattServiceClientTest, SuccessfulWriteKeyBaseCharacteristics) {
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  WriteRequestToKeyBased();
  EXPECT_EQ(TriggerKeyBasedGattChanged(), absl::OkStatus());
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
}

TEST_F(FastPairGattServiceClientTest, SuccessfulWritePasskeyCharacteristics) {
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  WriteRequestToPasskey();
  EXPECT_EQ(TriggerPasskeyGattChanged(), absl::OkStatus());
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
}

TEST_F(FastPairGattServiceClientTest, FailedSubscribeKeybaseCharacteristic) {
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  RemoveDiscoveredKeyBasedCharacteristic();
  WriteRequestToKeyBased();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kKeyBasedPairingCharacteristicSubscription);
  WriteRequestToPasskey();
  EXPECT_EQ(TriggerPasskeyGattChanged(), absl::OkStatus());
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
}

TEST_F(FastPairGattServiceClientTest, FailedSubscribePasskeyCharacteristic) {
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  RemoveDiscoveredPasskeyCharacteristic();
  WriteRequestToKeyBased();
  EXPECT_EQ(TriggerKeyBasedGattChanged(), absl::OkStatus());
  EXPECT_EQ(GetWriteCallbackResult(), std::nullopt);
  WriteRequestToPasskey();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kPasskeyCharacteristicSubscription);
}

TEST_F(FastPairGattServiceClientTest, KeyBasedPairingResponseTimeout) {
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  CountDownLatch latch(1);
  gatt_client_->WriteRequestAsync(
      kMessageType, kFlags, kProviderAddress, kSeekerAddress,
      *fast_pair_data_encryptor_,
      [&](absl::string_view response, std::optional<PairFailure> failure) {
        WriteTestCallback(response, failure);
        latch.CountDown();
      });
  SystemClock::Sleep(kGattOperationTimeout);
  latch.Await();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kKeyBasedPairingResponseTimeout);
}

TEST_F(FastPairGattServiceClientTest, PasskeyResponseTimeout) {
  InsertCorrectGattCharacteristics();
  InitializeFastPairGattServiceClient();
  CountDownLatch latch(1);
  gatt_client_->WritePasskeyAsync(
      kSeekerPasskey, kPasskey, *fast_pair_data_encryptor_,
      [&](absl::string_view response, std::optional<PairFailure> failure) {
        WriteTestCallback(response, failure);
        latch.CountDown();
      });
  SystemClock::Sleep(kGattOperationTimeout);
  latch.Await();
  EXPECT_EQ(GetWriteCallbackResult(), PairFailure::kPasskeyResponseTimeout);
}
}  // namespace fastpair
}  // namespace nearby
