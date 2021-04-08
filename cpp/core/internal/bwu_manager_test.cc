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

#include "core/internal/bwu_manager.h"

#include <string>

#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/mediums/mediums.h"
#include "core/internal/mediums/utils.h"
#include "core/internal/offline_frames.h"
#include "platform/public/system_clock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

TEST(BwuManagerTest, CanCreateInstance) {
  Mediums mediums;
  EndpointChannelManager ecm;
  EndpointManager em{&ecm};
  BwuManager bwu_manager{mediums, em, ecm, {}, {}};

  SystemClock::Sleep(absl::Seconds(3));

  bwu_manager.Shutdown();
}

TEST(BwuManagerTest, CanInitiateBwu) {
  ClientProxy client;
  std::string endpoint_id("EP_A");
  Mediums mediums;
  EndpointChannelManager ecm;
  EndpointManager em{&ecm};
  BwuManager bwu_manager{mediums, em, ecm, {}, {}};

  // Method returns void, so we just verify we did not SEGFAULT while calling.
  bwu_manager.InitiateBwuForEndpoint(&client, endpoint_id);
  SystemClock::Sleep(absl::Seconds(3));

  bwu_manager.Shutdown();
}

TEST(BwuManagerTest, CanProcessBandwidthUpgradeFrames) {
  ClientProxy client;
  std::string endpoint_id("EP_A");
  LocationHint location_hint = Utils::BuildLocationHint("US");
  Mediums mediums;
  EndpointChannelManager ecm;
  EndpointManager em{&ecm};
  BwuManager bwu_manager{mediums, em, ecm, {}, {}};

  ExceptionOr<OfflineFrame> path_available_frame = parser::FromBytes(
      parser::ForBwuWebrtcPathAvailable("my_id", location_hint));
  bwu_manager.OnIncomingFrame(path_available_frame.result(), endpoint_id,
                              &client, Medium::WEB_RTC);

  ExceptionOr<OfflineFrame> last_write_frame =
      parser::FromBytes(parser::ForBwuLastWrite());
  bwu_manager.OnIncomingFrame(last_write_frame.result(), endpoint_id, &client,
                              Medium::WEB_RTC);

  ExceptionOr<OfflineFrame> safe_to_close_frame =
      parser::FromBytes(parser::ForBwuSafeToClose());
  bwu_manager.OnIncomingFrame(safe_to_close_frame.result(), endpoint_id,
                              &client, Medium::WEB_RTC);

  bwu_manager.Shutdown();
}

TEST(BwuManagerTest, InitiateBwu_UpgradeFails_NoCrash) {
  ClientProxy client;
  std::string endpoint_id("EP_A");
  Mediums mediums;
  EndpointChannelManager ecm;
  EndpointManager em{&ecm};
  BwuManager bwu_manager{mediums, em, ecm, {}, {}};
  parser::UpgradePathInfo upgrade_path_info;
  upgrade_path_info.set_medium(parser::UpgradePathInfo::WEB_RTC);

  bwu_manager.InitiateBwuForEndpoint(&client, endpoint_id);
  ExceptionOr<OfflineFrame> bwu_failed_frame =
      parser::FromBytes(parser::ForBwuFailure(upgrade_path_info));
  bwu_manager.OnIncomingFrame(bwu_failed_frame.result(), endpoint_id, &client,
                              Medium::WEB_RTC);

  bwu_manager.Shutdown();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
