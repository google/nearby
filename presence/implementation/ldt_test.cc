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
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace presence {

namespace {
using ::nearby::ByteArray;

// Test data from Android tests.
constexpr absl::string_view kKeySeedBase16 =
    "CCDB2489E9FCAC42B39348B8941ED19A1D360E75E098C8C15E6B1CC2B620CD39";
constexpr absl::string_view kKnownMacBase16 =
    "B4C59FA599241B81758D976B5A621C05232FE1BF89AE5987CA254C3554DCE50E";
constexpr absl::string_view kPlainTextBase16 =
    "CD683FE1A1D1F846543D0A13D4AEA40040C8D67B";
constexpr absl::string_view kCipherTextBase16 =
    "61E481C12F4DE24F2D4AB22D8908F80D3A3F9B40";
constexpr absl::string_view kSaltBase16 = "0C0F";

TEST(Ldt, EncryptAndDecrypt) {
  // Test data copied from NP LDT tests
  ByteArray seed({204, 219, 36, 137, 233, 252, 172, 66, 179, 147, 72,
                  184, 148, 30, 209, 154, 29,  54,  14, 117, 224, 152,
                  200, 193, 94, 107, 28,  194, 182, 32, 205, 57});
  ByteArray known_mac({0xB4, 0xC5, 0x9F, 0xA5, 0x99, 0x24, 0x1B, 0x81,
                       0x75, 0x8D, 0x97, 0x6B, 0x5A, 0x62, 0x1C, 0x05,
                       0x23, 0x2F, 0xE1, 0xBF, 0x89, 0xAE, 0x59, 0x87,
                       0xCA, 0x25, 0x4C, 0x35, 0x54, 0xDC, 0xE5, 0x0E});
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
  absl::StatusOr<LdtEncryptor> encryptor =
      LdtEncryptor::Create(absl::HexStringToBytes(kKeySeedBase16),
                           absl::HexStringToBytes(kKnownMacBase16));
  ASSERT_OK(encryptor);

  absl::StatusOr<std::string> decrypted =
      encryptor->DecryptAndVerify(absl::HexStringToBytes(kCipherTextBase16),
                                  absl::HexStringToBytes(kSaltBase16));

  ASSERT_OK(decrypted);
  EXPECT_EQ(*decrypted, absl::HexStringToBytes(kPlainTextBase16));
}

}  // namespace
}  // namespace presence
}  // namespace nearby
