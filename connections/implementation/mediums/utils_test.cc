// Copyright 2026 Google LLC
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

#include "connections/implementation/mediums/utils.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace connections {
namespace {

using ::testing::MatchesRegex;

// The service type is the on-air identifier every platform (and every iOS
// client's Info.plist) must agree on, so pin the exact derivation:
//   "_" + UPPERHEX(SHA-256(service_id)[0:6]) + "._tcp."
TEST(UtilsTest, GenerateServiceTypeIsPinned) {
  // SHA-256("A") =
  // 559aead08264d5795d3909718cdd05abd49572e84fe55590eef31a88a08fdffd
  EXPECT_EQ(Utils::GenerateServiceType("A"), "_559AEAD08264._tcp.");
  // SHA-256("A_UPGRADE") = ec44f58139d2...; covers the upgrade service ID form.
  EXPECT_EQ(Utils::GenerateServiceType("A_UPGRADE"), "_EC44F58139D2._tcp.");
}

TEST(UtilsTest, GenerateServiceTypeHasNsdTypeFormat) {
  EXPECT_THAT(Utils::GenerateServiceType("com.google.location.nearby.test"),
              MatchesRegex("_[0-9A-F]{12}\\._tcp\\."));
}

TEST(UtilsTest, GenerateServiceTypeIsDeterministicPerServiceId) {
  EXPECT_EQ(Utils::GenerateServiceType("service_a"),
            Utils::GenerateServiceType("service_a"));
  EXPECT_NE(Utils::GenerateServiceType("service_a"),
            Utils::GenerateServiceType("service_b"));
  EXPECT_NE(Utils::GenerateServiceType("service_a"),
            Utils::GenerateServiceType("service_a_UPGRADE"));
}

// Same pinned values as WifiAwareTest.GenerateServiceNameIsPinned; checked
// here too because this test also runs where Wi-Fi Aware is stubbed out.
TEST(UtilsTest, GenerateWifiAwareServiceNameIsPinned) {
  EXPECT_EQ(Utils::GenerateWifiAwareServiceName("A"), "_93D474A2A379._tcp");
  EXPECT_EQ(Utils::GenerateWifiAwareServiceName("A_UPGRADE"),
            "_646124107DD7._tcp");
}

}  // namespace
}  // namespace connections
}  // namespace nearby
