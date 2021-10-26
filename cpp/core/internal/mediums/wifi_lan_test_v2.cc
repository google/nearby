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

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "core/internal/mediums/wifi_lan_v2.h"
#include "platform/base/medium_environment.h"
#include "platform/public/logging.h"
#include "platform/public/wifi_lan_v2.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

using FeatureFlags = FeatureFlags::Flags;

constexpr FeatureFlags kTestCases[] = {
    FeatureFlags{
        .enable_cancellation_flag = true,
    },
    FeatureFlags{
        .enable_cancellation_flag = false,
    },
};

constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kServiceInfoName{
    "Simulated WifiLan service encrypted string #1"};
constexpr absl::string_view kEndpointName{"Simulated endpoint name"};
constexpr absl::string_view kEndpointInfoKey{"n"};

class WifiLanV2Test : public ::testing::TestWithParam<FeatureFlags> {
 protected:
  WifiLanV2Test() { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(WifiLanV2Test, CanConstructValidObject) {
  env_.Start();
  WifiLanV2 wifi_lan_a;
  WifiLanV2 wifi_lan_b;
  std::string service_id(kServiceID);

  EXPECT_TRUE(wifi_lan_a.IsAvailable());
  EXPECT_TRUE(wifi_lan_b.IsAvailable());
  env_.Stop();
}

TEST_F(WifiLanV2Test, CanStartAdvertising) {
  env_.Start();
  WifiLanV2 wifi_lan_a;
  std::string service_id(kServiceID);
  std::string service_info_name{kServiceInfoName};
  std::string endpoint_info_name{kEndpointName};

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  EXPECT_TRUE(wifi_lan_a.StartAdvertising(service_id, nsd_service_info));
  EXPECT_TRUE(wifi_lan_a.StopAdvertising(service_id));
  env_.Stop();
}

TEST_F(WifiLanV2Test, CanStartMultipleAdvertising) {
  env_.Start();
  WifiLanV2 wifi_lan_a;
  std::string service_id(kServiceID);
  std::string service_id_1(kServiceID);
  std::string service_id_2("com.google.location.nearby.apps.test_1");
  std::string service_info_name{kServiceInfoName};
  std::string endpoint_info_name{kEndpointName};

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  EXPECT_TRUE(wifi_lan_a.StartAdvertising(service_id_1, nsd_service_info));
  EXPECT_TRUE(wifi_lan_a.StartAdvertising(service_id_2, nsd_service_info));
  EXPECT_TRUE(wifi_lan_a.StopAdvertising(service_id_1));
  EXPECT_TRUE(wifi_lan_a.StopAdvertising(service_id_2));
  env_.Stop();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
