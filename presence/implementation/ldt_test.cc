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
#include "presence/implementation/ldt.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "internal/platform/byte_array.h"
#include "internal/proto/credential.pb.h"

namespace nearby {
namespace presence {

namespace {
using ::nearby::ByteArray;
using ::nearby::internal::SharedCredential;

#ifdef USE_RUST_LDT

// Test data from Android tests.
constexpr absl::string_view kKeySeedBase16 =
    "BAF3C12E1BBBB3E4367BBD40986D0D7CD158DF6D662AAE6312FE67634B5D4547";
constexpr absl::string_view kKnownMacBase16 =
    "CDDA7C6CF56882D74364F8BE9874A78D7C961BFF9800A40D83F6652E6CF5D1A7";
constexpr absl::string_view kSharedCredentialBase16 =
    "1220BAF3C12E1BBBB3E4367BBD40986D0D7CD158DF6D662AAE6312FE67634B5D45473220"
    "CDDA7C6CF56882D74364F8BE9874A78D7C961BFF9800A40D83F6652E6CF5D1A7";
constexpr absl::string_view kPlainTextBase16 =
    "205BF1D88FF539EC740CCC2EC2DE19353EF30F01054C3E24";
constexpr absl::string_view kCipherTextBase16 =
    "FDABC09D6F8028D4E5E585C62E9A0DB5003F19FEBDF92524";
constexpr absl::string_view kSaltBase16 = "874C";

TEST(Ldt, EncryptAndDecrypt) {
  // Test data copied from NP LDT tests
  ByteArray seed({204, 219, 36, 137, 233, 252, 172, 66, 179, 147, 72,
                  184, 148, 30, 209, 154, 29,  54,  14, 117, 224, 152,
                  200, 193, 94, 107, 28,  194, 182, 32, 205, 57});
  ByteArray known_mac({223, 185, 10,  31,  155, 31, 226, 141, 24,  187, 204,
                       165, 34,  64,  181, 204, 44, 203, 95,  141, 82,  137,
                       163, 203, 100, 235, 53,  65, 202, 97,  75,  180});
  ByteArray test_data({205, 104, 63,  225, 161, 209, 248, 70,  84,  61,
                       10,  19,  212, 174, 164, 0,   64,  200, 214, 123});
  ByteArray salt({12, 15});

  absl::StatusOr<LdtEncryptor> encryptor =
      LdtEncryptor::Create(seed.AsStringView(), known_mac.AsStringView());
  ASSERT_OK(encryptor);
  absl::StatusOr<std::string> encrypted =
      encryptor->Encrypt(test_data.AsStringView(), salt.AsStringView());
  ASSERT_OK(encrypted);
  absl::StatusOr<std::string> decrypted =
      encryptor->DecryptAndVerify(*encrypted, salt.AsStringView());
  ASSERT_OK(decrypted);
  EXPECT_EQ(*decrypted, test_data.AsStringView());
}

TEST(Ldt, EncryptAndroidData) {
  absl::StatusOr<LdtEncryptor> encryptor =
      LdtEncryptor::Create(absl::HexStringToBytes(kKeySeedBase16),
                           absl::HexStringToBytes(kKnownMacBase16));
  ASSERT_OK(encryptor);

  absl::StatusOr<std::string> encrypted =
      encryptor->Encrypt(absl::HexStringToBytes(kPlainTextBase16),
                         absl::HexStringToBytes(kSaltBase16));

  ASSERT_OK(encrypted);
  EXPECT_EQ(*encrypted, absl::HexStringToBytes(kCipherTextBase16));
}

TEST(Ldt, DecryptAndroidData) {
  SharedCredential shared_credential;
  ASSERT_TRUE(shared_credential.ParseFromString(
      absl::HexStringToBytes(kSharedCredentialBase16)));
  absl::StatusOr<LdtEncryptor> encryptor = LdtEncryptor::Create(
      shared_credential.key_seed(),
      shared_credential.metadata_encryption_key_tag_v0());
  ASSERT_OK(encryptor);

  absl::StatusOr<std::string> decrypted =
      encryptor->DecryptAndVerify(absl::HexStringToBytes(kCipherTextBase16),
                                  absl::HexStringToBytes(kSaltBase16));

  ASSERT_OK(decrypted);
  EXPECT_EQ(*decrypted, absl::HexStringToBytes(kPlainTextBase16));
}

#else
TEST(Ldt, LdtUnvailable) {
  ByteArray seed({204, 219, 36, 137, 233, 252, 172, 66, 179, 147, 72,
                  184, 148, 30, 209, 154, 29,  54,  14, 117, 224, 152,
                  200, 193, 94, 107, 28,  194, 182, 32, 205, 57});
  ByteArray known_mac({223, 185, 10,  31,  155, 31, 226, 141, 24,  187, 204,
                       165, 34,  64,  181, 204, 44, 203, 95,  141, 82,  137,
                       163, 203, 100, 235, 53,  65, 202, 97,  75,  180});

  absl::StatusOr<LdtEncryptor> encryptor =
      LdtEncryptor::Create(seed.AsStringView(), known_mac.AsStringView());

  EXPECT_THAT(encryptor.status(),
              absl::UnavailableError("Failed to create LDT encryptor"));
}
#endif

}  // namespace
}  // namespace presence
}  // namespace nearby
