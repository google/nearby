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

#include "internal/crypto_cros/random.h"

#include <stddef.h>

#include <string>

#include "gtest/gtest.h"
#include "internal/crypto_cros/nearby_base.h"
#include "internal/platform/implementation/crypto.h"

// Basic functionality tests. Does NOT test the security of the random data.

namespace crypto {
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

TEST(RandBytes, RandBytes) {
  std::string bytes(16, '\0');
  RandBytes(nearbybase::WriteInto(&bytes, bytes.size()), bytes.size());
  EXPECT_TRUE(!IsTrivial(bytes));
}

TEST(RandBytes, RandomString) {
  constexpr size_t kSize = 30;

  std::string bytes(kSize, 0);
  RandBytes(const_cast<std::string::value_type*>(bytes.data()), bytes.size());

  EXPECT_EQ(bytes.size(), kSize);
  EXPECT_TRUE(!IsTrivial(bytes));
}

TEST(RandBytes, RandData) {
  uint64_t x = nearby::RandData<uint64_t>();
  uint64_t y = nearby::RandData<uint64_t>();

  // Once in a billion years, consecutively generated random numbers will be
  // the same and the test will fail.
  EXPECT_NE(x, y);
  EXPECT_NE(x >> 32, x & 0xFFFFFFFF);
}

}  // namespace
}  // namespace crypto
