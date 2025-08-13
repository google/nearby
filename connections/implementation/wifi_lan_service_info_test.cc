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

#include "connections/implementation/wifi_lan_service_info.h"

#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/pcp.h"
#include "connections/implementation/webrtc_state.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/nsd_service_info.h"

namespace nearby {
namespace connections {
namespace {

constexpr WifiLanServiceInfo::Version kVersion =
    WifiLanServiceInfo::Version::kV1;
constexpr Pcp kPcp = Pcp::kP2pCluster;
constexpr absl::string_view kEndPointID{"AB12"};
constexpr absl::string_view kServiceIDHashBytes{"\x0a\x0b\x0c"};
constexpr absl::string_view kEndPointName{"RAWK + ROWL!"};
constexpr absl::string_view kUwbAddressBytes{
    "\x01\x02\x03\x04\xab\xcd\xef\xac"};
constexpr WebRtcState kWebRtcState = WebRtcState::kConnectable;

// TODO(b/169550050): Implement UWBAddress.
TEST(WifiLanServiceInfoTest, ConstructionWorks) {
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  ByteArray uwb_address{std::string(kUwbAddressBytes)};
  WifiLanServiceInfo wifi_lan_service_info{
      kVersion,      kPcp,        kEndPointID, service_id_hash,
      endpoint_info, {uwb_address}, kWebRtcState};

  EXPECT_TRUE(wifi_lan_service_info.IsValid());
  EXPECT_EQ(kPcp, wifi_lan_service_info.GetPcp());
  EXPECT_EQ(kVersion, wifi_lan_service_info.GetVersion());
  EXPECT_EQ(kEndPointID, wifi_lan_service_info.GetEndpointId());
  EXPECT_EQ(service_id_hash, wifi_lan_service_info.GetServiceIdHash());
  EXPECT_EQ(endpoint_info, wifi_lan_service_info.GetEndpointInfo());
  EXPECT_EQ(uwb_address, wifi_lan_service_info.GetUwbAddress());
  EXPECT_EQ(kWebRtcState, wifi_lan_service_info.GetWebRtcState());
}

TEST(WifiLanServiceInfoTest, ConstructionFromSerializedStringWorks) {
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  ByteArray uwb_address{std::string(kUwbAddressBytes)};
  WifiLanServiceInfo org_wifi_lan_service_info{
      kVersion,      kPcp,        kEndPointID, service_id_hash,
      endpoint_info, uwb_address, kWebRtcState};
  NsdServiceInfo nsd_service_info{org_wifi_lan_service_info};

  WifiLanServiceInfo wifi_lan_service_info{nsd_service_info};

  EXPECT_TRUE(wifi_lan_service_info.IsValid());
  EXPECT_EQ(kPcp, wifi_lan_service_info.GetPcp());
  EXPECT_EQ(kVersion, wifi_lan_service_info.GetVersion());
  EXPECT_EQ(kEndPointID, wifi_lan_service_info.GetEndpointId());
  EXPECT_EQ(service_id_hash, wifi_lan_service_info.GetServiceIdHash());
  EXPECT_EQ(endpoint_info, wifi_lan_service_info.GetEndpointInfo());
  EXPECT_EQ(uwb_address, wifi_lan_service_info.GetUwbAddress());
  EXPECT_EQ(kWebRtcState, wifi_lan_service_info.GetWebRtcState());
}

TEST(WifiLanServiceInfoTest,
     ConstructionFromSerializedStringWorksNoUwbAddress) {
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo org_wifi_lan_service_info{
      kVersion,      kPcp,        kEndPointID, service_id_hash,
      endpoint_info, {}, kWebRtcState};
  NsdServiceInfo nsd_service_info{org_wifi_lan_service_info};

  WifiLanServiceInfo wifi_lan_service_info{nsd_service_info};

  EXPECT_TRUE(wifi_lan_service_info.IsValid());
  EXPECT_EQ(kPcp, wifi_lan_service_info.GetPcp());
  EXPECT_EQ(kVersion, wifi_lan_service_info.GetVersion());
  EXPECT_EQ(kEndPointID, wifi_lan_service_info.GetEndpointId());
  EXPECT_EQ(service_id_hash, wifi_lan_service_info.GetServiceIdHash());
  EXPECT_EQ(endpoint_info, wifi_lan_service_info.GetEndpointInfo());
  EXPECT_EQ(kWebRtcState, wifi_lan_service_info.GetWebRtcState());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithBadVersion) {
  auto bad_version = static_cast<WifiLanServiceInfo::Version>(666);

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo wifi_lan_service_info{
      bad_version,   kPcp,        kEndPointID, service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithBadPCP) {
  auto bad_pcp = static_cast<Pcp>(666);

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo wifi_lan_service_info{
      kVersion,      bad_pcp,     kEndPointID, service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithShortEndpointId) {
  std::string short_endpoint_id("AB1");

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo wifi_lan_service_info{
      kVersion,      kPcp,        short_endpoint_id, service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithLongEndpointId) {
  std::string long_endpoint_id("AB12X");

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo wifi_lan_service_info{
      kVersion,      kPcp,        long_endpoint_id, service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithShortServiceIdHash) {
  char short_service_id_hash_bytes[] = "\x0a\x0b";

  ByteArray short_service_id_hash{short_service_id_hash_bytes};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo wifi_lan_service_info{
      kVersion,      kPcp,        kEndPointID, short_service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithLongServiceIdHash) {
  char long_service_id_hash_bytes[] = "\x0a\x0b\x0c\x0d";

  ByteArray long_service_id_hash{long_service_id_hash_bytes};
  ByteArray endpoint_info{std::string(kEndPointName)};
  WifiLanServiceInfo wifi_lan_service_info{
      kVersion,      kPcp,        kEndPointID, long_service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithShortServiceNameLength) {
  char wifi_lan_service_info_name[] = {'X', '\0'};
  ByteArray wifi_lan_service_info_bytes{wifi_lan_service_info_name};

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(
      Base64Utils::Encode(wifi_lan_service_info_bytes));

  WifiLanServiceInfo wifi_lan_service_info{nsd_service_info};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithLongEndpointInfoLength) {
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray long_endpoint_info(WifiLanServiceInfo::kMaxEndpointInfoLength + 1);

  WifiLanServiceInfo wifi_lan_service_info{
      kVersion,           kPcp,        kEndPointID, service_id_hash,
      long_endpoint_info, ByteArray{}, kWebRtcState};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}


TEST(WifiLanServiceInfoTest, ConstructionBadEndPointInfo) {
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  ByteArray uwb_address{std::string(kUwbAddressBytes)};
  WifiLanServiceInfo org_wifi_lan_service_info{
      kVersion,      kPcp,        kEndPointID, service_id_hash,
      endpoint_info, uwb_address, kWebRtcState};
  NsdServiceInfo nsd_service_info{org_wifi_lan_service_info};

  // Create a ByteArray larger than the allowed size.
  ByteArray bad_endpoint_info(WifiLanServiceInfo::kMaxEndpointInfoLength + 2);
  // Base64 encode the oversized ByteArray.
  std::string encoded_bad_endpoint_info =
      Base64Utils::Encode(bad_endpoint_info);
  // Set the TXT record with the Base64 encoded bad endpoint info.
  nsd_service_info.SetTxtRecord(
      std::string(WifiLanServiceInfo::kKeyEndpointInfo),
      encoded_bad_endpoint_info);

  WifiLanServiceInfo wifi_lan_service_info{nsd_service_info};

  EXPECT_FALSE(wifi_lan_service_info.IsValid());
}


TEST(WifiLanServiceInfoTest, ConstructionBadServiceName) {
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  ByteArray uwb_address{std::string(kUwbAddressBytes)};
  WifiLanServiceInfo org_wifi_lan_service_info{
      kVersion,      kPcp,        kEndPointID, service_id_hash,
      endpoint_info, uwb_address, kWebRtcState};

  NsdServiceInfo nsd_service_info_1{org_wifi_lan_service_info};
  nsd_service_info_1.SetServiceName("");
  WifiLanServiceInfo wifi_lan_service_info_1{nsd_service_info_1};
  EXPECT_FALSE(wifi_lan_service_info_1.IsValid());

  NsdServiceInfo nsd_service_info_2{org_wifi_lan_service_info};
  // ServiceName must be at least 9 bytes long, purposely set it to 4 bytes.
  nsd_service_info_2.SetServiceName("AB12");
  WifiLanServiceInfo wifi_lan_service_info_2{nsd_service_info_2};
  EXPECT_FALSE(wifi_lan_service_info_2.IsValid());
}


TEST(WifiLanServiceInfoTest, ConstructionBadFields) {
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  ByteArray uwb_address{std::string(kUwbAddressBytes)};
  WifiLanServiceInfo org_wifi_lan_service_info{
      kVersion,      kPcp,        kEndPointID, service_id_hash,
      endpoint_info, uwb_address, kWebRtcState};

  NsdServiceInfo nsd_service_info_1{org_wifi_lan_service_info};
  auto service_name = Base64Utils::Decode(nsd_service_info_1.GetServiceName());
  // Set Version to wrong value.
  if (!service_name.Empty()) {
    char* service_name_ptr = service_name.data();
    service_name_ptr[0] = 0x42;
  }
  nsd_service_info_1.SetServiceName(Base64Utils::Encode(service_name));
  WifiLanServiceInfo wifi_lan_service_info_1{nsd_service_info_1};
  EXPECT_FALSE(wifi_lan_service_info_1.IsValid());

  NsdServiceInfo nsd_service_info_2{org_wifi_lan_service_info};
  service_name = Base64Utils::Decode(nsd_service_info_2.GetServiceName());
  // Set Pcp to wrong value.
  if (!service_name.Empty()) {
    char* service_name_ptr = service_name.data();
    service_name_ptr[0] = 0x26;
  }
  nsd_service_info_2 .SetServiceName(Base64Utils::Encode(service_name));
  WifiLanServiceInfo wifi_lan_service_info_2{nsd_service_info_2};
  EXPECT_TRUE(wifi_lan_service_info_2.IsValid());

  NsdServiceInfo nsd_service_info_3{org_wifi_lan_service_info};
  service_name = Base64Utils::Decode(nsd_service_info_3.GetServiceName());
  // Set service name to shorter value to remove the UWB address field.
  if (!service_name.Empty()) {
    service_name.resize(9);
  }
  nsd_service_info_3.SetServiceName(Base64Utils::Encode(service_name));
  WifiLanServiceInfo wifi_lan_service_info_3{nsd_service_info_3};
  EXPECT_FALSE(wifi_lan_service_info_3.IsValid());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
