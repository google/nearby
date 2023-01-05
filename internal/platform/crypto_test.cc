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

#include "internal/platform/crypto.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/byte_array.h"

namespace nearby {

TEST(CryptoTest, Md5GeneratesHash) {
  const ByteArray expected_md5(
      "\xb4\x5c\xff\xe0\x84\xdd\x3d\x20\xd9\x28\xbe\xe8\x5e\x7b\x0f\x21");
  ByteArray md5_hash = Crypto::Md5("string");
  EXPECT_EQ(md5_hash, expected_md5);
}

TEST(CryptoTest, Md5ReturnsEmptyOnError) {
  EXPECT_EQ(Crypto::Md5(""), ByteArray{});
}

TEST(CryptoTest, Sha256GeneratesHash) {
  const ByteArray expected_sha256(
      "\x47\x32\x87\xf8\x29\x8d\xba\x71\x63\xa8\x97\x90\x89\x58\xf7\xc0"
      "\xea\xe7\x33\xe2\x5d\x2e\x02\x79\x92\xea\x2e\xdc\x9b\xed\x2f\xa8");
  ByteArray sha256_hash = Crypto::Sha256("string");
  EXPECT_EQ(sha256_hash, expected_sha256);
}

TEST(CryptoTest, Sha256ReturnsEmptyOnError) {
  EXPECT_EQ(Crypto::Sha256(""), ByteArray{});
}

}  // namespace nearby
