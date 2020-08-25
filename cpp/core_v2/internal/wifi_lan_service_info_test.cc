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

#include "core_v2/internal/wifi_lan_service_info.h"

#include <cstring>
#include <memory>

#include "platform_v2/base/base64_utils.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

constexpr WifiLanServiceInfo::Version kVersion =
    WifiLanServiceInfo::Version::kV1;
constexpr Pcp kPcp = Pcp::kP2pCluster;
constexpr absl::string_view kEndPointID{"AB12"};
constexpr absl::string_view kServiceIDHashBytes{"\x0a\x0b\x0c"};
constexpr absl::string_view kEndPointName{"RAWK + ROWL!"};

TEST(WifiLanServiceInfoTest, ConstructionWorks) {
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo wifi_lan_service_info{
      kVersion, kPcp, kEndPointID, service_id_hash, endpoint_info};

  EXPECT_TRUE(wifi_lan_service_info.IsValid());
  EXPECT_EQ(kPcp, wifi_lan_service_info.GetPcp());
  EXPECT_EQ(kVersion, wifi_lan_service_info.GetVersion());
  EXPECT_EQ(kEndPointID, wifi_lan_service_info.GetEndpointId());
  EXPECT_EQ(service_id_hash, wifi_lan_service_info.GetServiceIdHash());
  EXPECT_EQ(endpoint_info, wifi_lan_service_info.GetEndpointInfo());
}

TEST(WifiLanServiceInfoTest, ConstructionFromSerializedStringWorks) {
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo org_wifi_lan_service_info{kVersion, kPcp, kEndPointID,
                                               service_id_hash, endpoint_info};
  std::string wifi_lan_service_info_string{org_wifi_lan_service_info};

  WifiLanServiceInfo wifi_lan_service_info{wifi_lan_service_info_string};

  EXPECT_TRUE(wifi_lan_service_info.IsValid());
  EXPECT_EQ(kPcp, wifi_lan_service_info.GetPcp());
  EXPECT_EQ(kVersion, wifi_lan_service_info.GetVersion());
  EXPECT_EQ(kEndPointID, wifi_lan_service_info.GetEndpointId());
  EXPECT_EQ(service_id_hash, wifi_lan_service_info.GetServiceIdHash());
  EXPECT_EQ(endpoint_info, wifi_lan_service_info.GetEndpointInfo());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithBadVersion) {
  auto bad_version = static_cast<WifiLanServiceInfo::Version>(666);

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo wifi_lan_service_info{bad_version, kPcp, kEndPointID,
                                           service_id_hash, endpoint_info};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithBadPCP) {
  auto bad_pcp = static_cast<Pcp>(666);

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo wifi_lan_service_info{kVersion, bad_pcp, kEndPointID,
                                           service_id_hash, endpoint_info};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithShortEndpointId) {
  std::string short_endpoint_id("AB1");

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo wifi_lan_service_info{kVersion, kPcp, short_endpoint_id,
                                           service_id_hash, endpoint_info};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithLongEndpointId) {
  std::string long_endpoint_id("AB12X");

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo wifi_lan_service_info{kVersion, kPcp, long_endpoint_id,
                                           service_id_hash, endpoint_info};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithShortServiceIdHash) {
  char short_service_id_hash_bytes[] = "\x0a\x0b";

  ByteArray short_service_id_hash{short_service_id_hash_bytes};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo wifi_lan_service_info{
      kVersion, kPcp, kEndPointID, short_service_id_hash, endpoint_info};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithLongServiceIdHash) {
  char long_service_id_hash_bytes[] = "\x0a\x0b\x0c\x0d";

  ByteArray long_service_id_hash{long_service_id_hash_bytes};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo wifi_lan_service_info{kVersion, kPcp, kEndPointID,
                                           long_service_id_hash, endpoint_info};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithShortStringLength) {
  char wifi_lan_service_info_string[] = {'X', '\0'};

  ByteArray wifi_lan_service_info_bytes{wifi_lan_service_info_string};
  WifiLanServiceInfo wifi_lan_service_info{
      Base64Utils::Encode(wifi_lan_service_info_bytes)};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
