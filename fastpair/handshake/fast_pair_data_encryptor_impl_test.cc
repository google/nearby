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

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/escaping.h"
#include "absl/time/time.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/protocol.h"
#include "fastpair/crypto/fast_pair_message_type.h"
#include "fastpair/dataparser/fast_pair_data_parser.h"
#include "fastpair/server_access/fake_fast_pair_repository.h"

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

class FastPairDataEncryptorImplTest : public testing::Test {
 public:
  void TearDown() override { data_encryptor_.reset(); }

  void FailedSetUpNoMetadata() {
    repository_ = std::make_unique<FakeFastPairRepository>();
    FastPairDevice device(kValidModelId, kTestAddress,
                          Protocol::kFastPairInitialPairing);
    FastPairDataEncryptorImpl::Factory::CreateAsync(
        device,
        absl::bind_front(
            &FastPairDataEncryptorImplTest::OnDataEncryptorCreateAsync, this));
  }

  void FailedSetUpNoKeyPair() {
    repository_ = std::make_unique<FakeFastPairRepository>();
    proto::Device metadata;
    std::string decoded_key;
    absl::Base64Unescape(kInvalidPublicAntiSpoof, &decoded_key);
    metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
    repository_->SetFakeMetadata(kValidModelId, metadata);
    FastPairDevice device(kValidModelId, kTestAddress,
                          Protocol::kFastPairInitialPairing);
    FastPairDataEncryptorImpl::Factory::CreateAsync(
        device,
        absl::bind_front(
            &FastPairDataEncryptorImplTest::OnDataEncryptorCreateAsync, this));
  }

  void SuccessfulSetUp() {
    repository_ = std::make_unique<FakeFastPairRepository>();
    proto::Device metadata;
    std::string decoded_key;
    absl::Base64Unescape(kPublicAntiSpoof, &decoded_key);
    metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
    repository_->SetFakeMetadata(kValidModelId, metadata);
    FastPairDevice device(kValidModelId, kTestAddress,
                          Protocol::kFastPairInitialPairing);
    FastPairDataEncryptorImpl::Factory::CreateAsync(
        device,
        absl::bind_front(
            &FastPairDataEncryptorImplTest::OnDataEncryptorCreateAsync, this));
  }

  void OnDataEncryptorCreateAsync(
      std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor) {
    data_encryptor_ = std::move(fast_pair_data_encryptor);
  }

  std::array<uint8_t, kAesBlockByteSize> EncryptBytes() {
    return data_encryptor_->EncryptBytes(kResponseBytes);
  }

  void ParseDecryptedResponse() {
    const std::array<uint8_t, kAesBlockByteSize> bytes =
        data_encryptor_->EncryptBytes(kResponseBytes);

    data_encryptor_->ParseDecryptResponse(
        std::vector<uint8_t>(bytes.begin(), bytes.end()),
        absl::bind_front(
            &FastPairDataEncryptorImplTest::ParseDecryptedResponseCallback,
            this));
  }

  void ParseDecryptedResponseInvalidBytes() {
    const std::array<uint8_t, kAesBlockByteSize> bytes =
        data_encryptor_->EncryptBytes(kResponseBytes);

    data_encryptor_->ParseDecryptResponse(
        std::vector<uint8_t>(bytes.begin() + 3, bytes.end()),
        absl::bind_front(
            &FastPairDataEncryptorImplTest::ParseDecryptedResponseCallback,
            this));
  }

  void ParseDecryptedResponseCallback(
      const std::optional<DecryptedResponse>& response) {
    response_ = response;
  }

  void ParseDecryptedPasskey() {
    const std::array<uint8_t, kAesBlockByteSize> bytes =
        data_encryptor_->EncryptBytes(kPasskeyBytes);

    data_encryptor_->ParseDecryptPasskey(
        std::vector<uint8_t>(bytes.begin(), bytes.end()),
        absl::bind_front(
            &FastPairDataEncryptorImplTest::ParseDecryptPasskeyCallback, this));
  }

  void ParseDecryptedPasskeyInvalidBytes() {
    const std::array<uint8_t, kAesBlockByteSize> bytes =
        data_encryptor_->EncryptBytes(kPasskeyBytes);

    data_encryptor_->ParseDecryptPasskey(
        std::vector<uint8_t>(bytes.begin() + 3, bytes.end()),
        absl::bind_front(
            &FastPairDataEncryptorImplTest::ParseDecryptPasskeyCallback, this));
  }

  void ParseDecryptPasskeyCallback(
      const std::optional<DecryptedPasskey>& passkey) {
    passkey_ = passkey;
  }

 protected:
  std::unique_ptr<FastPairDataEncryptor> data_encryptor_;
  std::optional<DecryptedResponse> response_ = std::nullopt;
  std::optional<DecryptedPasskey> passkey_ = std::nullopt;
  std::unique_ptr<FastPairDataParser> data_parser_;

 private:
  std::unique_ptr<FastPairDevice> device_;
  std::unique_ptr<FakeFastPairRepository> repository_;
};

TEST_F(FastPairDataEncryptorImplTest, FailedSetUpNoMetadata) {
  EXPECT_FALSE(data_encryptor_);
  FailedSetUpNoMetadata();
  EXPECT_FALSE(data_encryptor_);
}

TEST_F(FastPairDataEncryptorImplTest, NoKeyPair) {
  FailedSetUpNoKeyPair();
  EXPECT_FALSE(data_encryptor_);
}

TEST_F(FastPairDataEncryptorImplTest, SuccessfulSetUp) {
  EXPECT_FALSE(data_encryptor_);
  SuccessfulSetUp();
  EXPECT_TRUE(data_encryptor_);
}

TEST_F(FastPairDataEncryptorImplTest, GetPublicKey) {
  SuccessfulSetUp();
  EXPECT_TRUE(data_encryptor_);
  ParseDecryptedPasskey();
  EXPECT_NE(data_encryptor_->GetPublicKey(), std::nullopt);
}

TEST_F(FastPairDataEncryptorImplTest, EncryptBytes) {
  SuccessfulSetUp();
  EXPECT_TRUE(data_encryptor_);
  EXPECT_FALSE(EncryptBytes().empty());
}

TEST_F(FastPairDataEncryptorImplTest, ParseDecryptedResponse) {
  SuccessfulSetUp();
  EXPECT_TRUE(data_encryptor_);
  ParseDecryptedResponse();
  EXPECT_TRUE(response_);
  const std::array<uint8_t, kDecryptedResponseAddressByteSize> kAddressBytes = {
      0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32};
  EXPECT_EQ(response_->address_bytes, kAddressBytes);
  EXPECT_EQ(response_->message_type,
            FastPairMessageType::kKeyBasedPairingResponse);
}

TEST_F(FastPairDataEncryptorImplTest, ParseDecryptedResponse_InvalidInputSize) {
  SuccessfulSetUp();
  EXPECT_TRUE(data_encryptor_);
  ParseDecryptedResponseInvalidBytes();
  EXPECT_FALSE(response_);
}

TEST_F(FastPairDataEncryptorImplTest, ParseDecryptedPasskey) {
  SuccessfulSetUp();
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

TEST_F(FastPairDataEncryptorImplTest, ParseDecryptedPasskey_InvalidInputSize) {
  SuccessfulSetUp();
  EXPECT_TRUE(data_encryptor_);
  ParseDecryptedPasskeyInvalidBytes();
  EXPECT_FALSE(passkey_);
}
}  // namespace
}  // namespace fastpair
}  // namespace nearby
