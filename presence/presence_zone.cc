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

namespace nearby {
namespace presence {

PresenceZone::DistanceBoundary::DistanceBoundary(float min_distance_meters,
                                                 float max_distance_meters,
                                                 RangeType range_type) noexcept
    : min_distance_meters_(min_distance_meters),
      max_distance_meters_(max_distance_meters),
      range_type_(range_type) {}

float PresenceZone::DistanceBoundary::GetMinDistanceMeters() const {
  return min_distance_meters_;
}

float PresenceZone::DistanceBoundary::GetMaxDistanceMeters() const {
  return max_distance_meters_;
}

PresenceZone::DistanceBoundary::RangeType
PresenceZone::DistanceBoundary::GetRangeType() const {
  return range_type_;
}

PresenceZone::AngleOfArrivalBoundary::AngleOfArrivalBoundary(
    float min_angle_degrees, float max_angle_degrees) noexcept
    : min_angle_degrees_(min_angle_degrees),
      max_angle_degrees_(max_angle_degrees) {}

float PresenceZone::AngleOfArrivalBoundary::GetMinAngleDegrees() const {
  return min_angle_degrees_;
}

float PresenceZone::AngleOfArrivalBoundary::GetMaxAngleDegrees() const {
  return max_angle_degrees_;
}

PresenceZone::PresenceZone(
    const DistanceBoundary& distance_boundary,
    const AngleOfArrivalBoundary& azimuth_angle_boundary,
    const AngleOfArrivalBoundary& elevation_angle_boundary,
    const std::vector<DeviceMotion>& device_motions)
    : distance_boundary_(distance_boundary),
      azimuth_angle_boundary_(azimuth_angle_boundary),
      elevation_angle_boundary_(elevation_angle_boundary),
      device_motions_(device_motions) {}

PresenceZone::DistanceBoundary PresenceZone::GetDistanceBoundary() const {
  return distance_boundary_;
}

PresenceZone::AngleOfArrivalBoundary PresenceZone::GetAzimuthAngleBoundary()
    const {
  return azimuth_angle_boundary_;
}

PresenceZone::AngleOfArrivalBoundary PresenceZone::GetElevationAngleBoundary()
    const {
  return elevation_angle_boundary_;
}

std::vector<DeviceMotion> PresenceZone::GetLocalDeviceMotions() const {
  return device_motions_;
}

}  // namespace presence
}  // namespace nearby
