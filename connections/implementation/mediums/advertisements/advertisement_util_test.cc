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
#include "connections/implementation/mediums/advertisements/advertisement_util.h"

#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "internal/platform/byte_array.h"

namespace nearby::connections::advertisements {
namespace {

TEST(AdvertisementUtilTest, ReadDeviceName) {
  ByteArray endpoint_info{
      "\x32\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0b"
      "\x54\x65\x73\x74\x20\x64\x65\x76\x69\x63\x65",
      29};
  std::optional<std::string> parsed_device_name = ReadDeviceName(endpoint_info);
  ASSERT_TRUE(parsed_device_name.has_value());
  EXPECT_EQ(*parsed_device_name, "Test device");
}

TEST(AdvertisementUtilTest, ReadInvaidDeviceName) {
  EXPECT_FALSE(ReadDeviceName(ByteArray("")).has_value());
  EXPECT_FALSE(ReadDeviceName(ByteArray("\x30")).has_value());
  EXPECT_FALSE(ReadDeviceName(ByteArray("\x32")).has_value());
  EXPECT_FALSE(
      ReadDeviceName(ByteArray("\x32\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                               "\x00\x00\x00\x00\x00\x00",
                               17))
          .has_value());
  EXPECT_FALSE(
      ReadDeviceName(ByteArray("\x32\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                               "\x00\x00\x00\x00\x00\x00\x0b"
                               "\x54\x65\x73\x74\x20\x64\x65\x76\x69\x63\x65",
                               25))
          .has_value());
}

}  // namespace
}  // namespace nearby::connections::advertisements
