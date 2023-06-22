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

#include "fastpair/handshake/fast_pair_data_encryptor_impl.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/escaping.h"
#include "fastpair/common/account_key.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/common/protocol.h"
#include "fastpair/crypto/fast_pair_encryption.h"
#include "fastpair/dataparser/fast_pair_data_parser.h"
#include "fastpair/handshake/fast_pair_data_encryptor.h"
#include "internal/platform/count_down_latch.h"

namespace nearby {
namespace fastpair {
namespace {
constexpr std::array<uint8_t, kAesBlockByteSize> kResponseBytes = {
    0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D,
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6};

constexpr std::array<uint8_t, kAesBlockByteSize> kPasskeyBytes = {
    0x02, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D,
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6};

constexpr char kPublicAntiSpoof[] =
    "Wuyr48lD3txnUhGiMF1IfzlTwRxxe+wMB1HLzP+"
    "0wVcljfT3XPoiy1fntlneziyLD5knDVAJSE+RM/zlPRP/Jg==";
constexpr char kInvalidPublicAntiSpoof[] = "InvalidPublicAntiSpoof";

constexpr char kValidModelId[] = "718c17";
constexpr char kTestAddress[] = "test_address";
constexpr absl::Duration kWaitTimeout = absl::Milliseconds(200);

class FastPairDataEncryptorImplTest : public testing::Test {
 public:
  void TearDown() override { data_encryptor_.reset(); }

  void FailedSetUpDeviceWithNoPublicKey() {
    proto::GetObservedDeviceResponse response;
    std::string decoded_key;
    absl::Base64Unescape(kInvalidPublicAntiSpoof, &decoded_key);
    response.mutable_device()->mutable_anti_spoofing_key_pair()->set_public_key(
        decoded_key);
    FastPairDevice device(kValidModelId, kTestAddress,
                          Protocol::kFastPairInitialPairing);
    device.SetMetadata(DeviceMetadata(response));
    CountDownLatch latch(1);
    FastPairDataEncryptorImpl::Factory::CreateAsync(
        device, absl::bind_front(
                    &FastPairDataEncryptorImplTest::OnDataEncryptorCreateAsync,
                    this, latch));
    latch.Await();
  }

  void SuccessCreateFastPairDataEncryptorWithKeyExchange() {
    proto::GetObservedDeviceResponse response;
    std::string decoded_key;
    absl::Base64Unescape(kPublicAntiSpoof, &decoded_key);
    response.mutable_device()->mutable_anti_spoofing_key_pair()->set_public_key(
        decoded_key);
    FastPairDevice device(kValidModelId, kTestAddress,
                          Protocol::kFastPairInitialPairing);
    device.SetMetadata(DeviceMetadata(response));
    CountDownLatch latch(1);
    FastPairDataEncryptorImpl::Factory::CreateAsync(
        device, absl::bind_front(
                    &FastPairDataEncryptorImplTest::OnDataEncryptorCreateAsync,
                    this, latch));
    latch.Await();
  }

  void SuccessCreateFastPairDataEncryptorWithAccountKey() {
    FastPairDevice device(kValidModelId, kTestAddress,
                          Protocol::kFastPairSubsequentPairing);
    const std::vector<uint8_t> kAccountKey{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                           0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                           0xCC, 0xDD, 0xEE, 0xFF};
    device.SetAccountKey(
        AccountKey(std::string(kAccountKey.begin(), kAccountKey.end())));
    CountDownLatch latch(1);
    FastPairDataEncryptorImpl::Factory::CreateAsync(
        device, absl::bind_front(
                    &FastPairDataEncryptorImplTest::OnDataEncryptorCreateAsync,
                    this, latch));
    latch.Await();
  }

  void OnDataEncryptorCreateAsync(
      CountDownLatch latch,
      std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor) {
    data_encryptor_ = std::move(fast_pair_data_encryptor);
    latch.CountDown();
  }

  std::array<uint8_t, kAesBlockByteSize> EncryptBytes() {
    return data_encryptor_->EncryptBytes(kResponseBytes);
  }

  void ParseDecryptedResponse() {
    const std::array<uint8_t, kAesBlockByteSize> bytes =
        data_encryptor_->EncryptBytes(kResponseBytes);
    CountDownLatch latch(1);
    data_encryptor_->ParseDecryptResponse(
        std::vector<uint8_t>(bytes.begin(), bytes.end()),
        absl::bind_front(
            &FastPairDataEncryptorImplTest::ParseDecryptedResponseCallback,
            this, latch));
    latch.Await();
  }

  void ParseDecryptedResponseInvalidBytes() {
    const std::array<uint8_t, kAesBlockByteSize> bytes =
        data_encryptor_->EncryptBytes(kResponseBytes);
    CountDownLatch latch(1);
    data_encryptor_->ParseDecryptResponse(
        std::vector<uint8_t>(bytes.begin() + 3, bytes.end()),
        absl::bind_front(
            &FastPairDataEncryptorImplTest::ParseDecryptedResponseCallback,
            this, latch));
    latch.Await();
  }

  void ParseDecryptedResponseCallback(
      CountDownLatch latch, const std::optional<DecryptedResponse>& response) {
    response_ = response;
    latch.CountDown();
  }

  void ParseDecryptedPasskey() {
    const std::array<uint8_t, kAesBlockByteSize> bytes =
        data_encryptor_->EncryptBytes(kPasskeyBytes);
    CountDownLatch latch(1);
    data_encryptor_->ParseDecryptPasskey(
        std::vector<uint8_t>(bytes.begin(), bytes.end()),
        absl::bind_front(
            &FastPairDataEncryptorImplTest::ParseDecryptPasskeyCallback, this,
            latch));
    latch.Await();
  }

  void ParseDecryptedPasskeyWithAccountKey() {
    const std::array<uint8_t, kSharedSecretKeyByteSize> kAccountKey{
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    auto bytes = FastPairEncryption::EncryptBytes(kAccountKey, kPasskeyBytes);
    CountDownLatch latch(1);
    data_encryptor_->ParseDecryptPasskey(
        std::vector<uint8_t>(bytes.begin(), bytes.end()),
        absl::bind_front(
            &FastPairDataEncryptorImplTest::ParseDecryptPasskeyCallback, this,
            latch));
    latch.Await();
  }

  void ParseDecryptedPasskeyInvalidBytes() {
    const std::array<uint8_t, kAesBlockByteSize> bytes =
        data_encryptor_->EncryptBytes(kPasskeyBytes);
    CountDownLatch latch(1);
    data_encryptor_->ParseDecryptPasskey(
        std::vector<uint8_t>(bytes.begin() + 3, bytes.end()),
        absl::bind_front(
            &FastPairDataEncryptorImplTest::ParseDecryptPasskeyCallback, this,
            latch));
    latch.Await();
  }

  void ParseDecryptResponseCallback(
      CountDownLatch latch, const std::optional<DecryptedResponse>& response) {
    response_ = response;
  }

  void ParseDecryptPasskeyCallback(
      CountDownLatch latch, const std::optional<DecryptedPasskey>& passkey) {
    passkey_ = passkey;
    latch.CountDown();
  }

 protected:
  std::unique_ptr<FastPairDataEncryptor> data_encryptor_;
  std::optional<DecryptedResponse> response_ = std::nullopt;
  std::optional<DecryptedPasskey> passkey_ = std::nullopt;
  std::unique_ptr<FastPairDataParser> data_parser_;

 private:
  std::unique_ptr<FastPairDevice> device_;
};

TEST_F(FastPairDataEncryptorImplTest, NoKeyPair) {
  FailedSetUpDeviceWithNoPublicKey();
  EXPECT_FALSE(data_encryptor_);
}

TEST_F(FastPairDataEncryptorImplTest,
       SuccessCreateFastPairDataEncryptorWithKeyExchange) {
  EXPECT_FALSE(data_encryptor_);
  SuccessCreateFastPairDataEncryptorWithKeyExchange();
  EXPECT_TRUE(data_encryptor_);
}

TEST_F(FastPairDataEncryptorImplTest,
       SuccessCreateFastPairDataEncryptorWithAccountKey) {
  EXPECT_FALSE(data_encryptor_);
  SuccessCreateFastPairDataEncryptorWithAccountKey();
  EXPECT_TRUE(data_encryptor_);
}

TEST_F(FastPairDataEncryptorImplTest, GetPublicKey) {
  SuccessCreateFastPairDataEncryptorWithKeyExchange();
  EXPECT_TRUE(data_encryptor_);
  EXPECT_NE(data_encryptor_->GetPublicKey(), std::nullopt);
}

TEST_F(FastPairDataEncryptorImplTest, GetNoPublicKey) {
  SuccessCreateFastPairDataEncryptorWithAccountKey();
  EXPECT_TRUE(data_encryptor_);
  EXPECT_EQ(data_encryptor_->GetPublicKey(), std::nullopt);
}

TEST_F(FastPairDataEncryptorImplTest, EncryptBytesWithKeyExchange) {
  SuccessCreateFastPairDataEncryptorWithKeyExchange();
  EXPECT_TRUE(data_encryptor_);
  EXPECT_FALSE(EncryptBytes().empty());
}

TEST_F(FastPairDataEncryptorImplTest, EncryptBytesWithAccountKey) {
  SuccessCreateFastPairDataEncryptorWithAccountKey();
  EXPECT_TRUE(data_encryptor_);
  EXPECT_FALSE(EncryptBytes().empty());
}

TEST_F(FastPairDataEncryptorImplTest, ParseDecryptedResponseWithKeyExchange) {
  SuccessCreateFastPairDataEncryptorWithKeyExchange();
  EXPECT_TRUE(data_encryptor_);
  ParseDecryptedResponse();
  EXPECT_TRUE(response_);
  const std::array<uint8_t, kDecryptedResponseAddressByteSize> kAddressBytes = {
      0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32};
  EXPECT_EQ(response_->address_bytes, kAddressBytes);
  EXPECT_EQ(response_->message_type,
            FastPairMessageType::kKeyBasedPairingResponse);
}

TEST_F(FastPairDataEncryptorImplTest, ParseDecryptedResponseWithAccountKey) {
  SuccessCreateFastPairDataEncryptorWithAccountKey();
  EXPECT_TRUE(data_encryptor_);
  ParseDecryptedResponse();
  EXPECT_TRUE(response_);
  const std::array<uint8_t, kDecryptedResponseAddressByteSize> kAddressBytes = {
      0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32};
  EXPECT_EQ(response_->address_bytes, kAddressBytes);
  EXPECT_EQ(response_->message_type,
            FastPairMessageType::kKeyBasedPairingResponse);
}

TEST_F(FastPairDataEncryptorImplTest, ParseDecryptedResponseInvalidInputSize) {
  SuccessCreateFastPairDataEncryptorWithKeyExchange();
  EXPECT_TRUE(data_encryptor_);
  ParseDecryptedResponseInvalidBytes();
  EXPECT_FALSE(response_);
}

TEST_F(FastPairDataEncryptorImplTest, ParseDecryptedPasskeyWithKeyExchange) {
  SuccessCreateFastPairDataEncryptorWithKeyExchange();
  EXPECT_TRUE(data_encryptor_);
  ParseDecryptedPasskey();
  EXPECT_TRUE(passkey_);
  // Passkey bytes.
  std::array<uint8_t, 3> passkey_bytes = {0x5E, 0x3F, 0x45};
  uint32_t passkey = passkey_bytes[2];
  passkey += passkey_bytes[1] << 8;
  passkey += passkey_bytes[0] << 16;

  EXPECT_EQ(passkey_->passkey, passkey);
  EXPECT_EQ(passkey_->message_type, FastPairMessageType::kSeekersPasskey);
}

TEST_F(FastPairDataEncryptorImplTest, ParseDecryptedPasskeyWithAccountKey) {
  SuccessCreateFastPairDataEncryptorWithAccountKey();
  EXPECT_TRUE(data_encryptor_);
  ParseDecryptedPasskeyWithAccountKey();
  EXPECT_TRUE(passkey_);
  // Passkey bytes.
  std::array<uint8_t, 3> passkey_bytes = {0x5E, 0x3F, 0x45};
  uint32_t passkey = passkey_bytes[2];
  passkey += passkey_bytes[1] << 8;
  passkey += passkey_bytes[0] << 16;

  EXPECT_EQ(passkey_->passkey, passkey);
  EXPECT_EQ(passkey_->message_type, FastPairMessageType::kSeekersPasskey);
}

TEST_F(FastPairDataEncryptorImplTest, ParseDecryptedPasskeyInvalidInputSize) {
  SuccessCreateFastPairDataEncryptorWithKeyExchange();
  EXPECT_TRUE(data_encryptor_);
  ParseDecryptedPasskeyInvalidBytes();
  EXPECT_FALSE(passkey_);
}
}  // namespace
}  // namespace fastpair
}  // namespace nearby
