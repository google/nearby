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

#include <stddef.h>

#include <cstdint>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/crypto_cros/nearby_base.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace {
// Ensures we don't have all trivial data, i.e. that the data is indeed random.
// Currently, that means the bytes cannot be all the same (e.g. all zeros).
bool IsTrivial(const std::string& bytes) {
  for (size_t i = 0; i < bytes.size(); i++) {
    if (bytes[i] != bytes[0]) {
      return false;
    }
  }
  return true;
}

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

// Basic functionality tests. Does NOT test the security of the random data.

TEST(CryptoTest, RandBytes) {
  std::string bytes(16, '\0');
  RandBytes(nearbybase::WriteInto(&bytes, bytes.size()), bytes.size());
  EXPECT_TRUE(!IsTrivial(bytes));
}

TEST(CryptoTest, RandomString) {
  constexpr size_t kSize = 30;

  std::string bytes(kSize, 0);
  RandBytes(const_cast<std::string::value_type*>(bytes.data()), bytes.size());

  EXPECT_EQ(bytes.size(), kSize);
  EXPECT_TRUE(!IsTrivial(bytes));
}

TEST(CryptoTest, RandData) {
  uint64_t x = nearby::RandData<uint64_t>();
  uint64_t y = nearby::RandData<uint64_t>();

  // Once in a billion years, consecutively generated random numbers will be
  // the same and the test will fail.
  EXPECT_NE(x, y);
  EXPECT_NE(x >> 32, x & 0xFFFFFFFF);
}

}  // namespace
}  // namespace nearby
