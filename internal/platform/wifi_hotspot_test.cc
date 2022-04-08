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

#include "internal/platform/wifi_hotspot.h"

#include <string>

#include "gtest/gtest.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/wifi_hotspot_credential.h"

namespace location {
namespace nearby {
namespace {

constexpr absl::string_view kSsid{"Direct-357a2d8c"};
constexpr absl::string_view kPassword{"b592f7d3"};

TEST(HotspotCredentialsTest, SetGetSsid) {
  std::string ssid(kSsid);
  HotspotCredentials hotspot_credentials;
  hotspot_credentials.SetSSID(ssid);

  EXPECT_EQ(kSsid, hotspot_credentials.GetSSID());
}

TEST(HotspotCredentialsTest, SetGetPassword) {
  std::string password(kPassword);
  HotspotCredentials hotspot_credentials;
  hotspot_credentials.SetPassword(password);

  EXPECT_EQ(kPassword, hotspot_credentials.GetPassword());
}

TEST(WifiHotspotMediumTest, ConstructorDestructorWorks) {
  WifiHotspotMedium wifi_hotspot_a;
  WifiHotspotMedium wifi_hotspot_b;

  if (wifi_hotspot_a.IsValid() && wifi_hotspot_b.IsValid()) {
    // Make sure we can create 2 distinct mediums.
    EXPECT_NE(&wifi_hotspot_a.GetImpl(), &wifi_hotspot_b.GetImpl());
  }
  // TODO(b/227482970): Add test coverage for wifi_hotspot.cc
}

TEST(WifiHotspotMediumTest, CanConnectToService) {
  CancellationFlag flag;
  WifiHotspotMedium wifi_hotspot_a;
  WifiHotspotMedium wifi_hotspot_b;

  WifiHotspotSocket socket_a;
  WifiHotspotServerSocket socket_b;
  EXPECT_FALSE(socket_a.IsValid());
  // TODO(b/227482970): Add test coverage for wifi_hotspot.cc
}

TEST(WifiHotspotMediumTest, CanSartHotspot) {
  WifiHotspotMedium wifi_hotspot;
  // TODO(b/227482970): Add test coverage for wifi_hotspot.cc
}

}  // namespace
}  // namespace nearby
}  // namespace location
