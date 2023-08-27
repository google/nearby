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

#include "presence/fpp/fpp_manager.h"

#include <cstdint>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "presence/implementation/sensor_fusion.h"

namespace nearby {
namespace presence {
namespace {
constexpr uint64_t kDeviceId = 1234;
constexpr int kReachRssi = -40;
constexpr int kShortRangeRssi = -60;
constexpr int kCallbackId = 12345;

TEST(FppManager, UpdateBleScanResultSuccess) {
  FppManager manager;
  bool callback_called = false;
  manager.RegisterZoneTransitionListener(
      kCallbackId,
      {.on_proximity_zone_changed =
           [&callback_called](
               uint64_t device_id,
               PresenceZone::DistanceBoundary::RangeType range_type) {
             callback_called = true;
           }});
  EXPECT_OK(manager.UpdateBleScanResult(kDeviceId, /*txPower=*/std::nullopt,
                                        kReachRssi,
                                        /*elapsed_real_time_millis=*/0));
  // State is only computed after second consecutive scan is fulfilled
  EXPECT_OK(manager.UpdateBleScanResult(kDeviceId, /*txPower=*/std::nullopt,
                                        kReachRssi,
                                        /*elapsed_real_time_millis=*/2000));
  EXPECT_EQ(manager.GetRangingData(kDeviceId)
                ->zone_transition.value()
                .distance_range_type,
            PresenceZone::DistanceBoundary::RangeType::kWithinReach);
  EXPECT_TRUE(callback_called);
}

TEST(FppManager, ZoneTransitionDetected) {
  FppManager manager;
  bool callback_called = false;
  manager.RegisterZoneTransitionListener(
      kCallbackId,
      {.on_proximity_zone_changed =
           [&callback_called](
               uint64_t device_id,
               PresenceZone::DistanceBoundary::RangeType range_type) {
             callback_called = true;
           }});
  // ProximityEstimate is only computed after consecutive scans is fulfilled
  EXPECT_OK(manager.UpdateBleScanResult(kDeviceId, /*txPower=*/std::nullopt,
                                        kReachRssi,
                                        /*elapsed_real_time_millis=*/0));
  EXPECT_OK(manager.UpdateBleScanResult(kDeviceId, /*txPower=*/std::nullopt,
                                        kReachRssi,
                                        /*elapsed_real_time_millis=*/2000));
  EXPECT_EQ(manager.GetRangingData(kDeviceId)
                ->zone_transition.value()
                .distance_range_type,
            PresenceZone::DistanceBoundary::RangeType::kWithinReach);
  EXPECT_TRUE(callback_called);
  callback_called = false;

  // Update with new zone
  EXPECT_OK(manager.UpdateBleScanResult(kDeviceId, /*txPower=*/std::nullopt,
                                        kShortRangeRssi,
                                        /*elapsed_real_time_millis=*/0));
  EXPECT_EQ(manager.GetRangingData(kDeviceId)
                ->zone_transition.value()
                .distance_range_type,
            PresenceZone::DistanceBoundary::RangeType::kWithinReach);
  EXPECT_FALSE(callback_called);
  // Update with consecutive scan of new zone
  EXPECT_OK(manager.UpdateBleScanResult(kDeviceId, /*txPower=*/std::nullopt,
                                        kShortRangeRssi,
                                        /*elapsed_real_time_millis=*/0));
  EXPECT_EQ(manager.GetRangingData(kDeviceId)
                ->zone_transition.value()
                .distance_range_type,
            PresenceZone::DistanceBoundary::RangeType::kFar);
  EXPECT_TRUE(callback_called);
}

TEST(FppManager, ConvertProximityEstimateToRangingData) {
  FppManager manager;
  ProximityEstimate proximity_estimate =
      ProximityEstimate{kDeviceId,
                        0.1,
                        MeasurementConfidence::Low,
                        0,
                        ProximityState::Reach,
                        PresenceDataSource::Ble};
  RangingData rangingData =
      manager.ConvertProximityEstimateToRangingData(proximity_estimate);
  EXPECT_EQ(rangingData.data_source, DataSource::kBle);
  EXPECT_EQ(rangingData.position.distance.value, 0.1f);
  EXPECT_EQ(rangingData.zone_transition->confidence_level, 0.0f);
  EXPECT_EQ(rangingData.zone_transition->distance_range_type,
            PresenceZone::DistanceBoundary::RangeType::kWithinReach);
  ProximityEstimate unknown_proximity_estimate =
      ProximityEstimate{kDeviceId,
                        0.0,
                        MeasurementConfidence::Low,
                        0,
                        ProximityState::Unknown,
                        PresenceDataSource::Ble};
  RangingData unknown_rangingData =
      manager.ConvertProximityEstimateToRangingData(unknown_proximity_estimate);
  EXPECT_EQ(unknown_rangingData.zone_transition->distance_range_type,
            PresenceZone::DistanceBoundary::RangeType::kRangeUnknown);
  ProximityEstimate tap_proximity_estimate =
      ProximityEstimate{kDeviceId,
                        0.03,
                        MeasurementConfidence::Low,
                        0,
                        ProximityState::Tap,
                        PresenceDataSource::Ble};
  RangingData tap_rangingData =
      manager.ConvertProximityEstimateToRangingData(tap_proximity_estimate);
  EXPECT_EQ(tap_rangingData.zone_transition->distance_range_type,
            PresenceZone::DistanceBoundary::RangeType::kWithinTap);
}

TEST(FppManager, UpdateBleScanResultWithTxPowerSuccess) {
  FppManager manager;
  bool callback_called = false;
  manager.RegisterZoneTransitionListener(
      kCallbackId,
      {.on_proximity_zone_changed =
           [&callback_called](
               uint64_t device_id,
               PresenceZone::DistanceBoundary::RangeType range_type) {
             callback_called = true;
           }});
  EXPECT_OK(manager.UpdateBleScanResult(kDeviceId, /*txPower=*/20, kReachRssi,
                                        /*elapsed_real_time_millis=*/0));
  EXPECT_OK(manager.UpdateBleScanResult(kDeviceId, /*txPower=*/20, kReachRssi,
                                        /*elapsed_real_time_millis=*/2000));
  EXPECT_EQ(manager.GetRangingData(kDeviceId)
                ->zone_transition.value()
                .distance_range_type,
            PresenceZone::DistanceBoundary::RangeType::kWithinTap);
  EXPECT_TRUE(callback_called);
}

TEST(FppManager, UnregisterZoneTransitionListener) {
  FppManager manager;
  bool callback_called = false;
  manager.RegisterZoneTransitionListener(
      kCallbackId,
      {.on_proximity_zone_changed =
           [&callback_called](
               uint64_t device_id,
               PresenceZone::DistanceBoundary::RangeType range_type) {
             callback_called = true;
           }});
  EXPECT_OK(manager.UpdateBleScanResult(kDeviceId, /*txPower=*/std::nullopt,
                                        kReachRssi,
                                        /*elapsed_real_time_millis=*/0));
  EXPECT_OK(manager.UpdateBleScanResult(kDeviceId, /*txPower=*/std::nullopt,
                                        kReachRssi,
                                        /*elapsed_real_time_millis=*/2000));
  EXPECT_TRUE(callback_called);
  callback_called = false;

  // Unregister listener and update with new zone
  manager.UnregisterZoneTransitionListener(kCallbackId);
  EXPECT_EQ(manager
                .UpdateBleScanResult(kDeviceId, /*txPower=*/std::nullopt,
                                     kShortRangeRssi,
                                     /*elapsed_real_time_millis=*/0)
                .code(),
            absl::StatusCode::kInternal);
  EXPECT_EQ(manager
                .UpdateBleScanResult(kDeviceId, /*txPower=*/std::nullopt,
                                     kShortRangeRssi,
                                     /*elapsed_real_time_millis=*/0)
                .code(),
            absl::StatusCode::kInternal);
  EXPECT_FALSE(callback_called);
}

TEST(FppManager, ResetProximityStateData) {
  FppManager manager;
  bool callback_called = false;
  manager.RegisterZoneTransitionListener(
      kCallbackId,
      {.on_proximity_zone_changed =
           [&callback_called](
               uint64_t device_id,
               PresenceZone::DistanceBoundary::RangeType range_type) {
             callback_called = true;
           }});
  ASSERT_OK(manager.UpdateBleScanResult(kDeviceId, /*txPower=*/std::nullopt,
                                        kReachRssi,
                                        /*elapsed_real_time_millis=*/0));
  // State is only computed after second consecutive scan is fulfilled
  ASSERT_OK(manager.UpdateBleScanResult(kDeviceId, /*txPower=*/std::nullopt,
                                        kReachRssi,
                                        /*elapsed_real_time_millis=*/2000));
  EXPECT_EQ(manager.GetRangingData(kDeviceId)
                ->zone_transition.value()
                .distance_range_type,
            PresenceZone::DistanceBoundary::RangeType::kWithinReach);
  EXPECT_TRUE(callback_called);

  // Reset proximity state data
  manager.ResetProximityStateData();
  EXPECT_EQ(manager.GetRangingData(kDeviceId)
                ->zone_transition.value()
                .distance_range_type,
            PresenceZone::DistanceBoundary::RangeType::kRangeUnknown);
}

TEST(FppManager, GetStatusStringFromCode) {
  FppManager manager;
  EXPECT_EQ(manager.GetStatusStringFromCode(101),
            "INVALID_PRESENCE_DETECTOR_HANDLE");
  EXPECT_EQ(manager.GetStatusStringFromCode(102), "NULL_OUTPUT_PARAMETER");
}

}  // namespace
}  // namespace presence
}  // namespace nearby
