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

#include "internal/platform/uuid.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "internal/platform/crypto.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace {

using ::testing::StrCaseEq;

constexpr absl::string_view kString{"some string"};
constexpr std::uint64_t kCopresenceServiceUuidMsb = 0x0000FEF300001000;
constexpr std::uint64_t kCopresenceServiceUuidLsb = 0x800000805F9B34FB;

TEST(UuidTest, ConstructorWorks) {
  Uuid uuid_1;

  EXPECT_TRUE(uuid_1.IsEmpty());

  Uuid uuid_2("test");

  EXPECT_FALSE(uuid_2.IsEmpty());

  Uuid uuid_3(1234, 5678);

  EXPECT_FALSE(uuid_3.IsEmpty());
}

TEST(UuidTest, CreateFromStringWithMd5) {
  Uuid uuid(kString);
  std::string uuid_str(uuid);
  std::array<char, 16> uuid_data = uuid.data();
  std::string md5_data(Crypto::Md5(kString));
  NEARBY_LOGS(INFO) << "MD5-based UUID: " << uuid_str;
  uuid_data[6] = 0;
  uuid_data[8] = 0;
  md5_data[6] = 0;
  md5_data[8] = 0;
  EXPECT_EQ(std::string(uuid_data.data(), uuid_data.size()), md5_data);
}

TEST(UuidTest, CreateFromBinaryCanOutputString) {
  Uuid uuid(kCopresenceServiceUuidMsb, kCopresenceServiceUuidLsb);
  std::array<char, 16> uuid_data = uuid.data();
  std::string uuid_str(uuid);
  NEARBY_LOGS(INFO) << "UUID: " << uuid_str;
  EXPECT_EQ(uuid_data[0],
            static_cast<char>((kCopresenceServiceUuidMsb >> 56) & 0xFF));
  EXPECT_EQ(uuid_data[1],
            static_cast<char>((kCopresenceServiceUuidMsb >> 48) & 0xFF));
  EXPECT_EQ(uuid_data[2],
            static_cast<char>((kCopresenceServiceUuidMsb >> 40) & 0xFF));
  EXPECT_EQ(uuid_data[3],
            static_cast<char>((kCopresenceServiceUuidMsb >> 32) & 0xFF));
  EXPECT_EQ(uuid_data[4],
            static_cast<char>((kCopresenceServiceUuidMsb >> 24) & 0xFF));
  EXPECT_EQ(uuid_data[5],
            static_cast<char>((kCopresenceServiceUuidMsb >> 16) & 0xFF));
  EXPECT_EQ(uuid_data[6],
            static_cast<char>((kCopresenceServiceUuidMsb >> 8) & 0xFF));
  EXPECT_EQ(uuid_data[7],
            static_cast<char>((kCopresenceServiceUuidMsb >> 0) & 0xFF));
  EXPECT_EQ(uuid_data[8],
            static_cast<char>((kCopresenceServiceUuidLsb >> 56) & 0xFF));
  EXPECT_EQ(uuid_data[9],
            static_cast<char>((kCopresenceServiceUuidLsb >> 48) & 0xFF));
  EXPECT_EQ(uuid_data[10],
            static_cast<char>((kCopresenceServiceUuidLsb >> 40) & 0xFF));
  EXPECT_EQ(uuid_data[11],
            static_cast<char>((kCopresenceServiceUuidLsb >> 32) & 0xFF));
  EXPECT_EQ(uuid_data[12],
            static_cast<char>((kCopresenceServiceUuidLsb >> 24) & 0xFF));
  EXPECT_EQ(uuid_data[13],
            static_cast<char>((kCopresenceServiceUuidLsb >> 16) & 0xFF));
  EXPECT_EQ(uuid_data[14],
            static_cast<char>((kCopresenceServiceUuidLsb >> 8) & 0xFF));
  EXPECT_EQ(uuid_data[15],
            static_cast<char>((kCopresenceServiceUuidLsb >> 0) & 0xFF));

  EXPECT_THAT(uuid_str, StrCaseEq("0000FEF3-0000-1000-8000-00805F9B34FB"));
  EXPECT_THAT(uuid.Get16BitAsString(), StrCaseEq("FEF3"));
}

TEST(UuidTest, ComparisonEqualWorksForMsbAndLsb) {
  Uuid uuid_1(kCopresenceServiceUuidMsb, kCopresenceServiceUuidLsb);
  Uuid uuid_2(kCopresenceServiceUuidMsb, kCopresenceServiceUuidLsb);

  EXPECT_EQ(uuid_1, uuid_2);

  Uuid uuid_3(1234, 5678);
  Uuid uuid_4(8765, 4321);

  EXPECT_FALSE(uuid_3 == uuid_4);
}

TEST(UuidTest, ComparisonEqualWorksForString) {
  Uuid uuid_1("uuid");
  Uuid uuid_2("uuid");

  EXPECT_EQ(uuid_1, uuid_2);

  Uuid uuid_3("foo");
  Uuid uuid_4("loo");

  EXPECT_FALSE(uuid_3 == uuid_4);
}

TEST(UuidTest, OperatorGreaterThan) {
  Uuid a("12345678-1234-1234-1234-123456789012");
  Uuid b("12345678-1234-1234-1234-123456789013");

  EXPECT_TRUE(a < b);
}

TEST(UuidTest, ConstructUuidFromString) {
  std::optional<Uuid> a =
      Uuid::FromString("12345678-1234-1234-1234-123456789012");

  ASSERT_TRUE(a.has_value());
  EXPECT_EQ(std::string(*a), "12345678-1234-1234-1234-123456789012");
}

}  // namespace
}  // namespace nearby
