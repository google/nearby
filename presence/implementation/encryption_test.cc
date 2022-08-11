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

#include "presence/implementation/encryption.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/escaping.h"

namespace nearby {
namespace presence {
namespace {

TEST(EncryptionTest, CustomizeBytesSize) {
  std::string ikm = "Input data";
  std::string kExpectedOutputHex = "51d2c0506732febbccf093066db66f269682137e";

  std::string result = Encryption::CustomizeBytesSize(ikm, 20);

  EXPECT_EQ(absl::BytesToHexString(result), kExpectedOutputHex);
}

TEST(EncryptionTest, GenerateRandomByteArray) {
  std::string random1 = Encryption::GenerateRandomByteArray(10);
  std::string random2 = Encryption::GenerateRandomByteArray(10);

  EXPECT_NE(random1, random2);
}

TEST(EncryptionTest, GenerateEncryptedMetadataKeyFor14bytes) {
  const std::string kAuthenticityKey =
      absl::HexStringToBytes("20212223242526272829303132333435");
  const std::string kSalt = absl::HexStringToBytes("0102");
  const std::string kMetadataKey =
      absl::HexStringToBytes("4041424344454647484950515253");
  const std::string kExpectedOutputHex = "637b4868fcbcb3d8a67c47481807";

  auto result = Encryption::GenerateEncryptedMetadataKey(
      kMetadataKey, kAuthenticityKey, kSalt);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(absl::BytesToHexString(result.value()), kExpectedOutputHex);
}

TEST(EncryptionTest, GenerateEncryptedMetadataKeyFor16bytes) {
  const std::string kAuthenticityKey =
      absl::HexStringToBytes("20212223242526272829303132333435");
  const std::string kSalt = absl::HexStringToBytes("0102");
  const std::string kMetadataKey =
      absl::HexStringToBytes("40414243444546474849505152535455");
  const std::string kExpectedOutputHex = "637b4868fcbcb3d8a67c47481807d139";

  auto result = Encryption::GenerateEncryptedMetadataKey(
      kMetadataKey, kAuthenticityKey, kSalt);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(absl::BytesToHexString(result.value()), kExpectedOutputHex);
}

TEST(EncryptionTest, GenerateDecryptedMetadataKeyFor14bytes) {
  const std::string kAuthenticityKey =
      absl::HexStringToBytes("20212223242526272829303132333435");
  const std::string kSalt = absl::HexStringToBytes("0102");
  const std::string kMetadataKeyHex = "4041424344454647484950515253";
  const std::string kEncryptedMetadata =
      absl::HexStringToBytes("637b4868fcbcb3d8a67c47481807");

  auto result = Encryption::GenerateDecryptedMetadataKey(
      kEncryptedMetadata, kAuthenticityKey, kSalt);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(absl::BytesToHexString(result.value()), kMetadataKeyHex);
}

TEST(EncryptionTest, GenerateDecryptedMetadataKeyFor16bytes) {
  const std::string kAuthenticityKey =
      absl::HexStringToBytes("20212223242526272829303132333435");
  const std::string kSalt = absl::HexStringToBytes("0102");
  const std::string kMetadataKeyHex = "40414243444546474849505152535455";
  const std::string kEncryptedMetadata =
      absl::HexStringToBytes("637b4868fcbcb3d8a67c47481807d139");

  auto result = Encryption::GenerateDecryptedMetadataKey(
      kEncryptedMetadata, kAuthenticityKey, kSalt);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(absl::BytesToHexString(result.value()), kMetadataKeyHex);
}

TEST(EncryptionTest, RejectTooLongMetadataKey) {
  const std::string kAuthenticityKey =
      absl::HexStringToBytes("20212223242526272829303132333435");
  const std::string kSalt = absl::HexStringToBytes("0102");
  const std::string kMetadataKey =
      absl::HexStringToBytes("4041424344454647484950515253545566");

  auto result = Encryption::GenerateEncryptedMetadataKey(
      kMetadataKey, kAuthenticityKey, kSalt);

  EXPECT_FALSE(result.ok());
}

TEST(EncryptionTest, RejectInvalidAuthenticityKeySize) {
  const std::string kAuthenticityKey =
      absl::HexStringToBytes("2021222324252627282930313233343546");
  const std::string kSalt = absl::HexStringToBytes("0102");
  const std::string kMetadataKey =
      absl::HexStringToBytes("40414243444546474849505152535455");

  auto result = Encryption::GenerateEncryptedMetadataKey(
      kMetadataKey, kAuthenticityKey, kSalt);

  EXPECT_FALSE(result.ok());
}
}  // namespace
}  // namespace presence
}  // namespace nearby
