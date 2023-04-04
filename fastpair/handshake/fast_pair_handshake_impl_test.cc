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

#include "fastpair/handshake/fast_pair_handshake_impl.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/common/protocol.h"
#include "fastpair/handshake/fast_pair_gatt_service_client_impl.h"
#include "fastpair/server_access/fake_fast_pair_repository.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {

namespace {
using Property = nearby::api::ble_v2::GattCharacteristic::Property;
using Permission = nearby::api::ble_v2::GattCharacteristic::Permission;
using ::nearby::api::ble_v2::GattCharacteristic;

constexpr absl::string_view kMetadataId("718c17");
constexpr absl::string_view kProviderAddress("11:22:33:44:55:66");
constexpr absl::string_view kPublicAddress("5E:3F:45:61:C3:32");
constexpr absl::string_view kKeyBasedResponse("keybasedresponse");
constexpr absl::string_view kWrongResponse("wrongresponse");
constexpr absl::string_view kPublicAntiSpoof =
    "Wuyr48lD3txnUhGiMF1IfzlTwRxxe+wMB1HLzP+"
    "0wVcljfT3XPoiy1fntlneziyLD5knDVAJSE+RM/zlPRP/Jg==";
constexpr char kInvalidPublicAntiSpoof[] = "InvalidPublicAntiSpoof";
constexpr std::array<uint8_t, kAesBlockByteSize> kRawResponseBytes = {
    0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D,
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6};
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
constexpr absl::Duration kGattOperationTimeout = absl::Seconds(15);
}  // namespace

class FastPairHandshakeImplTest : public testing::Test {
 public:
  void SetUp() override {
    repository_ = std::make_unique<FakeFastPairRepository>();
    env_.Start({.use_simulated_clock = true});
    BluetoothAdapter adapter;
    BleV2Medium ble(adapter);
    gatt_server_ = ble.StartGattServer(/*ServerGattConnectionCallback=*/{});
  }

  void TearDown() override {
    repository_.reset();
    key_based_characteristic_ = std::nullopt;
    passkey_characteristic_ = std::nullopt;
    gatt_server_->Stop();
    gatt_server_.reset();
    handshake_.reset();
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

  void SetUpFastPairRepository() {
    proto::Device metadata;
    std::string decoded_key;
    absl::Base64Unescape(kPublicAntiSpoof, &decoded_key);
    metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
    repository_->SetFakeMetadata(kMetadataId, metadata);
  }

  void FailedFastPairRepository() {
    proto::Device metadata;
    std::string decoded_key;
    absl::Base64Unescape(kInvalidPublicAntiSpoof, &decoded_key);
    metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
    repository_->SetFakeMetadata(kMetadataId, metadata);
  }

  absl::Status TriggerKeyBasedGattChanged() {
    FastPairDataEncryptor* fast_pair_data_encryptor_ =
        handshake_->fast_pair_data_encryptor();
    std::array<uint8_t, kAesBlockByteSize> encryptedResponse =
        fast_pair_data_encryptor_->EncryptBytes(kRawResponseBytes);
    std::array<char, kAesBlockByteSize> response;
    std::copy(encryptedResponse.begin(), encryptedResponse.end(),
              response.begin());
    return gatt_server_->NotifyCharacteristicChanged(
        key_based_characteristic_.value(), false, ByteArray(response));
  }

  absl::Status TriggerKeyBasedGattChangedWithWrongResponse() {
    return gatt_server_->NotifyCharacteristicChanged(
        key_based_characteristic_.value(), false,
        ByteArray(std::string(kKeyBasedResponse)));
  }

  absl::Status TriggerKeyBasedGattChangedWithWrongSizeResponse() {
    return gatt_server_->NotifyCharacteristicChanged(
        key_based_characteristic_.value(), false,
        ByteArray(std::string(kWrongResponse)));
  }

 protected:
  std::unique_ptr<FastPairHandshake> handshake_;

 private:
  MediumEnvironment& env_{MediumEnvironment::Instance()};
  std::unique_ptr<GattServer> gatt_server_;
  std::optional<GattCharacteristic> key_based_characteristic_;
  std::optional<GattCharacteristic> passkey_characteristic_;
  std::unique_ptr<FakeFastPairRepository> repository_;
  Property properties_ = Property::kWrite | Property::kNotify;
  Permission permissions_ = Permission::kWrite;
};

TEST_F(FastPairHandshakeImplTest, Success) {
  SetUpFastPairRepository();
  InsertCorrectGattCharacteristics();
  FastPairDevice device(kMetadataId, kProviderAddress,
                        Protocol::kFastPairInitialPairing);
  CountDownLatch latch(1);
  handshake_ = std::make_unique<FastPairHandshakeImpl>(
      device,
      [&](FastPairDevice& callback_device, std::optional<PairFailure> failure) {
        EXPECT_EQ(&device, &callback_device);
        EXPECT_EQ(device.public_address(), kPublicAddress);
        EXPECT_FALSE(failure.has_value());
        latch.CountDown();
      });
  EXPECT_OK(TriggerKeyBasedGattChanged());
  latch.Await();
  EXPECT_TRUE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, GattError) {
  SetUpFastPairRepository();
  FastPairDevice device(kMetadataId, kProviderAddress,
                        Protocol::kFastPairInitialPairing);
  CountDownLatch latch(1);
  handshake_ = std::make_unique<FastPairHandshakeImpl>(
      device,
      [&](FastPairDevice& callback_device, std::optional<PairFailure> failure) {
        EXPECT_EQ(&device, &callback_device);
        EXPECT_EQ(failure.value(), PairFailure::kCreateGattConnection);
        latch.CountDown();
      });
  latch.Await();
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, DataEncryptorCreateError) {
  FailedFastPairRepository();
  InsertCorrectGattCharacteristics();
  FastPairDevice device(kMetadataId, kProviderAddress,
                        Protocol::kFastPairInitialPairing);
  CountDownLatch latch(1);
  handshake_ = std::make_unique<FastPairHandshakeImpl>(
      device,
      [&](FastPairDevice& callback_device, std::optional<PairFailure> failure) {
        EXPECT_EQ(&device, &callback_device);
        EXPECT_EQ(failure.value(), PairFailure::kDataEncryptorRetrieval);
        latch.CountDown();
      });
  latch.Await();
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, WriteResponseError) {
  SetUpFastPairRepository();
  InsertCorrectGattCharacteristics();
  FastPairDevice device(kMetadataId, kProviderAddress,
                        Protocol::kFastPairInitialPairing);
  CountDownLatch latch(1);
  handshake_ = std::make_unique<FastPairHandshakeImpl>(
      device,
      [&](FastPairDevice& callback_device, std::optional<PairFailure> failure) {
        EXPECT_EQ(&device, &callback_device);
        EXPECT_EQ(failure.value(),
                  PairFailure::kKeyBasedPairingResponseTimeout);
        latch.CountDown();
      });
  SystemClock::Sleep(kGattOperationTimeout);
  latch.Await();
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, WriteResponseWrongSize) {
  SetUpFastPairRepository();
  InsertCorrectGattCharacteristics();
  FastPairDevice device(kMetadataId, kProviderAddress,
                        Protocol::kFastPairInitialPairing);
  CountDownLatch latch(1);
  handshake_ = std::make_unique<FastPairHandshakeImpl>(
      device,
      [&](FastPairDevice& callback_device, std::optional<PairFailure> failure) {
        EXPECT_EQ(&device, &callback_device);
        EXPECT_EQ(failure.value(),
                  PairFailure::kKeybasedPairingResponseDecryptFailure);
        latch.CountDown();
      });
  EXPECT_OK(TriggerKeyBasedGattChangedWithWrongSizeResponse());
  latch.Await();
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, ParseResponseError) {
  SetUpFastPairRepository();
  InsertCorrectGattCharacteristics();
  FastPairDevice device(kMetadataId, kProviderAddress,
                        Protocol::kFastPairInitialPairing);
  CountDownLatch latch(1);
  handshake_ = std::make_unique<FastPairHandshakeImpl>(
      device,
      [&](FastPairDevice& callback_device, std::optional<PairFailure> failure) {
        EXPECT_EQ(&device, &callback_device);
        EXPECT_EQ(failure.value(),
                  PairFailure::kKeybasedPairingResponseDecryptFailure);
        latch.CountDown();
      });
  EXPECT_OK(TriggerKeyBasedGattChangedWithWrongResponse());
  latch.Await();
  EXPECT_FALSE(handshake_->completed_successfully());
}
}  // namespace fastpair
}  // namespace nearby
