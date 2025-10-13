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

#include "internal/platform/implementation/windows/wifi_direct_service.h"

#include <iostream>
#include <string>

#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {
namespace {

TEST(WifiDirectServiceMedium, DISABLED_StartWifiDirectService) {
  int run_test;
  LOG(INFO) << "Run StartWifiDirectService test case? input 0 or 1:";
  std::cin >> run_test;

  if (run_test) {
    winrt::init_apartment();
    WifiDirectServiceMedium wifi_direct_service_medium;

    EXPECT_TRUE(wifi_direct_service_medium.StartWifiDirectService());

    while (true) {
      LOG(INFO) << "Enter \"s\" to stop test:";
      std::string stop;
      std::cin >> stop;
      if (stop == "s") {
        LOG(INFO) << "Exit WiFi WifiDirectService GO";
        EXPECT_TRUE(wifi_direct_service_medium.StopWifiDirectService());
        break;
      }
    }
  } else {
    LOG(INFO) << "Skip the test";
  }
}

TEST(WifiDirectServiceMedium, DISABLED_ConnectWifiDirectService) {
  int run_test;
  LOG(INFO) << "Run ConnectWifiDirectService GO test case? input 0 or 1:";
  std::cin >> run_test;

  if (run_test) {
    WifiDirectServiceMedium wifi_direct_service_medium;
    EXPECT_TRUE(wifi_direct_service_medium.ConnectWifiDirectService());

    absl::SleepFor(absl::Seconds(2));
    while (true) {
      LOG(INFO) << "Enter \"s\" to stop test:";
      std::string stop;
      std::cin >> stop;
      if (stop == "s") {
        LOG(INFO) << "Disconnect WiFiDirectService GO";
        EXPECT_TRUE(wifi_direct_service_medium.DisconnectWifiDirectService());
        break;
      }
    }
  } else {
    LOG(INFO) << "Skip the test";
  }
}
}  // namespace
}  // namespace windows
}  // namespace nearby
