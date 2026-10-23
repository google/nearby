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

#include "connections/implementation/mediums/wifi_aware/wifi_aware.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/implementation/service_id_constants.h"

namespace nearby {
namespace connections {
namespace {

using ::testing::EndsWith;
using ::testing::MatchesRegex;
using ::testing::Not;

constexpr char kServiceId[] = "A";

// The name is what the remote device (possibly Android) subscribes to and what
// an iOS client declares in its Info.plist, so pin the exact derivation:
//   "_" + UPPERHEX(SHA-256(service_id + "_AWARE")[0:6]) + "._tcp"
TEST(WifiAwareTest, GenerateServiceNameIsPinned) {
  // SHA-256("A_AWARE") = 93d474a2a379...
  EXPECT_EQ(WifiAware::GenerateServiceName("A"), "_93D474A2A379._tcp");
  // SHA-256("A_UPGRADE_AWARE") = 646124107dd7...; the bandwidth upgrade form.
  EXPECT_EQ(WifiAware::GenerateServiceName("A_UPGRADE"), "_646124107DD7._tcp");
}

// Apple's WiFiAwareServices keys and Android's NAN service names have no
// trailing dot, unlike the mDNS service types.
TEST(WifiAwareTest, GenerateServiceNameHasNoTrailingDot) {
  EXPECT_THAT(WifiAware::GenerateServiceName(kServiceId),
              MatchesRegex("_[0-9A-F]{12}\\._tcp"));
  EXPECT_THAT(WifiAware::GenerateServiceName(kServiceId), Not(EndsWith(".")));
}

TEST(WifiAwareTest, GenerateServiceNameDiffersFromWifiLanServiceType) {
  const std::string wifi_lan_type = Utils::GenerateServiceType(kServiceId);
  EXPECT_NE(WifiAware::GenerateServiceName(kServiceId),
            wifi_lan_type.substr(0, wifi_lan_type.size() - 1));
}

// The initiator derives the name from the upgrade service ID it is handed; the
// responder only has the plain service ID and wraps it itself. Both must land
// on the same name.
TEST(WifiAwareTest, GenerateServiceNameAgreesBetweenInitiatorAndResponder) {
  const std::string upgrade_service_id =
      WrapInitiatorUpgradeServiceId(kServiceId);
  EXPECT_EQ(WifiAware::GenerateServiceName(upgrade_service_id),
            WifiAware::GenerateServiceName(
                WrapInitiatorUpgradeServiceId(kServiceId)));
  EXPECT_NE(WifiAware::GenerateServiceName(upgrade_service_id),
            WifiAware::GenerateServiceName(kServiceId));
}

}  // namespace
}  // namespace connections
}  // namespace nearby
