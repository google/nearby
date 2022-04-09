
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

#include "connections/implementation/mediums/wifi_hotspot.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/system_clock.h"
#include "internal/platform/wifi_hotspot.h"
#include "internal/platform/wifi_hotspot_credential.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

constexpr absl::string_view kGateway{"0.0.0.0"};
constexpr absl::Duration kWait = absl::Milliseconds(300);
constexpr absl::string_view kSsid{"Direct_Nearby"};
constexpr absl::string_view kPassword{"12345678"};

TEST(WifiHotspotTest, ConStartHotspot) {
  WifiHotspot wifi_hotspot_a;

  if (wifi_hotspot_a.IsAvailable()) {
    wifi_hotspot_a.StartWifiHotspot();
    SystemClock::Sleep(kWait);
    wifi_hotspot_a.StopWifiHotspot();
  }
  // TODO(b/227482970): Add test coverage for wifi_hotspot.cc
}

TEST(WifiHotspotTest, CanConnectToHotspot) {
  WifiHotspot wifi_hotspot_a;

  if (wifi_hotspot_a.IsAvailable()) {
    std::string ssid(kSsid);
    std::string password(kPassword);
    wifi_hotspot_a.ConnectWifiHotspot(ssid, password);
    SystemClock::Sleep(kWait);
    wifi_hotspot_a.DisconnectWifiHotspot();
  }
  // TODO(b/227482970): Add test coverage for wifi_hotspot.cc
}

TEST(WifiHotspotTest, CanGetCredentials) {
  WifiHotspot wifi_hotspot_a;

  if (wifi_hotspot_a.IsAvailable()) {
    HotspotCredentials* hotspot_crendential =
        wifi_hotspot_a.GetCredentials("TEST_SERVICE_ID");
    EXPECT_EQ(kGateway, hotspot_crendential->GetGateway());
  }
  // TODO(b/227482970): Add test coverage for wifi_hotspot.cc
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
