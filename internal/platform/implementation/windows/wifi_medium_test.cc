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

#include "internal/platform/implementation/windows/wifi.h"

#include <iostream>

#include "gtest/gtest.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {
namespace {

TEST(WifiMedium, DISABLED_GetCapabilityAndInformation) {
  int run_test;
  NEARBY_LOGS(INFO)
      << "Run GetCapabilityAndInformation test case? input 0 or 1:";
  std::cin >> run_test;

  if (run_test) {
    WifiMedium wifi_medium;

    auto& capability = wifi_medium.GetCapability();
    NEARBY_LOGS(INFO) << "Support 5G? " << capability.supports_5_ghz;

    auto& information = wifi_medium.GetInformation();
    NEARBY_LOGS(INFO) << "Is Connected? " << information.is_connected
                      << "; ssid = " << information.ssid
                      << "; bssid = " << information.bssid
                      << "; ap_frequency: " << information.ap_frequency
                      << "; ip_address_dot_decimal: "
                      << information.ip_address_dot_decimal
                      << "; ip_address_4_bytes: "
                      << information.ip_address_4_bytes;
  } else {
    NEARBY_LOGS(INFO) << "Skip the test";
  }
}

}  // namespace
}  // namespace windows
}  // namespace nearby
