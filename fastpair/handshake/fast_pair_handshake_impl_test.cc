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

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/common/protocol.h"
#include "fastpair/crypto/decrypted_response.h"
#include "fastpair/handshake/fake_fast_pair_data_encryptor.h"
#include "fastpair/handshake/fast_pair_data_encryptor.h"
#include "fastpair/handshake/fast_pair_data_encryptor_impl.h"
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
constexpr Uuid kFastPairServiceUuid(0x0000FE2C00001000, 0x800000805F9B34FB);
constexpr Uuid kKeyBasedCharacteristicUuidV2(0xFE2C123483664814,
                                             0x8EB001DE32100BEA);
constexpr Uuid kPasskeyCharacteristicUuidV2(0xFE2C123583664814,
                                            0x8EB001DE32100BEA);
constexpr Uuid kAccountKeyCharacteristicUuidV2(0xFE2C123683664814,
                                               0x8EB001DE32100BEA);
constexpr std::array<uint8_t, 6> address_bytes = {0x5E, 0x3F, 0x45,
                                                  0x61, 0xC3, 0x32};

constexpr std::array<uint8_t, 9> salt = {0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                         0x0D, 0x0E, 0x0F, 0x00};
}  // namespace

class MediumEnvironmentStarter {
 public:
  MediumEnvironmentStarter() { MediumEnvironment::Instance().Start(); }
  ~MediumEnvironmentStarter() { MediumEnvironment::Instance().Stop(); }
};

class FastPairFakeDataEncryptorImplFactory
    : public FastPairDataEncryptorImpl::Factory {
 public:
  void CreateInstance(
      const FastPairDevice& device,
      absl::AnyInvocable<void(std::unique_ptr<FastPairDataEncryptor>)>
          on_get_instance_callback) override {
    if (!successful_retrieval_) {
      std::move(on_get_instance_callback)(nullptr);
      return;
    }

    auto data_encryptor = std::make_unique<FakeFastPairDataEncryptor>();
    data_encryptor_ = data_encryptor.get();
    data_encryptor->SetResponse(response_);
    std::move(on_get_instance_callback)(std::move(data_encryptor));
  }

  FakeFastPairDataEncryptor* data_encryptor() { return data_encryptor_; }

  void SetFailedRetrieval() { successful_retrieval_ = false; }

  void SetResponse(std::optional<DecryptedResponse> response) {
    response_ = std::move(response);
  }

 private:
  FakeFastPairDataEncryptor* data_encryptor_ = nullptr;
  bool successful_retrieval_ = true;
  std::optional<DecryptedResponse> response_;
};

struct CharacteristicData {
  // Write result returned to the gatt client.
  absl::Status write_result;
  std::optional<std::string> notify_response;
};

class FastPairHandshakeImplTest : public testing::Test {
 public:
  FastPairHandshakeImplTest() {
    FastPairDataEncryptorImpl::Factory::SetFactoryForTesting(
        &fake_data_encryptor_factory_);
  }

  void TearDown() override {
    executor_.Shutdown();
    key_based_characteristic_ = std::nullopt;
    passkey_characteristic_ = std::nullopt;
    handshake_.reset();
    gatt_server_->Stop();
    gatt_server_.reset();
  }

  void StartGattServer() {
    gatt_server_ =
        provider_ble_.StartGattServer(/*ServerGattConnectionCallback=*/{
            .on_characteristic_write_cb =
                [&](const api::ble_v2::BlePeripheral& remote_device,
                    const api::ble_v2::GattCharacteristic& characteristic,
                    int offset, absl::string_view data,
                    BleV2Medium::ServerGattConnectionCallback::
                        WriteValueCallback callback) mutable {
                  MutexLock lock(&mutex_);
                  auto it = characteristics_.find(characteristic);
                  if (it == characteristics_.end()) {
                    callback(absl::NotFoundError("characteristic not found"));
                    return;
                  }
                  callback(it->second.write_result);
                  if (it->second.notify_response.has_value()) {
                    auto ignored = gatt_server_->NotifyCharacteristicChanged(
                        characteristic, false,
                        ByteArray(*it->second.notify_response));
                  }
                }});
    provider_address_ = *gatt_server_->GetBlePeripheral().GetAddress();
  }

  void InsertCorrectGattCharacteristics() {
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
  }

  void SetNotifyResponse(GattCharacteristic characteristic,
                         absl::string_view response) {
    MutexLock lock(&mutex_);
    CHECK(characteristics_.find(characteristic) != characteristics_.end());
    characteristics_[characteristic].notify_response = response;
  }

  void SetDecryptedResponse() {
    DecryptedResponse decrypted_response(
        FastPairMessageType::kKeyBasedPairingResponse, address_bytes, salt);
    fake_data_encryptor_factory_.SetResponse(std::move(decrypted_response));
  }

 protected:
  MediumEnvironmentStarter env_;
  Mutex mutex_;
  SingleThreadExecutor executor_;
  std::unique_ptr<FastPairHandshake> handshake_;
  BluetoothAdapter provider_adapter_;
  BleV2Medium provider_ble_{provider_adapter_};
  std::string provider_address_;
  Mediums mediums_;
  std::unique_ptr<FastPairDevice> fast_pair_device_;
  absl::flat_hash_map<GattCharacteristic, CharacteristicData> characteristics_
      ABSL_GUARDED_BY(mutex_);
  std::optional<GattCharacteristic> key_based_characteristic_;
  std::optional<GattCharacteristic> passkey_characteristic_;
  std::optional<GattCharacteristic> accountkey_characteristic_;
  FastPairFakeDataEncryptorImplFactory fake_data_encryptor_factory_;

 private:
  std::unique_ptr<GattServer> gatt_server_;
  Property properties_ = Property::kWrite | Property::kNotify;
  Permission permissions_ = Permission::kWrite;
};

TEST_F(FastPairHandshakeImplTest, Success) {
  StartGattServer();
  InsertCorrectGattCharacteristics();
  SetNotifyResponse(*key_based_characteristic_, kKeyBasedResponse);
  fast_pair_device_ = std::make_unique<FastPairDevice>(
      kMetadataId, provider_address_, Protocol::kFastPairInitialPairing);
  SetDecryptedResponse();
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
  EXPECT_TRUE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, GattError) {
  StartGattServer();
  fast_pair_device_ = std::make_unique<FastPairDevice>(
      kMetadataId, provider_address_, Protocol::kFastPairInitialPairing);
  SetDecryptedResponse();
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
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, DataEncryptorCreateError) {
  StartGattServer();
  InsertCorrectGattCharacteristics();
  SetNotifyResponse(*key_based_characteristic_, kKeyBasedResponse);
  fast_pair_device_ = std::make_unique<FastPairDevice>(
      kMetadataId, provider_address_, Protocol::kFastPairInitialPairing);
  fake_data_encryptor_factory_.SetFailedRetrieval();
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
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, WriteResponseError) {
  StartGattServer();
  InsertCorrectGattCharacteristics();
  fast_pair_device_ = std::make_unique<FastPairDevice>(
      kMetadataId, provider_address_, Protocol::kFastPairInitialPairing);
  SetDecryptedResponse();
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
  StartGattServer();
  InsertCorrectGattCharacteristics();
  SetNotifyResponse(*key_based_characteristic_, kWrongResponse);
  fast_pair_device_ = std::make_unique<FastPairDevice>(
      kMetadataId, provider_address_, Protocol::kFastPairInitialPairing);
  SetDecryptedResponse();
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
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, ParseResponseError) {
  StartGattServer();
  InsertCorrectGattCharacteristics();
  SetNotifyResponse(*key_based_characteristic_, kKeyBasedResponse);
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
  EXPECT_FALSE(handshake_->completed_successfully());
}
}  // namespace fastpair
}  // namespace nearby
