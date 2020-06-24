#include "core_v2/internal/mediums/wifi_lan.h"

#include <string>

#include "platform_v2/base/medium_environment.h"
#include "platform_v2/public/wifi_lan.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kServiceInfoName{
    "Simulated WifiLan service encrypted string #1"};

// TODO(edwinwu): Continue writing more tests after medium_environment is done.
class WifiLanTest : public ::testing::Test {
 protected:
  using DiscoveredServiceCallback = WifiLanMedium::DiscoveredServiceCallback;

  WifiLanTest() { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(WifiLanTest, CanConstructValidObject) {
  env_.Start();
  WifiLan wifi_lan_a;
  WifiLan wifi_lan_b;

  EXPECT_TRUE(wifi_lan_a.IsAvailable());
  EXPECT_TRUE(wifi_lan_b.IsAvailable());
  env_.Stop();
}

TEST_F(WifiLanTest, CanStartAdvertising) {
  env_.Start();
  WifiLan wifi_lan;
  EXPECT_TRUE(wifi_lan.StartAdvertising(std::string(kServiceID),
                                        std::string(kServiceInfoName)));
  env_.Stop();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
