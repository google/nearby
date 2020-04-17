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

#include "core/internal/wifi_lan_service_info.h"

#include <cstring>

#include "platform/base64_utils.h"
#include "platform/port/string.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

const WifiLanServiceInfo::Version kVersion = WifiLanServiceInfo::Version::kV1;
const PCP::Value kPcp = PCP::P2P_CLUSTER;
const char kEndPointID[] = "AB12";
const char kServiceIDHashBytes[] = {0x0A, 0x0B, 0x0C};
// TODO(b/149806065): Implements test endpoint_name.

TEST(WifiLanServiceInfoTest, SerializationDeserializationWorks) {
  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      kServiceIDHashBytes, sizeof(kServiceIDHashBytes) / sizeof(char)));

  std::string wifi_lan_service_info_string = WifiLanServiceInfo::AsString(
      kVersion, kPcp, kEndPointID, ConstifyPtr(scoped_service_id_hash.get()));
  ScopedPtr<Ptr<WifiLanServiceInfo> > scoped_wifi_lan_service_info(
      WifiLanServiceInfo::FromString(wifi_lan_service_info_string));

  EXPECT_EQ(kPcp, scoped_wifi_lan_service_info->GetPcp());
  EXPECT_EQ(kVersion, scoped_wifi_lan_service_info->GetVersion());
  EXPECT_EQ(kEndPointID, scoped_wifi_lan_service_info->GetEndpointId());
  EXPECT_EQ(*scoped_service_id_hash,
            *(scoped_wifi_lan_service_info->GetServiceIdHash()));
}

TEST(WifiLanServiceInfoTest,
     SerializationDeserializationWorksWithEmptyEndpointName) {
  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      kServiceIDHashBytes, sizeof(kServiceIDHashBytes) / sizeof(char)));

  std::string wifi_lan_service_info_string = WifiLanServiceInfo::AsString(
      kVersion, kPcp, kEndPointID, ConstifyPtr(scoped_service_id_hash.get()));
  ScopedPtr<Ptr<WifiLanServiceInfo> > scoped_wifi_lan_service_info(
      WifiLanServiceInfo::FromString(wifi_lan_service_info_string));

  EXPECT_EQ(kPcp, scoped_wifi_lan_service_info->GetPcp());
  EXPECT_EQ(kVersion, scoped_wifi_lan_service_info->GetVersion());
  EXPECT_EQ(kEndPointID, scoped_wifi_lan_service_info->GetEndpointId());
  EXPECT_EQ(*scoped_service_id_hash,
            *(scoped_wifi_lan_service_info->GetServiceIdHash()));
}

TEST(WifiLanServiceInfoTest, SerializationFailsWithBadVersion) {
  WifiLanServiceInfo::Version bad_version =
      static_cast<WifiLanServiceInfo::Version>(666);

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      kServiceIDHashBytes, sizeof(kServiceIDHashBytes) / sizeof(char)));

  std::string wifi_lan_service_info_string =
      WifiLanServiceInfo::AsString(bad_version, kPcp, kEndPointID,
                                   ConstifyPtr(scoped_service_id_hash.get()));

  EXPECT_TRUE(wifi_lan_service_info_string.empty());
}

TEST(WifiLanServiceInfoTest, SerializationFailsWithBadPCP) {
  PCP::Value bad_pcp = static_cast<PCP::Value>(666);

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      kServiceIDHashBytes, sizeof(kServiceIDHashBytes) / sizeof(char)));

  std::string wifi_lan_service_info_string =
      WifiLanServiceInfo::AsString(kVersion, bad_pcp, kEndPointID,
                                   ConstifyPtr(scoped_service_id_hash.get()));

  EXPECT_TRUE(wifi_lan_service_info_string.empty());
}

TEST(WifiLanServiceInfoTest, SerializationFailsWithShortEndpointId) {
  std::string short_endpoint_id("AB1");

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      kServiceIDHashBytes, sizeof(kServiceIDHashBytes) / sizeof(char)));

  std::string wifi_lan_service_info_string =
      WifiLanServiceInfo::AsString(kVersion, kPcp, short_endpoint_id,
                                   ConstifyPtr(scoped_service_id_hash.get()));

  EXPECT_TRUE(wifi_lan_service_info_string.empty());
}

TEST(WifiLanServiceInfoTest, SerializationFailsWithLongEndpointId) {
  std::string long_endpoint_id("AB12X");

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      kServiceIDHashBytes, sizeof(kServiceIDHashBytes) / sizeof(char)));

  std::string wifi_lan_service_info_string =
      WifiLanServiceInfo::AsString(kVersion, kPcp, long_endpoint_id,
                                   ConstifyPtr(scoped_service_id_hash.get()));

  EXPECT_TRUE(wifi_lan_service_info_string.empty());
}

TEST(WifiLanServiceInfoTest, SerializationFailsWithShortServiceIdHash) {
  char short_service_id_hash_bytes[] = {0x0A, 0x0B};

  ScopedPtr<Ptr<ByteArray> > scoped_short_service_id_hash(
      new ByteArray(short_service_id_hash_bytes,
                    sizeof(short_service_id_hash_bytes) / sizeof(char)));

  std::string wifi_lan_service_info_string = WifiLanServiceInfo::AsString(
      kVersion, kPcp, kEndPointID,
      ConstifyPtr(scoped_short_service_id_hash.get()));

  EXPECT_TRUE(wifi_lan_service_info_string.empty());
}

TEST(WifiLanServiceInfoTest, SerializationFailsWithLongServiceIdHash) {
  char long_service_id_hash_bytes[] = {0x0A, 0x0B, 0x0C, 0x0D};

  ScopedPtr<Ptr<ByteArray> > scoped_long_service_id_hash(
      new ByteArray(long_service_id_hash_bytes,
                    sizeof(long_service_id_hash_bytes) / sizeof(char)));

  std::string wifi_lan_service_info_string = WifiLanServiceInfo::AsString(
      kVersion, kPcp, kEndPointID,
      ConstifyPtr(scoped_long_service_id_hash.get()));

  EXPECT_TRUE(wifi_lan_service_info_string.empty());
}

TEST(WifiLanServiceInfoTest, DeserializationFailsWithShortLength) {
  char wifi_lan_service_info_bytes[] = {'X'};

  ScopedPtr<Ptr<ByteArray> > scoped_wifi_lan_service_info_bytes(
      new ByteArray(wifi_lan_service_info_bytes,
                    sizeof(wifi_lan_service_info_bytes) / sizeof(char)));

  ScopedPtr<Ptr<WifiLanServiceInfo> > scoped_wifi_lan_service_info(
      WifiLanServiceInfo::FromString(Base64Utils::encode(
          ConstifyPtr(scoped_wifi_lan_service_info_bytes.get()))));

  EXPECT_TRUE(scoped_wifi_lan_service_info.isNull());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
