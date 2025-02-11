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
  std::string expected_decoded_string = "ab";
  expected_decoded_string.push_back('\0');
  expected_decoded_string.push_back('\0');
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
  std::string expected_data;
  expected_data.push_back(0x00);
  expected_data.push_back(0x00);
  expected_data.push_back(0x00);
  expected_data.push_back(0x00);
  EXPECT_EQ(Base85Decode(Base85Encode(data)), expected_data);
  data.clear();
  data.push_back(0x00);
  data.push_back(0x01);
  EXPECT_EQ(Base85Encode(data), "!!*");
  data.clear();
  data.push_back(0x00);
  data.push_back(0x01);
  data.push_back(0xff);
  EXPECT_EQ(Base85Encode(data), "!!3*");
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
