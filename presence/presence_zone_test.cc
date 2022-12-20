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

#include "presence/presence_zone.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "presence/device_motion.h"

namespace nearby {
namespace presence {
namespace {

using DistanceBoundary = nearby::presence::PresenceZone::DistanceBoundary;
using RangeType = nearby::presence::PresenceZone::DistanceBoundary::RangeType;
using AngleOfArrivalBoundary =
    nearby::presence::PresenceZone::AngleOfArrivalBoundary;

static const float kDefaultDistanceMeters = 0;
static const float kTestMinDistanceMeters = 1;
static const float kTestMaxDistanceMeters = 2;

static const float kDefaultDegrees = 0;
static const float kTestMinAngleDegrees = 10;
static const float kTestMaxAngleDegrees = 20;

static const float kTestConfidence = 0.1;

static const RangeType kDefaultRangeType = RangeType::kRangeUnknown;
static const RangeType kTestRangeType = RangeType::kFar;

static const DistanceBoundary kDefaultDistanceBoundary;
static const DistanceBoundary kTestDistanceBoundary = {
    kTestMinDistanceMeters, kTestMaxDistanceMeters, kTestRangeType};
static const AngleOfArrivalBoundary kDefaultAngleBoundary;
static const AngleOfArrivalBoundary kTestAzimuthAngleBoundary = {
    kTestMinAngleDegrees, kTestMaxAngleDegrees};
static const AngleOfArrivalBoundary kTestElevationAngleBoundary = {
    kTestMinAngleDegrees, kTestMaxAngleDegrees};
static const DeviceMotion kTestDeviceMotion = {
    DeviceMotion::MotionType::kPointAndHold, kTestConfidence};

TEST(DistanceBoundaryTest, DefaultConstructorWorks) {
  DistanceBoundary boundary;
  EXPECT_EQ(boundary.GetMinDistanceMeters(), kDefaultDistanceMeters);
  EXPECT_EQ(boundary.GetMaxDistanceMeters(), kDefaultDistanceMeters);
  EXPECT_EQ(boundary.GetRangeType(), kDefaultRangeType);
}

TEST(DistanceBoundaryTest, DefaultEquals) {
  DistanceBoundary boundary1;
  DistanceBoundary boundary2;
  EXPECT_EQ(boundary1, boundary2);
}

TEST(DistanceBoundaryTest, PartiallyInitializationWorks) {
  DistanceBoundary boundary1 = {kTestMinDistanceMeters, kTestMaxDistanceMeters};
  DistanceBoundary boundary2 = {kTestMinDistanceMeters};
  EXPECT_EQ(boundary1.GetMinDistanceMeters(), kTestMinDistanceMeters);
  EXPECT_EQ(boundary1.GetMaxDistanceMeters(), kTestMaxDistanceMeters);
  EXPECT_EQ(boundary1.GetRangeType(), kDefaultRangeType);
  EXPECT_EQ(boundary2.GetMinDistanceMeters(), kTestMinDistanceMeters);
  EXPECT_EQ(boundary2.GetMaxDistanceMeters(), kDefaultDistanceMeters);
  EXPECT_EQ(boundary2.GetRangeType(), kDefaultRangeType);
}

TEST(DistanceBoundaryTest, ExplicitInitEquals) {
  DistanceBoundary boundary1 = {kTestMinDistanceMeters, kTestMaxDistanceMeters,
                                kTestRangeType};
  DistanceBoundary boundary2 = {kTestMinDistanceMeters, kTestMaxDistanceMeters,
                                kTestRangeType};
  EXPECT_EQ(boundary1.GetMinDistanceMeters(), kTestMinDistanceMeters);
  EXPECT_EQ(boundary1.GetMaxDistanceMeters(), kTestMaxDistanceMeters);
  EXPECT_EQ(boundary1.GetRangeType(), kTestRangeType);
  EXPECT_EQ(boundary1, boundary2);
}

TEST(DistanceBoundaryTest, ExplicitInitNotEquals) {
  DistanceBoundary boundary1 = {kTestMinDistanceMeters, kTestMaxDistanceMeters,
                                kTestRangeType};
  DistanceBoundary boundary2 = {kTestMinDistanceMeters + 0.1f,
                                kTestMaxDistanceMeters, kTestRangeType};
  EXPECT_NE(boundary1, boundary2);
}

TEST(DistanceBoundaryTest, CopyInitEquals) {
  DistanceBoundary boundary1 = {kTestMinDistanceMeters, kTestMaxDistanceMeters,
                                kTestRangeType};
  DistanceBoundary boundary2 = {boundary1};
  EXPECT_EQ(boundary1, boundary2);
}

TEST(AngleOfArrivalBoundaryTest, DefaultConstructorWorks) {
  AngleOfArrivalBoundary aoa_boundary;
  EXPECT_EQ(aoa_boundary.GetMinAngleDegrees(), kDefaultDegrees);
  EXPECT_EQ(aoa_boundary.GetMaxAngleDegrees(), kDefaultDegrees);
}

TEST(AngleOfArrivalBoundaryTest, DefaultEquals) {
  AngleOfArrivalBoundary aoa_boundary1;
  AngleOfArrivalBoundary aoa_boundary2;
  EXPECT_EQ(aoa_boundary1, aoa_boundary2);
}

TEST(AngleOfArrivalBoundaryTest, PartiallyInitializationWorks) {
  AngleOfArrivalBoundary aoa_boundary = {kTestMinAngleDegrees};
  EXPECT_EQ(aoa_boundary.GetMinAngleDegrees(), kTestMinAngleDegrees);
  EXPECT_EQ(aoa_boundary.GetMaxAngleDegrees(), kDefaultDegrees);
}

TEST(AngleOfArrivalBoundaryTest, ExplicitInitEquals) {
  AngleOfArrivalBoundary aoa_boundary1 = {kTestMinAngleDegrees,
                                          kTestMaxAngleDegrees};
  AngleOfArrivalBoundary aoa_boundary2 = {kTestMinAngleDegrees,
                                          kTestMaxAngleDegrees};
  EXPECT_EQ(aoa_boundary1.GetMinAngleDegrees(), kTestMinAngleDegrees);
  EXPECT_EQ(aoa_boundary1.GetMaxAngleDegrees(), kTestMaxAngleDegrees);
  EXPECT_EQ(aoa_boundary1, aoa_boundary2);
}

TEST(AngleOfArrivalBoundaryTest, ExplicitInitNotEquals) {
  AngleOfArrivalBoundary aoa_boundary1 = {kTestMinAngleDegrees,
                                          kTestMaxAngleDegrees};
  AngleOfArrivalBoundary aoa_boundary2 = {kTestMinAngleDegrees,
                                          kTestMaxAngleDegrees + 0.1f};
  EXPECT_NE(aoa_boundary1, aoa_boundary2);
}

TEST(AngleOfArrivalBoundaryTest, CopyInitEquals) {
  AngleOfArrivalBoundary aoa_boundary1 = {kTestMinAngleDegrees,
                                          kTestMaxAngleDegrees};
  AngleOfArrivalBoundary aoa_boundary2 = {aoa_boundary1};
  EXPECT_EQ(aoa_boundary1, aoa_boundary2);
}

TEST(PresenceZoneTest, DefaultConstructorWorks) {
  PresenceZone zone;
  EXPECT_EQ(zone.GetDistanceBoundary(), kDefaultDistanceBoundary);
  EXPECT_EQ(zone.GetAzimuthAngleBoundary(), kDefaultAngleBoundary);
  EXPECT_EQ(zone.GetElevationAngleBoundary(), kDefaultAngleBoundary);
  EXPECT_EQ(zone.GetLocalDeviceMotions().capacity(), 0);
}

TEST(PresenceZoneTest, DefaultEquals) {
  PresenceZone zone1;
  PresenceZone zone2;
  EXPECT_EQ(zone1, zone2);
}

TEST(PresenceZoneTest, PartiallyInitializationWorks) {
  PresenceZone zone1 = {kTestDistanceBoundary, kTestAzimuthAngleBoundary,
                        kTestElevationAngleBoundary};
  PresenceZone zone2 = {kTestDistanceBoundary, kTestAzimuthAngleBoundary};
  PresenceZone zone3 = {kTestDistanceBoundary};
  EXPECT_EQ(zone1.GetDistanceBoundary(), kTestDistanceBoundary);
  EXPECT_EQ(zone1.GetAzimuthAngleBoundary(), kTestAzimuthAngleBoundary);
  EXPECT_EQ(zone1.GetElevationAngleBoundary(), kTestElevationAngleBoundary);
  EXPECT_EQ(zone1.GetLocalDeviceMotions().capacity(), 0);
  EXPECT_EQ(zone2.GetDistanceBoundary(), kTestDistanceBoundary);
  EXPECT_EQ(zone2.GetAzimuthAngleBoundary(), kTestAzimuthAngleBoundary);
  EXPECT_EQ(zone2.GetElevationAngleBoundary(), kDefaultAngleBoundary);
  EXPECT_EQ(zone2.GetLocalDeviceMotions().capacity(), 0);
  EXPECT_EQ(zone3.GetDistanceBoundary(), kTestDistanceBoundary);
  EXPECT_EQ(zone3.GetAzimuthAngleBoundary(), kDefaultAngleBoundary);
  EXPECT_EQ(zone3.GetElevationAngleBoundary(), kDefaultAngleBoundary);
  EXPECT_EQ(zone3.GetLocalDeviceMotions().capacity(), 0);
}

TEST(PresenceZoneTest, ExplicitInitEquals) {
  PresenceZone zone1 = {kTestDistanceBoundary,
                        kTestAzimuthAngleBoundary,
                        kTestElevationAngleBoundary,
                        {kTestDeviceMotion}};
  PresenceZone zone2 = {kTestDistanceBoundary,
                        kTestAzimuthAngleBoundary,
                        kTestElevationAngleBoundary,
                        {kTestDeviceMotion}};
  EXPECT_EQ(zone1.GetDistanceBoundary(), kTestDistanceBoundary);
  EXPECT_EQ(zone1.GetAzimuthAngleBoundary(), kTestAzimuthAngleBoundary);
  EXPECT_EQ(zone1.GetElevationAngleBoundary(), kTestElevationAngleBoundary);
  EXPECT_EQ(zone1.GetLocalDeviceMotions().size(), 1);
  EXPECT_EQ(zone1.GetLocalDeviceMotions()[0], kTestDeviceMotion);
  EXPECT_EQ(zone1, zone2);
}

TEST(PresenceZoneTest, ExplicitInitNotEquals) {
  PresenceZone zone1 = {kTestDistanceBoundary,
                        kTestAzimuthAngleBoundary,
                        kTestElevationAngleBoundary,
                        {kTestDeviceMotion}};
  PresenceZone zone2 = {kTestDistanceBoundary,
                        kTestAzimuthAngleBoundary,
                        kTestElevationAngleBoundary,
                        {}};
  EXPECT_NE(zone1, zone2);
}

TEST(PresenceZoneTest, CopyInitEquals) {
  PresenceZone zone1 = {kTestDistanceBoundary,
                        kTestAzimuthAngleBoundary,
                        kTestElevationAngleBoundary,
                        {kTestDeviceMotion}};
  PresenceZone zone2 = {zone1};
  EXPECT_EQ(zone1, zone2);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
