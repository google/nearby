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

#include "connections/implementation/mediums/ble_v2/ble_utils.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace bleutils {

namespace {

using ::testing::StrCaseEq;

TEST(BleUtilsTest, CanGenerateHash) {
  std::string service_id = {"service_id"};

  ByteArray generated_bytes = GenerateHash(service_id, 4);

  // Can generate a non-empty byte array.
  EXPECT_FALSE(generated_bytes.Empty());

  generated_bytes = GenerateHash(service_id, 0);

  // Cannot generate a non-empty byte array for size 0.
  EXPECT_TRUE(generated_bytes.Empty());
}

TEST(BleUtilsTest, CanGenerateServiceIdHash) {
  std::string service_id = {"service_id"};

  ByteArray generated_bytes = GenerateServiceIdHash(service_id);

  // Can generate a non-empty byte array.
  EXPECT_FALSE(generated_bytes.Empty());
}

TEST(BleUtilsTest, CanGenerateDeviceToken) {
  ByteArray generated_bytes = GenerateDeviceToken();

  // Can generate a non-empty byte array.
  EXPECT_FALSE(generated_bytes.Empty());
}

TEST(BleUtilsTest, CanGenerateAdvertisementHash) {
  ByteArray empty_advertisement_bytes = {};
  ByteArray non_empty_advertisement_bytes("abcd");
  ByteArray generated_bytes_1 =
      GenerateAdvertisementHash(empty_advertisement_bytes);

  ByteArray generated_bytes_2 =
      GenerateAdvertisementHash(non_empty_advertisement_bytes);

  // Can generate a non-empty byte array.
  EXPECT_FALSE(generated_bytes_1.Empty());
  EXPECT_FALSE(generated_bytes_2.Empty());
}

TEST(BleUtilsTest, CanGenerateAdvertisementUuid) {
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<Uuid> generated_uuid = GenerateAdvertisementUuid(0);
  ASSERT_TRUE(generated_uuid.has_value());
  EXPECT_THAT(std::string(*generated_uuid),
              StrCaseEq("00000000-0000-3000-8000-000000000000"));

  generated_uuid = GenerateAdvertisementUuid(1);
  ASSERT_TRUE(generated_uuid.has_value());
  EXPECT_THAT(std::string(*generated_uuid),
              StrCaseEq("00000000-0000-3000-8000-000000000001"));

  generated_uuid = GenerateAdvertisementUuid(10);
  ASSERT_TRUE(generated_uuid.has_value());
  EXPECT_THAT(std::string(*generated_uuid),
              StrCaseEq("00000000-0000-3000-8000-00000000000a"));

  // Can't generate an advertisement uuid for slot < 0.
  generated_uuid = GenerateAdvertisementUuid(-1);
  EXPECT_FALSE(generated_uuid.has_value());
}

}  // namespace

}  // namespace bleutils
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
