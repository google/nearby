// Copyright 2023 Google LLC
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

#include "sharing/internal/base/encode.h"

#include <stdint.h>

#include <vector>

#include "gtest/gtest.h"

namespace nearby {
namespace utils {
namespace {

TEST(HexEncode, HexEncodeNormal) {
  std::vector<uint8_t> data = {0x04, 0x3f, 0xf7, 0xe6, 0xf8, 0xc7, 0x0c};
  EXPECT_EQ(HexEncode(data), "043FF7E6F8C70C");
}

TEST(HexEncode, HexEncodeEmpty) {
  EXPECT_EQ(HexEncode(std::vector<uint8_t>({})), "");
}

TEST(HexEncode, HexEncodeWithPadding) {
  EXPECT_EQ(HexEncode(std::vector<uint8_t>({0, 0, 0, 0})), "00000000");
}

}  // namespace
}  // namespace utils
}  // namespace nearby
