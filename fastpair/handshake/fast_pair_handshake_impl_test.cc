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
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/common/protocol.h"
#include "fastpair/handshake/fast_pair_gatt_service_client_impl.h"
#include "fastpair/server_access/fake_fast_pair_repository.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

namespace {
using Property = nearby::api::ble_v2::GattCharacteristic::Property;
using Permission = nearby::api::ble_v2::GattCharacteristic::Permission;
using ::nearby::api::ble_v2::GattCharacteristic;

constexpr absl::string_view kMetadataId("718c17");
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
constexpr Uuid kAccountKeyCharacteristicUuidV2(0xFE2C123683664814,
                                               0x8EB001DE32100BEA);
// Length of advertisement byte should be 16
constexpr absl::string_view kKeyBasedCharacteristicAdvertisementByte =
    "keyBasedCharacte";
constexpr absl::string_view kPasskeyharacteristicAdvertisementByte =
    "passkeyCharacter";
}  // namespace

class MediumEnvironmentStarter {
 public:
  MediumEnvironmentStarter() { MediumEnvironment::Instance().Start(); }
  ~MediumEnvironmentStarter() { MediumEnvironment::Instance().Stop(); }
};

struct CharacteristicData {
  // Write result returned to the gatt client.
  absl::Status write_result;
};

class FastPairHandshakeImplTest : public testing::Test {
 public:
  void TearDown() override {
    executor_.Shutdown();
    repository_.reset();
    key_based_characteristic_ = std::nullopt;
    passkey_characteristic_ = std::nullopt;
    gatt_server_->Stop();
    gatt_server_.reset();
    handshake_.reset();
  }

  void StartGattServer(
      absl::AnyInvocable<void()> trigger_keybase_value_change) {
    gatt_server_ =
        provider_ble_.StartGattServer(/*ServerGattConnectionCallback=*/{
            .on_characteristic_write_cb =
                [&, trigger_keybase_value_change =
                        std::move(trigger_keybase_value_change)](
                    const api::ble_v2::BlePeripheral& remote_device,
                    const api::ble_v2::GattCharacteristic& characteristic,
                    int offset, absl::string_view data,
                    BleV2Medium::ServerGattConnectionCallback::
                        WriteValueCallback callback) mutable {
                  auto it = characteristics_.find(characteristic);
                  if (it == characteristics_.end()) {
                    callback(absl::NotFoundError("characteristic not found"));
                    return;
                  }
                  callback(it->second.write_result);
                  if (it->second.write_result.ok() &&
                      characteristic == *key_based_characteristic_) {
                    trigger_keybase_value_change();
                  }
                }});
    provider_address_ = *gatt_server_->GetBlePeripheral().GetAddress();
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

  void SetUpFastPairRepository() {
    repository_ = std::make_unique<FakeFastPairRepository>();
    proto::Device metadata;
    std::string decoded_key;
    absl::Base64Unescape(kPublicAntiSpoof, &decoded_key);
    metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
    repository_->SetFakeMetadata(kMetadataId, metadata);
  }

  void FailedFastPairRepository() {
    repository_ = std::make_unique<FakeFastPairRepository>();
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
  MediumEnvironmentStarter env_;
  SingleThreadExecutor executor_;
  std::unique_ptr<FastPairHandshake> handshake_;
  BluetoothAdapter provider_adapter_;
  BleV2Medium provider_ble_{provider_adapter_};
  std::string provider_address_;
  Mediums mediums_;
  std::unique_ptr<FastPairDevice> fast_pair_device_;

 private:
  absl::flat_hash_map<GattCharacteristic, CharacteristicData> characteristics_;
  std::unique_ptr<GattServer> gatt_server_;
  std::optional<GattCharacteristic> key_based_characteristic_;
  std::optional<GattCharacteristic> passkey_characteristic_;
  std::optional<GattCharacteristic> accountkey_characteristic_;
  std::unique_ptr<FakeFastPairRepository> repository_;
  Property properties_ = Property::kWrite | Property::kNotify;
  Permission permissions_ = Permission::kWrite;
};

TEST_F(FastPairHandshakeImplTest, Success) {
  bool notified = false;
  StartGattServer([&]() {
    notified = true;
    EXPECT_OK(TriggerKeyBasedGattChanged());
  });
  SetUpFastPairRepository();
  InsertCorrectGattCharacteristics();
  fast_pair_device_ = std::make_unique<FastPairDevice>(
      kMetadataId, provider_address_, Protocol::kFastPairInitialPairing);
  CountDownLatch latch(1);
  handshake_ = std::make_unique<FastPairHandshakeImpl>(
      *fast_pair_device_, mediums_,
      [&](FastPairDevice& callback_device, std::optional<PairFailure> failure) {
        EXPECT_EQ(fast_pair_device_.get(), &callback_device);
        EXPECT_EQ(fast_pair_device_->GetPublicAddress(), kPublicAddress);
        EXPECT_FALSE(failure.has_value());
        latch.CountDown();
      },
      &executor_);
  latch.Await();
  EXPECT_TRUE(notified);
  EXPECT_TRUE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, GattError) {
  bool notified = false;
  StartGattServer([&]() {
    notified = true;
    EXPECT_OK(TriggerKeyBasedGattChanged());
  });
  SetUpFastPairRepository();
  fast_pair_device_ = std::make_unique<FastPairDevice>(
      kMetadataId, provider_address_, Protocol::kFastPairInitialPairing);
  CountDownLatch latch(1);
  handshake_ = std::make_unique<FastPairHandshakeImpl>(
      *fast_pair_device_, mediums_,
      [&](FastPairDevice& callback_device, std::optional<PairFailure> failure) {
        EXPECT_EQ(fast_pair_device_.get(), &callback_device);
        EXPECT_EQ(failure.value(), PairFailure::kCreateGattConnection);
        latch.CountDown();
      },
      &executor_);
  latch.Await();
  EXPECT_FALSE(notified);
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, DataEncryptorCreateError) {
  bool notified = false;
  StartGattServer([&]() {
    notified = true;
    EXPECT_OK(TriggerKeyBasedGattChanged());
  });
  FailedFastPairRepository();
  InsertCorrectGattCharacteristics();
  fast_pair_device_ = std::make_unique<FastPairDevice>(
      kMetadataId, provider_address_, Protocol::kFastPairInitialPairing);
  CountDownLatch latch(1);
  handshake_ = std::make_unique<FastPairHandshakeImpl>(
      *fast_pair_device_, mediums_,
      [&](FastPairDevice& callback_device, std::optional<PairFailure> failure) {
        EXPECT_EQ(fast_pair_device_.get(), &callback_device);
        EXPECT_EQ(failure.value(), PairFailure::kDataEncryptorRetrieval);
        latch.CountDown();
      },
      &executor_);
  latch.Await();
  EXPECT_FALSE(notified);
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, WriteResponseError) {
  StartGattServer([]() {});
  SetUpFastPairRepository();
  InsertCorrectGattCharacteristics();
  fast_pair_device_ = std::make_unique<FastPairDevice>(
      kMetadataId, provider_address_, Protocol::kFastPairInitialPairing);
  CountDownLatch latch(1);
  handshake_ = std::make_unique<FastPairHandshakeImpl>(
      *fast_pair_device_, mediums_,
      [&](FastPairDevice& callback_device, std::optional<PairFailure> failure) {
        EXPECT_EQ(fast_pair_device_.get(), &callback_device);
        EXPECT_EQ(failure.value(),
                  PairFailure::kKeyBasedPairingResponseTimeout);
        latch.CountDown();
      },
      &executor_);
  latch.Await();
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, WriteResponseWrongSize) {
  bool notified = false;
  StartGattServer([&]() {
    notified = true;
    EXPECT_OK(TriggerKeyBasedGattChangedWithWrongSizeResponse());
  });
  SetUpFastPairRepository();
  InsertCorrectGattCharacteristics();
  fast_pair_device_ = std::make_unique<FastPairDevice>(
      kMetadataId, provider_address_, Protocol::kFastPairInitialPairing);
  CountDownLatch latch(1);
  handshake_ = std::make_unique<FastPairHandshakeImpl>(
      *fast_pair_device_, mediums_,
      [&](FastPairDevice& callback_device, std::optional<PairFailure> failure) {
        EXPECT_EQ(fast_pair_device_.get(), &callback_device);
        EXPECT_EQ(failure.value(),
                  PairFailure::kKeybasedPairingResponseDecryptFailure);
        latch.CountDown();
      },
      &executor_);
  latch.Await();
  EXPECT_TRUE(notified);
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, ParseResponseError) {
  bool notified = false;
  StartGattServer([&]() {
    notified = true;
    EXPECT_OK(TriggerKeyBasedGattChangedWithWrongResponse());
  });
  SetUpFastPairRepository();
  InsertCorrectGattCharacteristics();
  fast_pair_device_ = std::make_unique<FastPairDevice>(
      kMetadataId, provider_address_, Protocol::kFastPairInitialPairing);
  CountDownLatch latch(1);
  handshake_ = std::make_unique<FastPairHandshakeImpl>(
      *fast_pair_device_, mediums_,
      [&](FastPairDevice& callback_device, std::optional<PairFailure> failure) {
        EXPECT_EQ(fast_pair_device_.get(), &callback_device);
        EXPECT_EQ(failure.value(),
                  PairFailure::kKeybasedPairingResponseDecryptFailure);
        latch.CountDown();
      },
      &executor_);
  latch.Await();
  EXPECT_TRUE(notified);
  EXPECT_FALSE(handshake_->completed_successfully());
}
}  // namespace fastpair
}  // namespace nearby
