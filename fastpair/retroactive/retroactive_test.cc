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

#include "fastpair/retroactive/retroactive.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "fastpair/message_stream/fake_provider.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/server_access/fake_fast_pair_repository.h"
#include "internal/base/bluetooth_address.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
namespace nearby {
namespace fastpair {
namespace {

constexpr char kModelId[] = {0xAB, 0xCD, 0xEF};
constexpr absl::string_view kBobPrivateKey =
    "02B437B0EDD6BBD429064A4E529FCBF1C48D0D624924D592274B7ED81193D763";
constexpr absl::string_view kBobPublicKey =
    "F7D496A62ECA416351540AA343BC690A6109F551500666B83B1251FB84FA2860795EBD63D3"
    "B8836F44A9A3E28BB34017E015F5979305D849FDF8DE10123B61D2";

class MediumEnvironmentStarter {
 public:
  MediumEnvironmentStarter() { MediumEnvironment::Instance().Start(); }
  ~MediumEnvironmentStarter() { MediumEnvironment::Instance().Stop(); }
};

class RetroactiveTest : public testing::Test {
 protected:
  void SetUp() override {
    BluetoothClassicMedium& seeker_medium =
        mediums_.GetBluetoothClassic().GetMedium();

    provider_.DiscoverProvider(seeker_medium);
    remote_device_ = seeker_medium.GetRemoteDevice(provider_.GetMacAddress());
    provider_.StartGattServer(
        [this](absl::string_view data) { return KbpCallback(data); });
    provider_.InsertCorrectGattCharacteristics();
    ASSERT_TRUE(remote_device_.IsValid());
  }

  void TearDown() override {
    provider_.DisableProviderRfcomm();
    provider_.Shutdown();
    MediumEnvironment::Instance().Stop();
  }

  void SetUpFastPairRepository(absl::string_view model_id,
                               absl::string_view public_key) {
    proto::Device metadata;
    metadata.mutable_anti_spoofing_key_pair()->set_public_key(public_key);
    repository_.SetFakeMetadata(model_id, metadata);
  }

  std::string KbpCallback(absl::string_view request) {
    // https://developers.google.com/nearby/fast-pair/specifications/characteristics#table1.1
    // Byte 0, 0x00 = Key-based Pairing Request
    // Byte 1, 0x10 (Bit 3 set) = retroactive pairing
    // Bytes 2 - 7, aabbccddeeff, provider's address
    // Bytes 8 - 13, 111213141516, seeker's address
    // Bytes 14 - 15, (not included), random salt
    std::string expected_kbp_request =
        absl::HexStringToBytes("0010aabbccddeeff111213141516");

    // https://developers.google.com/nearby/fast-pair/specifications/characteristics#table1.2.2
    // Byte 0, 0x01 = Key-based Pairing Response
    // Bytes 1 - 6, c1c2c3c4c5c6, provider's public address
    // Bytes 7 - 15, random salt
    std::string kbp_response =
        absl::HexStringToBytes("01c1c2c3c4c5c601234567890abcdef0");

    NEARBY_LOGS(INFO) << "KBP request " << absl::BytesToHexString(request);
    std::string decrypted_request = provider_.DecryptKbpRequest(request);
    EXPECT_EQ(decrypted_request.size(), kEncryptedDataByteSize);
    // The last bytes in decrypted request are random, so we ignore them.
    EXPECT_EQ(decrypted_request.substr(0, expected_kbp_request.size()),
              expected_kbp_request);

    ByteArray response(provider_.Encrypt(kbp_response));

    return response.string_data();
  }

  // The medium environment must be initialized (started) before adding
  // adapters.
  MediumEnvironmentStarter env_;
  Mediums mediums_;
  FakeProvider provider_;
  BluetoothDevice remote_device_;
  FakeFastPairRepository repository_;
};

TEST_F(RetroactiveTest, Constructor) {
  FastPairController controller(&mediums_, remote_device_);
  Retroactive retro(&controller);
}

TEST_F(RetroactiveTest, DISABLED_Pair) {
  SetUpFastPairRepository(kModelId, absl::HexStringToBytes(kBobPublicKey));
  FastPairController controller(&mediums_, remote_device_);
  provider_.EnableProviderRfcomm();
  provider_.LoadAntiSpoofingKey(absl::HexStringToBytes(kBobPrivateKey),
                                absl::HexStringToBytes(kBobPublicKey));
  Retroactive retro(&controller);

  Future<absl::Status> result = retro.Pair();

  // Provider sends their ModelId.
  provider_.WriteProviderBytes(absl::HexStringToBytes("03010003ABCDEF"));
  // Provider sends their BLE address.
  provider_.WriteProviderBytes(absl::HexStringToBytes("03020006AABBCCDDEEFF"));

  // EXPECT_TRUE(result.Get(absl::Minutes(5)).ok());
  EXPECT_TRUE(result.Get(absl::Seconds(5)).ok());
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
