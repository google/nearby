// Copyright 2025 Google LLC
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

#include "internal/encoding/base85.h"

#include <string>

#include "gtest/gtest.h"
#include "absl/random/random.h"

namespace nearby {
namespace encoding {
namespace {

TEST(Base85Test, EncodeAndDecodeString) {
  EXPECT_EQ(Base85Encode(""), "");
  EXPECT_EQ(Base85Decode(Base85Encode("")), "");
  EXPECT_EQ(Base85Encode("b"), "@K");
  EXPECT_EQ(Base85Decode(Base85Encode("b")), "b");
  EXPECT_EQ(Base85Encode("ab"), "@:B");
  EXPECT_EQ(Base85Decode(Base85Encode("ab")), "ab");
  EXPECT_EQ(Base85Encode("xyz"), "G^4T");
  EXPECT_EQ(Base85Decode(Base85Encode("xyz")), "xyz");
  EXPECT_EQ(Base85Encode("true"), "FE2M8");
  EXPECT_EQ(Base85Decode(Base85Encode("true")), "true");
  EXPECT_EQ(Base85Encode("ab", /*padding=*/true), "@:B3:");
  std::string expected_decoded_string("ab\x00\x00", 4);
  EXPECT_EQ(Base85Decode(Base85Encode("ab", /*padding=*/true)),
            expected_decoded_string);
  EXPECT_EQ(Base85Encode("hello,world"), "BOu!rD_-*NEbo7");
  EXPECT_EQ(Base85Decode("BOu!rD_-*NEbo7"), "hello,world");
  EXPECT_EQ(Base85Encode("abcdefgh"), "@:E_WAS,Rg");
  EXPECT_EQ(Base85Decode(Base85Encode("abcdefgh")), "abcdefgh");
}

TEST(Base85Test, EncodeAndDecodeBytes) {
  std::string data;
  data.push_back(0x00);
  EXPECT_EQ(Base85Encode(data), "z");
  std::string expected_data("\x00\x00\x00\x00", 4);
  EXPECT_EQ(Base85Decode(Base85Encode(data)), expected_data);
  data = std::string("\x00\x01", 2);
  EXPECT_EQ(Base85Encode(data), "!!*");
  data = std::string("\x00\x01\xff", 3);
  EXPECT_EQ(Base85Encode(data), "!!3*");
  EXPECT_EQ(Base85Decode(Base85Encode(data)), data);
}

TEST(Base85Test, DecodeStringWithZero) {
  std::string data("\x00\x00\x00\x00\xf8\xf9\xf1\x08\x00\x00\x00\x00\xf9", 13);
  EXPECT_EQ(Base85Encode(data), "zq\"aFczq#");
  EXPECT_EQ(Base85Decode(Base85Encode(data)), data);
  data = std::string("\x00\x00\x00\x00\xf8\xf9\xf1\x08\x00", 9);
  EXPECT_EQ(Base85Encode(data), "zq\"aFcz");
  data = std::string("\x00\x00\x00\x00\xf8\xf9\xf1\x08\x00\x00", 10);
  EXPECT_EQ(Base85Encode(data), "zq\"aFcz");
  data = std::string("\x00\x00\x00\x00\xf8\xf9\xf1\x08\x00\x00\x00", 11);
  EXPECT_EQ(Base85Encode(data), "zq\"aFcz");
}

TEST(Base85Test, EncodeAndDecodeRandomString) {
  absl::BitGen bitgen;
  std::string data;
  for (int i = 0; i < 1000; ++i) {
    data.push_back(absl::Uniform(bitgen, 0, 256));
  }
  EXPECT_EQ(Base85Decode(Base85Encode(data)), data);
}

TEST(Base85Test, InvalideEncodedString) {
  EXPECT_FALSE(Base85Decode("abhjki"));
  EXPECT_FALSE(Base85Decode("abz"));
  EXPECT_FALSE(Base85Decode("u`qL["));
}

}  // namespace
}  // namespace encoding
}  // namespace nearby
