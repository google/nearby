// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>
#include <optional>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "presence/fpp/sensor_fusion_impl.h"

namespace nearby {
namespace presence {
namespace {
constexpr uint64_t kDeviceId = 1234;
constexpr int kReachRssi = -40;

TEST(SensorFusion, RequestZoneTransitionUpdatesSuccess) {
  SensorFusionImpl sensor_fusion_impl;
  bool callback_called = false;
  bool callback2_called = false;
  sensor_fusion_impl.RequestZoneTransitionUpdates(
      {.on_callback_id_generated = [&callback_called](uint64_t callback_id) {
        callback_called = true;
        EXPECT_EQ(callback_id, 1);
      }});
  sensor_fusion_impl.RequestZoneTransitionUpdates(
      {.on_callback_id_generated = [&callback2_called](uint64_t callback_id2) {
        callback2_called = true;
        EXPECT_EQ(callback_id2, 2);
      }});
  EXPECT_TRUE(callback2_called);
}

TEST(SensorFusion, RemoveZoneTransitionUpdates) {
  SensorFusionImpl sensor_fusion_impl;
  bool callback_called = false;
  sensor_fusion_impl.RequestZoneTransitionUpdates(
      {.on_callback_id_generated = [&callback_called](uint64_t callback_id) {
        callback_called = true;
        EXPECT_EQ(callback_id, 1);
      }});
  sensor_fusion_impl.RemoveZoneTransitionUpdates(1);
  EXPECT_EQ(
      sensor_fusion_impl
          .UpdateBleScanResult(kDeviceId, /*txPower=*/std::nullopt, kReachRssi,
                               /*elapsed_real_time_millis=*/0)
          .code(),
      absl::StatusCode::kInternal);
}

TEST(SensorFusion, UpdateBleScanResult) {
  SensorFusionImpl sensor_fusion_impl;
  bool proximity_zone_changed_called = false;
  sensor_fusion_impl.RequestZoneTransitionUpdates(
      {.on_proximity_zone_changed =
           [&proximity_zone_changed_called](
               uint64_t device_id,
               PresenceZone::DistanceBoundary::RangeType range_type) {
             proximity_zone_changed_called = true;
           }});
  EXPECT_OK(sensor_fusion_impl.UpdateBleScanResult(
      kDeviceId, /*txPower=*/std::nullopt, kReachRssi,
      /*elapsed_real_time_millis=*/0));
  EXPECT_OK(sensor_fusion_impl.UpdateBleScanResult(
      kDeviceId, /*txPower=*/std::nullopt, kReachRssi,
      /*elapsed_real_time_millis=*/0));

  EXPECT_TRUE(proximity_zone_changed_called);
}

TEST(SensorFusion, GetRangingData) {
  SensorFusionImpl sensor_fusion_impl;
  bool proximity_zone_changed_called = false;
  sensor_fusion_impl.RequestZoneTransitionUpdates(
      {.on_proximity_zone_changed =
           [&proximity_zone_changed_called](
               uint64_t device_id,
               PresenceZone::DistanceBoundary::RangeType range_type) {
             proximity_zone_changed_called = true;
           }});
  EXPECT_OK(sensor_fusion_impl.UpdateBleScanResult(
      kDeviceId, /*txPower=*/std::nullopt, kReachRssi,
      /*elapsed_real_time_millis=*/0));
  EXPECT_OK(sensor_fusion_impl.UpdateBleScanResult(
      kDeviceId, /*txPower=*/std::nullopt, kReachRssi,
      /*elapsed_real_time_millis=*/0));

  EXPECT_TRUE(proximity_zone_changed_called);

  EXPECT_EQ(sensor_fusion_impl.GetRangingData(kDeviceId)
                ->zone_transition.value()
                .distance_range_type,
            PresenceZone::DistanceBoundary::RangeType::kWithinReach);
}
}  // namespace
}  // namespace presence
}  // namespace nearby
