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

#include "sharing/advertisement_capabilities.h"
#include <cstdint>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby::sharing {
namespace {

TEST(AdvertisementCapabilitiesTest, ParseEmpty) {
  EXPECT_TRUE(AdvertisementCapabilities::Parse({}).IsEmpty());
}

TEST(AdvertisementCapabilitiesTest, ParseFileSync) {
  uint8_t data[] = {
      static_cast<uint8_t>(AdvertisementCapabilities::Capability::kFileSync)};
  AdvertisementCapabilities capabilities =
      AdvertisementCapabilities::Parse(data);
  EXPECT_FALSE(capabilities.IsEmpty());
  EXPECT_THAT(capabilities.ToBytes(), testing::ElementsAre(0x01));
}

TEST(AdvertisementCapabilitiesTest, ToBytesEmpty) {
  AdvertisementCapabilities capabilities({});
  EXPECT_TRUE(capabilities.ToBytes().empty());
}

TEST(AdvertisementCapabilitiesTest, ToBytesFileSync) {
  AdvertisementCapabilities capabilities(
      {AdvertisementCapabilities::Capability::kFileSync});
  EXPECT_THAT(capabilities.ToBytes(), testing::ElementsAre(0x01));
}

TEST(AdvertisementCapabilitiesTest, AddCapability) {
  AdvertisementCapabilities capabilities({});
  EXPECT_TRUE(capabilities.IsEmpty());
  capabilities.Add(AdvertisementCapabilities::Capability::kFileSync);
  EXPECT_FALSE(capabilities.IsEmpty());
  EXPECT_THAT(capabilities.ToBytes(), testing::ElementsAre(0x01));
}

TEST(AdvertisementCapabilitiesTest, MultipleAdds) {
  // Currently only kFileSync is supported.
  AdvertisementCapabilities capabilities({});
  capabilities.Add(AdvertisementCapabilities::Capability::kFileSync);
  capabilities.Add(AdvertisementCapabilities::Capability::kFileSync);
  // Duplicate adds should still result in bit 0 being set.
  EXPECT_THAT(capabilities.ToBytes(), testing::ElementsAre(0x01));
}

}  // namespace
}  // namespace nearby::sharing
