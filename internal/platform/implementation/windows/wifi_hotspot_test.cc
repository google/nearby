// Copyright 2023 Google LLC
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

#include <iostream>
#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"
#include "internal/platform/implementation/windows/wifi_hotspot.h"

namespace nearby {
namespace windows {
namespace {

TEST(WifiHotspotMedium, DISABLED_StartWifiHotspot) {
  int run_test;
  NEARBY_LOGS(INFO) << "Run StartWifiHotspot test case? input 0 or 1:";
  std::cin >> run_test;

  if (run_test) {
    HotspotCredentials hotspot_credentials;
    WifiHotspotMedium hotspot_medium;

    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        platform::config_package_nearby::nearby_platform_feature::
            kEnableIntelPieSdk,
        true);

    EXPECT_TRUE(hotspot_medium.IsInterfaceValid());
    EXPECT_TRUE(hotspot_medium.StartWifiHotspot(&hotspot_credentials));

    while (true) {
      NEARBY_LOGS(INFO) << "Enter \"s\" to stop test:";
      std::string stop;
      std::cin >> stop;
      if (stop == "s") {
        NEARBY_LOGS(INFO) << "Exit WiFi Hotspot";
        EXPECT_TRUE(hotspot_medium.StopWifiHotspot());
        break;
      }
    }
  } else {
    NEARBY_LOGS(INFO) << "Skip the test";
  }
}

TEST(WifiHotspotMedium, DISABLED_WifiHotspotServerStartListen) {
  int run_test;
  NEARBY_LOGS(INFO) << "Run WifiHotspotServerStartListen test? input 0 or 1:";
  std::cin >> run_test;

  if (run_test) {
    HotspotCredentials hotspot_credentials;
    WifiHotspotMedium hotspot_medium;

    EXPECT_TRUE(hotspot_medium.IsInterfaceValid());
    EXPECT_TRUE(hotspot_medium.StartWifiHotspot(&hotspot_credentials));
    absl::SleepFor(absl::Seconds(1));
    std::unique_ptr<api::WifiHotspotServerSocket> server_socket =
        hotspot_medium.ListenForService(0);
    absl::SleepFor(absl::Seconds(10));
    std::unique_ptr<api::WifiHotspotSocket> client_socket =
        server_socket->Accept();

    while (true) {
      NEARBY_LOGS(INFO) << "Enter \"s\" to stop test:";
      std::string stop;
      std::cin >> stop;
      if (stop == "s") {
        NEARBY_LOGS(INFO) << "Close server socket and stop WiFi Hotspot";
        server_socket->Close();
        EXPECT_TRUE(hotspot_medium.StopWifiHotspot());
        break;
      }
    }
  } else {
    NEARBY_LOGS(INFO) << "Skip the test";
  }
}


TEST(WifiHotspotMedium, DISABLED_ConnectWifiHotspot) {
  int run_test;
  NEARBY_LOGS(INFO) << "Run ConnectWifiHotspot test case? input 0 or 1:";
  std::cin >> run_test;

  if (run_test) {
    HotspotCredentials hotspot_credentials;
    WifiHotspotMedium hotspot_medium;
    NEARBY_LOGS(INFO) << "Enter Network SSID to be connected: ";
    std::string ssid;
    std::cin >> ssid;
    NEARBY_LOGS(INFO) << "Enter password: ";
    std::string password;
    std::cin >> password;
    NEARBY_LOGS(INFO) << "Enter frequency(input 0 if unknown): ";
    int frequency;
    std::cin >> frequency;

    hotspot_credentials.SetSSID(ssid);
    hotspot_credentials.SetPassword(password);
    hotspot_credentials.SetFrequency(frequency);

    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        platform::config_package_nearby::nearby_platform_feature::
            kEnableIntelPieSdk,
        true);

    EXPECT_TRUE(hotspot_medium.ConnectWifiHotspot(&hotspot_credentials));
    absl::SleepFor(absl::Seconds(1));
    while (true) {
      NEARBY_LOGS(INFO) << "Enter \"s\" to stop test:";
      std::string stop;
      std::cin >> stop;
      if (stop == "s") {
        NEARBY_LOGS(INFO) << "Disconnect WiFi";
        EXPECT_TRUE(hotspot_medium.DisconnectWifiHotspot());
        break;
      }
    }
  } else {
    NEARBY_LOGS(INFO) << "Skip the test";
  }
}

}  // namespace
}  // namespace windows
}  // namespace nearby
