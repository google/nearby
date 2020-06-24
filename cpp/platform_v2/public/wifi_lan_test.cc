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

#include "platform_v2/public/wifi_lan.h"

#include <memory>

#include "platform_v2/base/medium_environment.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace {

constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};

class WifiLanMediumTest : public ::testing::Test {
 protected:
  using DiscoveredServiceCallback = WifiLanMedium::DiscoveredServiceCallback;

  WifiLanMediumTest() { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(WifiLanMediumTest, ConstructorDestructorWorks) {
  env_.Start();
  WifiLanMedium medium_a;
  WifiLanMedium medium_b;

  // Make sure we can create functional mediums.
  ASSERT_TRUE(medium_a.IsValid());
  ASSERT_TRUE(medium_b.IsValid());

  // Make sure we can create 2 distinct mediums.
  EXPECT_NE(&medium_a.GetImpl(), &medium_b.GetImpl());
  env_.Stop();
}

TEST_F(WifiLanMediumTest, CanStartDiscoveryAndServiceIndeedDiscovered) {
  env_.Start();
  WifiLanMedium medium;
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  medium.StartDiscovery(std::string(kServiceID),
                        DiscoveredServiceCallback{
                            .service_discovered_cb =
                                [&found_latch](WifiLanService& service,
                                               const std::string& service_id) {
                                  NEARBY_LOG(INFO, "Service discovered: %s",
                                             service.GetName().c_str());
                                  EXPECT_EQ(kServiceID, service_id);
                                  found_latch.CountDown();
                                },
                            .service_lost_cb =
                                [&lost_latch](WifiLanService& service,
                                              const std::string& service_id) {
                                  NEARBY_LOG(INFO, "Service lost: %s",
                                             service.GetName().c_str());
                                  EXPECT_EQ(kServiceID, service_id);
                                  lost_latch.CountDown();
                                },
                        });
  EXPECT_TRUE(found_latch.Await(absl::Milliseconds(1000)).result());
  env_.Stop();
}

TEST_F(WifiLanMediumTest, CanStopDiscovery) {
  env_.Start();
  WifiLanMedium medium;
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  medium.StartDiscovery(std::string(kServiceID),
                        DiscoveredServiceCallback{
                            .service_discovered_cb =
                                [&found_latch](WifiLanService& service,
                                               const std::string& service_id) {
                                  NEARBY_LOG(INFO, "Service discovered: %s",
                                             service.GetName().c_str());
                                  EXPECT_EQ(kServiceID, service_id);
                                  found_latch.CountDown();
                                },
                            .service_lost_cb =
                                [&lost_latch](WifiLanService& service,
                                              const std::string& service_id) {
                                  NEARBY_LOG(INFO, "Service lost: %s",
                                             service.GetName().c_str());
                                  EXPECT_EQ(kServiceID, service_id);
                                  lost_latch.CountDown();
                                },
                        });
  EXPECT_TRUE(found_latch.Await(absl::Milliseconds(1000)).result());
  bool stop = medium.StopDiscovery(std::string(kServiceID));
  EXPECT_TRUE(stop);
  env_.Stop();
}

}  // namespace
}  // namespace nearby
}  // namespace location
