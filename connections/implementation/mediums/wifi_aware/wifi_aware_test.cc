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

#include "gtest/gtest.h"

namespace nearby {
namespace connections {
namespace {

// The expected value is a cross-platform contract: it is declared in iOS apps'
// Info.plist (WiFiAwareServices) and must match the name every other platform
// derives, so it must not change without a coordinated update everywhere.
TEST(WifiAwareTest, GetServiceNameMatchesDerivedName) {
  // "_" + uppercase hex of SHA-256("NearbySharing_UPGRADE_AWARE")[0:6] +
  // "._tcp".
  EXPECT_EQ(WifiAware::GetServiceName(), "_8E70C42FBA2B._tcp");
}

}  // namespace
}  // namespace connections
}  // namespace nearby
