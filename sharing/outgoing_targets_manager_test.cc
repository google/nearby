// Copyright 2025 Google LLC
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

#include "sharing/outgoing_targets_manager.h"

#include <optional>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/fake_nearby_connections_manager.h"
#include "sharing/outgoing_share_session.h"
#include "sharing/share_target.h"
#include "sharing/transfer_metadata.h"

namespace nearby::sharing {
namespace {
using ::absl::Seconds;
using ::testing::InSequence;

class OutgoingTargetsManagerTest : public ::testing::Test {
 protected:
  OutgoingTargetsManagerTest()
      : service_thread_(&clock_, /*count=*/1),
        analytics_recorder_(/*vendor_id=*/0,
                            /*event_logger=*/nullptr),
        outgoing_targets_manager_(
            &clock_, &service_thread_, &connections_manager_,
            &analytics_recorder_,
            share_target_discovered_callback_.AsStdFunction(),
            share_target_updated_callback_.AsStdFunction(),
            share_target_lost_callback_.AsStdFunction(),
            transfer_update_callback_.AsStdFunction()) {}

  FakeClock clock_;
  FakeTaskRunner service_thread_;
  FakeNearbyConnectionsManager connections_manager_;
  analytics::AnalyticsRecorder analytics_recorder_;
  testing::MockFunction<void(const ShareTarget&)>
      share_target_discovered_callback_;
  testing::MockFunction<void(const ShareTarget&)>
      share_target_updated_callback_;
  testing::MockFunction<void(const ShareTarget&)> share_target_lost_callback_;
  testing::MockFunction<void(OutgoingShareSession&, const TransferMetadata&)>
      transfer_update_callback_;
  OutgoingTargetsManager outgoing_targets_manager_;
};

TEST_F(OutgoingTargetsManagerTest, onShareTargetDiscoveredNewTarget) {
  constexpr int kShareTargetId = 1234;
  constexpr absl::string_view kEndpointId = "endpoint_id";
  ShareTarget target;
  target.id = kShareTargetId;
  EXPECT_CALL(share_target_discovered_callback_, Call).Times(1);
  EXPECT_CALL(share_target_updated_callback_, Call).Times(0);
  EXPECT_CALL(share_target_lost_callback_, Call).Times(0);
  EXPECT_CALL(transfer_update_callback_, Call).Times(0);

  outgoing_targets_manager_.OnShareTargetDiscovered(
      target, kEndpointId, /*certificate=*/std::nullopt);

  OutgoingShareSession* session =
      outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId);
  EXPECT_NE(session, nullptr);
  EXPECT_EQ(session->share_target(), target);
  EXPECT_EQ(session->endpoint_id(), kEndpointId);

  bool has_targets = false;
  outgoing_targets_manager_.ForEachShareTarget(
      [&](const ShareTarget& share_target) {
        has_targets = true;
        EXPECT_EQ(share_target, target);
      });
  EXPECT_TRUE(has_targets);
}

TEST_F(OutgoingTargetsManagerTest, onShareTargetLost) {
  constexpr int kShareTargetId = 1234;
  constexpr absl::string_view kEndpointId = "endpoint_id";
  ShareTarget target;
  target.id = kShareTargetId;
  ShareTarget disabled_target = target;
  disabled_target.receive_disabled = true;
  {
    InSequence s;
    EXPECT_CALL(share_target_discovered_callback_, Call).Times(1);
    EXPECT_CALL(share_target_updated_callback_, Call)
        .WillOnce([&](const ShareTarget& share_target) {
          EXPECT_EQ(share_target, disabled_target);
        });
    EXPECT_CALL(share_target_lost_callback_, Call).Times(1);
    EXPECT_CALL(transfer_update_callback_, Call).Times(0);
  }
  outgoing_targets_manager_.OnShareTargetDiscovered(
      target, kEndpointId, /*certificate=*/std::nullopt);
  EXPECT_NE(outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId),
            nullptr);

  outgoing_targets_manager_.OnShareTargetLost(std::string(kEndpointId),
                                              Seconds(10));

  bool has_targets = false;
  outgoing_targets_manager_.ForEachShareTarget(
      [&](const ShareTarget& share_target) {
        has_targets = true;
        EXPECT_EQ(share_target, disabled_target);
      });
  EXPECT_TRUE(has_targets);
  EXPECT_EQ(outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId),
            nullptr);

  // Retention timer expired.
  clock_.FastForward(Seconds(10));
  service_thread_.Sync();

  has_targets = false;
  outgoing_targets_manager_.ForEachShareTarget(
      [&](const ShareTarget& share_target) { has_targets = true; });
  EXPECT_FALSE(has_targets);
}

TEST_F(OutgoingTargetsManagerTest, onShareTargeDedupNoLossByEndpointId) {
  constexpr int kShareTargetId = 1234;
  constexpr int kShareTargetId2 = kShareTargetId + 100;
  constexpr absl::string_view kEndpointId = "endpoint_id";
  ShareTarget target;
  target.id = kShareTargetId;
  target.device_name = "device_name";
  ShareTarget target2 = target;
  target2.id = kShareTargetId2;
  target2.device_name = "device_name_2";
  ShareTarget merged_target = target;
  merged_target.device_name = "device_name_2";
  {
    InSequence s;
    EXPECT_CALL(share_target_discovered_callback_, Call).Times(1);
    EXPECT_CALL(share_target_updated_callback_, Call)
        .WillOnce([&](const ShareTarget& share_target) {
          EXPECT_EQ(share_target, merged_target);
        });
    EXPECT_CALL(share_target_lost_callback_, Call).Times(0);
    EXPECT_CALL(transfer_update_callback_, Call).Times(0);
  }
  outgoing_targets_manager_.OnShareTargetDiscovered(
      target, kEndpointId, /*certificate=*/std::nullopt);
  EXPECT_NE(outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId),
            nullptr);

  outgoing_targets_manager_.OnShareTargetDiscovered(
      target2, kEndpointId, /*certificate=*/std::nullopt);

  // Make sure share session is not created for new target id.
  EXPECT_EQ(outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId2),
            nullptr);
  // Make sure share session is created for original target id.
  OutgoingShareSession* session2 =
      outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId);
  EXPECT_NE(session2, nullptr);
  EXPECT_EQ(session2->share_target(), merged_target);
  EXPECT_EQ(session2->endpoint_id(), kEndpointId);
  bool has_targets = false;
  outgoing_targets_manager_.ForEachShareTarget(
      [&](const ShareTarget& share_target) {
        has_targets = true;
        EXPECT_EQ(share_target, merged_target);
      });
  EXPECT_TRUE(has_targets);
}

TEST_F(OutgoingTargetsManagerTest, onShareTargeDedupNoLossByDeviceId) {
  constexpr int kShareTargetId = 1234;
  constexpr int kShareTargetId2 = kShareTargetId + 100;
  constexpr absl::string_view kEndpointId1 = "endpoint_id";
  constexpr absl::string_view kEndpointId2 = "endpoint_id_2";
  ShareTarget target;
  target.id = kShareTargetId;
  target.device_id = "device_id";
  target.device_name = "device_name";
  ShareTarget target2 = target;
  target2.id = kShareTargetId2;
  target2.device_name = "device_name_2";
  ShareTarget merged_target = target;
  merged_target.device_name = "device_name_2";
  {
    InSequence s;
    EXPECT_CALL(share_target_discovered_callback_, Call).Times(1);
    EXPECT_CALL(share_target_updated_callback_, Call)
        .WillOnce([&](const ShareTarget& share_target) {
          EXPECT_EQ(share_target, merged_target);
        });
    EXPECT_CALL(share_target_lost_callback_, Call).Times(0);
    EXPECT_CALL(transfer_update_callback_, Call).Times(0);
  }
  outgoing_targets_manager_.OnShareTargetDiscovered(
      target, kEndpointId1, /*certificate=*/std::nullopt);
  EXPECT_NE(outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId),
            nullptr);

  // Discover a new target with the same device id, but different endpoint id.
  outgoing_targets_manager_.OnShareTargetDiscovered(
      target2, kEndpointId2, /*certificate=*/std::nullopt);

  // Make sure share session is not created for new target id.
  EXPECT_EQ(outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId2),
            nullptr);
  // Make sure share session is created for original target id.
  OutgoingShareSession* session2 =
      outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId);
  EXPECT_NE(session2, nullptr);
  EXPECT_EQ(session2->share_target(), merged_target);
  EXPECT_EQ(session2->endpoint_id(), kEndpointId2);
  bool has_targets = false;
  outgoing_targets_manager_.ForEachShareTarget(
      [&](const ShareTarget& share_target) {
        has_targets = true;
        EXPECT_EQ(share_target, merged_target);
      });
  EXPECT_TRUE(has_targets);
}

TEST_F(OutgoingTargetsManagerTest, onShareTargeDedupByEndpointId) {
  constexpr int kShareTargetId = 1234;
  constexpr int kShareTargetId2 = kShareTargetId + 100;
  constexpr absl::string_view kEndpointId = "endpoint_id";
  ShareTarget target;
  target.id = kShareTargetId;
  target.device_name = "device_name";
  ShareTarget disabled_target = target;
  disabled_target.receive_disabled = true;
  ShareTarget target2 = target;
  target2.id = kShareTargetId2;
  target2.device_name = "device_name_2";
  ShareTarget merged_target = target;
  merged_target.device_name = "device_name_2";
  {
    InSequence s;
    EXPECT_CALL(share_target_discovered_callback_, Call).Times(1);
    EXPECT_CALL(share_target_updated_callback_, Call)
        .WillOnce([&](const ShareTarget& share_target) {
          EXPECT_EQ(share_target, disabled_target);
        })
        .WillOnce([&](const ShareTarget& share_target) {
          EXPECT_EQ(share_target, merged_target);
        });
    EXPECT_CALL(share_target_lost_callback_, Call).Times(0);
    EXPECT_CALL(transfer_update_callback_, Call).Times(0);
  }
  outgoing_targets_manager_.OnShareTargetDiscovered(
      target, kEndpointId, /*certificate=*/std::nullopt);
  EXPECT_NE(outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId),
            nullptr);
  outgoing_targets_manager_.OnShareTargetLost(std::string(kEndpointId),
                                              Seconds(10));
  EXPECT_EQ(outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId),
            nullptr);

  outgoing_targets_manager_.OnShareTargetDiscovered(
      target2, kEndpointId, /*certificate=*/std::nullopt);

  // Make sure share session is not created for new target id.
  EXPECT_EQ(outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId2),
            nullptr);
  // Make sure share session is created for original target id.
  OutgoingShareSession* session2 =
      outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId);
  EXPECT_NE(session2, nullptr);
  EXPECT_EQ(session2->share_target(), merged_target);
  EXPECT_EQ(session2->endpoint_id(), kEndpointId);
  bool has_targets = false;
  outgoing_targets_manager_.ForEachShareTarget(
      [&](const ShareTarget& share_target) {
        has_targets = true;
        EXPECT_EQ(share_target, merged_target);
      });
  EXPECT_TRUE(has_targets);

  // Retention timer expired.
  clock_.FastForward(Seconds(10));
  service_thread_.Sync();

  // Make sure share session is not removed after retention timer expired.
  session2 = outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId);
  EXPECT_NE(session2, nullptr);
  EXPECT_EQ(session2->share_target(), merged_target);
  EXPECT_EQ(session2->endpoint_id(), kEndpointId);
  has_targets = false;
  outgoing_targets_manager_.ForEachShareTarget(
      [&](const ShareTarget& share_target) {
        has_targets = true;
        EXPECT_EQ(share_target, merged_target);
      });
  EXPECT_TRUE(has_targets);
}

TEST_F(OutgoingTargetsManagerTest, onShareTargeDedupByDeviceId) {
  constexpr int kShareTargetId = 1234;
  constexpr int kShareTargetId2 = kShareTargetId + 100;
  constexpr absl::string_view kEndpointId1 = "endpoint_id";
  constexpr absl::string_view kEndpointId2 = "endpoint_id_2";
  ShareTarget target;
  target.id = kShareTargetId;
  target.device_id = "device_id";
  target.device_name = "device_name";
  ShareTarget disabled_target = target;
  disabled_target.receive_disabled = true;
  ShareTarget target2 = target;
  target2.id = kShareTargetId2;
  target2.device_name = "device_name_2";
  ShareTarget merged_target = target;
  merged_target.device_name = "device_name_2";
  {
    InSequence s;
    EXPECT_CALL(share_target_discovered_callback_, Call).Times(1);
    EXPECT_CALL(share_target_updated_callback_, Call)
        .WillOnce([&](const ShareTarget& share_target) {
          EXPECT_EQ(share_target, disabled_target);
        })
        .WillOnce([&](const ShareTarget& share_target) {
          EXPECT_EQ(share_target, merged_target);
        });
    EXPECT_CALL(share_target_lost_callback_, Call).Times(0);
    EXPECT_CALL(transfer_update_callback_, Call).Times(0);
  }
  outgoing_targets_manager_.OnShareTargetDiscovered(
      target, kEndpointId1, /*certificate=*/std::nullopt);
  EXPECT_NE(outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId),
            nullptr);
  outgoing_targets_manager_.OnShareTargetLost(std::string(kEndpointId1),
                                              Seconds(10));
  EXPECT_EQ(outgoing_targets_manager_.GetOutgoingShareSession(1234), nullptr);

  // Discover a new target with the same device id, but different endpoint id.
  outgoing_targets_manager_.OnShareTargetDiscovered(
      target2, kEndpointId2, /*certificate=*/std::nullopt);

  // Make sure share session is not created for new target id.
  EXPECT_EQ(outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId2),
            nullptr);
  // Make sure share session is created for original target id.
  OutgoingShareSession* session2 =
      outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId);
  EXPECT_NE(session2, nullptr);
  EXPECT_EQ(session2->share_target(), merged_target);
  EXPECT_EQ(session2->endpoint_id(), kEndpointId2);
  bool has_targets = false;
  outgoing_targets_manager_.ForEachShareTarget(
      [&](const ShareTarget& share_target) {
        has_targets = true;
        EXPECT_EQ(share_target, merged_target);
      });
  EXPECT_TRUE(has_targets);

  // Retention timer expired.
  clock_.FastForward(Seconds(10));
  service_thread_.Sync();

  // Make sure share session is not removed after retention timer expired.
  session2 = outgoing_targets_manager_.GetOutgoingShareSession(kShareTargetId);
  EXPECT_NE(session2, nullptr);
  EXPECT_EQ(session2->share_target(), merged_target);
  EXPECT_EQ(session2->endpoint_id(), kEndpointId2);
  has_targets = false;
  outgoing_targets_manager_.ForEachShareTarget(
      [&](const ShareTarget& share_target) {
        has_targets = true;
        EXPECT_EQ(share_target, merged_target);
      });
  EXPECT_TRUE(has_targets);
}

}  // namespace
}  // namespace nearby::sharing
