// Copyright 2020 Google LLC
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

#include "internal/platform/implementation/crypto.h"

#include <string>

#include "gtest/gtest.h"

namespace nearby {
namespace {

TEST(CryptoTest, Md5Hash) {
  const std::string input{"Hello Nearby Connection"};
  const ByteArray expected_md5(
      "\x94\xa3\xbe\xc1\x8d\x30\xe3\x24\x5f\xa1\x4c\xee\xe7\x52\xe9\x36");
  ByteArray md5_hash = Crypto::Md5(input);
  EXPECT_EQ(md5_hash, expected_md5);
}

TEST(CryptoTest, Md5HashOnEmptyInput) {
  EXPECT_EQ(Crypto::Md5(""), ByteArray{});
}

TEST(CryptoTest, Sha256Hash) {
  const std::string input("Hello Nearby Connection");
  const ByteArray expected_sha256(
      "\xb4\x24\xd3\xc0\x58\x12\x9a\x42\xcb\x81\xa0\x4b\x6e\x9d\xfe\x45\x45\x9f"
      "\x15\xf7\xc0\xa9\x32\x2f\xfb\x9\x45\xf0\xf9\xbe\x75\xb");
  ByteArray sha256_hash = Crypto::Sha256(input);
  EXPECT_EQ(sha256_hash, expected_sha256);
}

TEST(CryptoTest, Sha256HashOnEmptyInput) {
  EXPECT_EQ(Crypto::Sha256(""), ByteArray{});
}

}  // namespace
}  // namespace nearby
