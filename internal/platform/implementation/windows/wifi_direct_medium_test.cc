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

#include "internal/platform/implementation/windows/wifi_direct.h"

#include <optional>

#include "gtest/gtest.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/wifi_credential.h"

namespace nearby::windows {
namespace {

// NOLINTNEXTLINE(misc-include-cleaner)
using ::winrt::Windows::Devices::WiFiDirect::WiFiDirectConfigurationMethod;

class WifiDirectMediumTest : public ::testing::Test {
 protected:
  void TearDown() override {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        platform::config_package_nearby::nearby_platform_feature::
            kEnableWifiDirectEmptyPin,
        true);
  }
};

TEST_F(WifiDirectMediumTest,
       ConnectWifiDirectAllowsEmptyPinWhenSsidAndPasswordArePresent) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      platform::config_package_nearby::nearby_platform_feature::
          kEnableWifiDirectEmptyPin,
      true);
  std::optional<WiFiDirectConfigurationMethod> configured_method;
  WifiDirectMedium medium([&configured_method](
                              WiFiDirectConfigurationMethod method) {
    configured_method = method;
    return true;
  });

  WifiDirectCredentials credentials;
  credentials.SetDeviceName("DIRECT-ab-TestDevice");
  credentials.SetSSID("DIRECT-ab-TestDevice");
  credentials.SetPassword("test_password_123");
  credentials.SetPin("");

  EXPECT_TRUE(medium.ConnectWifiDirect(credentials));
  ASSERT_TRUE(configured_method.has_value());
  EXPECT_EQ(*configured_method, WiFiDirectConfigurationMethod::PushButton);
}

TEST_F(WifiDirectMediumTest, ConnectWifiDirectUsesProvidePinWhenPinIsPresent) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      platform::config_package_nearby::nearby_platform_feature::
          kEnableWifiDirectEmptyPin,
      true);
  std::optional<WiFiDirectConfigurationMethod> configured_method;
  WifiDirectMedium medium([&configured_method](
                              WiFiDirectConfigurationMethod method) {
    configured_method = method;
    return true;
  });

  WifiDirectCredentials credentials;
  credentials.SetDeviceName("DIRECT-ab-TestDevice");
  credentials.SetSSID("DIRECT-ab-TestDevice");
  credentials.SetPassword("test_password_123");
  credentials.SetPin("12345678");

  EXPECT_TRUE(medium.ConnectWifiDirect(credentials));
  ASSERT_TRUE(configured_method.has_value());
  EXPECT_EQ(*configured_method, WiFiDirectConfigurationMethod::ProvidePin);
}

TEST_F(WifiDirectMediumTest,
       ConnectWifiDirectRejectsEmptyPinWhenSsidOrPasswordIsMissing) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      platform::config_package_nearby::nearby_platform_feature::
          kEnableWifiDirectEmptyPin,
      true);
  std::optional<WiFiDirectConfigurationMethod> configured_method;
  WifiDirectMedium medium([&configured_method](
                              WiFiDirectConfigurationMethod method) {
    configured_method = method;
    return true;
  });

  WifiDirectCredentials credentials;
  credentials.SetDeviceName("DIRECT-ab-TestDevice");
  credentials.SetSSID("DIRECT-ab-TestDevice");
  credentials.SetPassword("");
  credentials.SetPin("");

  EXPECT_FALSE(medium.ConnectWifiDirect(credentials));
  EXPECT_FALSE(configured_method.has_value());
}

TEST_F(WifiDirectMediumTest,
       ConnectWifiDirectRejectsEmptyPinWhenFlagIsDisabled) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      platform::config_package_nearby::nearby_platform_feature::
          kEnableWifiDirectEmptyPin,
      false);
  std::optional<WiFiDirectConfigurationMethod> configured_method;
  WifiDirectMedium medium([&configured_method](
                              WiFiDirectConfigurationMethod method) {
    configured_method = method;
    return true;
  });

  WifiDirectCredentials credentials;
  credentials.SetDeviceName("DIRECT-ab-TestDevice");
  credentials.SetSSID("DIRECT-ab-TestDevice");
  credentials.SetPassword("test_password_123");
  credentials.SetPin("");

  EXPECT_FALSE(medium.ConnectWifiDirect(credentials));
  EXPECT_FALSE(configured_method.has_value());
}

}  // namespace
}  // namespace nearby::windows
