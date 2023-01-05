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

#include "internal/network/utils.h"

#include <string>

#include "gtest/gtest.h"

namespace nearby {
namespace network {
namespace {

TEST(UrlEncode, TestEncode) {
  EXPECT_EQ(UrlEncode("abc12&()*t"), "abc12%26()*t");
}

TEST(UrlDecode, TestDecode) {
  EXPECT_EQ(UrlDecode("abc12%26()*t"), "abc12&()*t");
}

TEST(UrlDecode, TestDecodeMatch) {
  std::string encode =
      UrlEncode("fsfs 1892427891$^%$*#$@ZDRIYIUHiheiuah bfd~+_()[]\\|©");
  std::string decode = UrlDecode(encode);

  EXPECT_EQ(decode, "fsfs 1892427891$^%$*#$@ZDRIYIUHiheiuah bfd~+_()[]\\|©");
  EXPECT_EQ(encode,
            "fsfs%201892427891%24%5E%25%24*%23%24%40ZDRIYIUHiheiuah%20bfd~%2B_("
            ")%5B%5D%5C%7C%C2%A9");
}

}  // namespace
}  // namespace network
}  // namespace nearby
