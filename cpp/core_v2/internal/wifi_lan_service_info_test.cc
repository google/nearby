#include "core_v2/internal/wifi_lan_service_info.h"

#include <cstring>
#include <memory>

#include "platform_v2/base/base64_utils.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

const WifiLanServiceInfo::Version kVersion = WifiLanServiceInfo::Version::kV1;
const Pcp kPcp = Pcp::kP2pCluster;
const char kEndPointID[] = "AB12";
const char kServiceIDHashBytes[] = {0x0A, 0x0B, 0x0C};
// TODO(edwinwu): Temp to set empty string for endpoint_name.
const char kEndPointName[] = "";

TEST(WifiLanServiceInfoTest, ConstructionWorks) {
  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto wifi_lan_service_info = WifiLanServiceInfo(
      kVersion, kPcp, kEndPointID, service_id_hash, kEndPointName);
  auto is_valid = wifi_lan_service_info.IsValid();

  EXPECT_TRUE(is_valid);
  EXPECT_EQ(kPcp, wifi_lan_service_info.GetPcp());
  EXPECT_EQ(kVersion, wifi_lan_service_info.GetVersion());
  EXPECT_EQ(kEndPointID, wifi_lan_service_info.GetEndpointId());
  EXPECT_EQ(service_id_hash, wifi_lan_service_info.GetServiceIdHash());
}

TEST(WifiLanServiceInfoTest, ConstructionFromSerializedStringWorks) {
  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto org_wifi_lan_service_info = WifiLanServiceInfo(
      kVersion, kPcp, kEndPointID, service_id_hash, kEndPointName);
  auto wifi_lan_service_info_string = std::string(org_wifi_lan_service_info);

  auto wifi_lan_service_info = WifiLanServiceInfo(wifi_lan_service_info_string);
  auto is_valid = wifi_lan_service_info.IsValid();

  EXPECT_TRUE(is_valid);
  EXPECT_EQ(kPcp, wifi_lan_service_info.GetPcp());
  EXPECT_EQ(kVersion, wifi_lan_service_info.GetVersion());
  EXPECT_EQ(kEndPointID, wifi_lan_service_info.GetEndpointId());
  EXPECT_EQ(service_id_hash, wifi_lan_service_info.GetServiceIdHash());
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithBadVersion) {
  auto bad_version = static_cast<WifiLanServiceInfo::Version>(666);

  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto wifi_lan_service_info = WifiLanServiceInfo(
      bad_version, kPcp, kEndPointID, service_id_hash, kEndPointName);

  auto is_valid = wifi_lan_service_info.IsValid();

  EXPECT_FALSE(is_valid);
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithBadPCP) {
  auto bad_pcp = static_cast<Pcp>(666);

  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto wifi_lan_service_info = WifiLanServiceInfo(
      kVersion, bad_pcp, kEndPointID, service_id_hash, kEndPointName);
  auto is_valid = wifi_lan_service_info.IsValid();

  EXPECT_FALSE(is_valid);
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithShortEndpointId) {
  std::string short_endpoint_id("AB1");

  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto wifi_lan_service_info = WifiLanServiceInfo(
      kVersion, kPcp, short_endpoint_id, service_id_hash, kEndPointName);
  auto is_valid = wifi_lan_service_info.IsValid();

  EXPECT_FALSE(is_valid);
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithLongEndpointId) {
  std::string long_endpoint_id("AB12X");

  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto wifi_lan_service_info = WifiLanServiceInfo(
      kVersion, kPcp, long_endpoint_id, service_id_hash, kEndPointName);
  auto is_valid = wifi_lan_service_info.IsValid();

  EXPECT_FALSE(is_valid);
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithShortServiceIdHash) {
  char short_service_id_hash_bytes[] = {0x0A, 0x0B};

  auto short_service_id_hash =
      ByteArray(short_service_id_hash_bytes,
                sizeof(short_service_id_hash_bytes) / sizeof(char));
  auto wifi_lan_service_info = WifiLanServiceInfo(
      kVersion, kPcp, kEndPointID, short_service_id_hash, kEndPointName);
  auto is_valid = wifi_lan_service_info.IsValid();

  EXPECT_FALSE(is_valid);
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithLongServiceIdHash) {
  char long_service_id_hash_bytes[] = {0x0A, 0x0B, 0x0C, 0x0D};

  auto long_service_id_hash =
      ByteArray(long_service_id_hash_bytes,
                sizeof(long_service_id_hash_bytes) / sizeof(char));
  auto wifi_lan_service_info = WifiLanServiceInfo(
      kVersion, kPcp, kEndPointID, long_service_id_hash, kEndPointName);
  auto is_valid = wifi_lan_service_info.IsValid();

  EXPECT_FALSE(is_valid);
}

TEST(WifiLanServiceInfoTest, ConstructionFailsWithShortStringLength) {
  char wifi_lan_service_info_string[] = {'X'};

  auto wifi_lan_service_info_bytes =
      ByteArray(wifi_lan_service_info_string,
                sizeof(wifi_lan_service_info_string) / sizeof(char));
  auto wifi_lan_service_info =
      WifiLanServiceInfo(Base64Utils::Encode(wifi_lan_service_info_bytes));
  auto is_valid = wifi_lan_service_info.IsValid();

  EXPECT_FALSE(is_valid);
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
