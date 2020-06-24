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

#include "core_v2/internal/mediums/uuid.h"

#include "platform_v2/public/crypto.h"
#include "platform_v2/public/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

constexpr absl::string_view kString{"some string"};
constexpr std::uint64_t kNum1 = 0x123456789abcdef0;
constexpr std::uint64_t kNum2 = 0x21436587a9cbed0f;

TEST(UuidTest, CreateFromStringWithMd5) {
  Uuid uuid(kString);
  std::string uuid_str(uuid);
  std::string uuid_data(uuid.data());
  std::string md5_data(Crypto::Md5(kString));
  NEARBY_LOG(INFO, "MD5-based UUID: '%s'", uuid_str.c_str());
  uuid_data[6] = 0;
  uuid_data[8] = 0;
  md5_data[6] = 0;
  md5_data[8] = 0;
  EXPECT_EQ(md5_data, uuid_data);
}

TEST(UuidTest, CreateFromBinary) {
  Uuid uuid(kNum1, kNum2);
  std::string uuid_data(uuid.data());
  std::string uuid_str(uuid);
  NEARBY_LOG(INFO, "UUID: '%s'", uuid_str.c_str());
  EXPECT_EQ(uuid_data[0], (kNum1 >> 56) & 0xFF);
  EXPECT_EQ(uuid_data[1], (kNum1 >> 48) & 0xFF);
  EXPECT_EQ(uuid_data[2], (kNum1 >> 40) & 0xFF);
  EXPECT_EQ(uuid_data[3], (kNum1 >> 32) & 0xFF);
  EXPECT_EQ(uuid_data[4], (kNum1 >> 24) & 0xFF);
  EXPECT_EQ(uuid_data[5], (kNum1 >> 16) & 0xFF);
  EXPECT_EQ(uuid_data[6], (kNum1 >>  8) & 0xFF);
  EXPECT_EQ(uuid_data[7], (kNum1 >>  0) & 0xFF);
  EXPECT_EQ(uuid_data[8], (kNum2 >> 56) & 0xFF);
  EXPECT_EQ(uuid_data[9], (kNum2 >> 48) & 0xFF);
  EXPECT_EQ(uuid_data[10], (kNum2 >> 40) & 0xFF);
  EXPECT_EQ(uuid_data[11], (kNum2 >> 32) & 0xFF);
  EXPECT_EQ(uuid_data[12], (kNum2 >> 24) & 0xFF);
  EXPECT_EQ(uuid_data[13], (kNum2 >> 16) & 0xFF);
  EXPECT_EQ(uuid_data[14], (kNum2 >>  8) & 0xFF);
  EXPECT_EQ(uuid_data[15], (kNum2 >>  0) & 0xFF);
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
