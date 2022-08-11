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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_ZONE_H_
#define THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_ZONE_H_

#include <vector>

#include "presence/device_motion.h"
namespace nearby {
namespace presence {
class PresenceZone {
 public:
  class DistanceBoundary {
   public:
    enum class RangeType {
      kRangeUnknown = 0,
      kFar,          // Distance is very far away from the peer device.
      kWithinReach,  // Distance is very close to the peer device, typically
                     // within one meter or less.
      kWithinTap,  // Distance is within tap range to the peer device, typically
                   // within ~0.127 meters.
    };
    DistanceBoundary(float min_distance_meters = 0,
                     float max_distance_meters = 0,
                     RangeType range_type = RangeType::kRangeUnknown) noexcept;
    float GetMinDistanceMeters() const;
    float GetMaxDistanceMeters() const;
    RangeType GetRangeType() const;

   private:
    const float min_distance_meters_;
    const float max_distance_meters_;
    const RangeType range_type_;
  };

  class AngleOfArrivalBoundary {
   public:
    AngleOfArrivalBoundary(float min_angle_degrees = 0,
                           float max_angle_degrees = 0) noexcept;
    float GetMinAngleDegrees() const;
    float GetMaxAngleDegrees() const;

   private:
    const float min_angle_degrees_;
    const float max_angle_degrees_;
  };

  PresenceZone(const DistanceBoundary& = {}, const AngleOfArrivalBoundary& = {},
               const AngleOfArrivalBoundary& = {},
               const std::vector<DeviceMotion>& = {});
  DistanceBoundary GetDistanceBoundary() const;
  AngleOfArrivalBoundary GetAzimuthAngleBoundary() const;
  AngleOfArrivalBoundary GetElevationAngleBoundary() const;
  std::vector<DeviceMotion> GetLocalDeviceMotions() const;

 private:
  const DistanceBoundary distance_boundary_;
  const AngleOfArrivalBoundary azimuth_angle_boundary_;
  const AngleOfArrivalBoundary elevation_angle_boundary_;
  const std::vector<DeviceMotion> device_motions_;
};

inline bool operator==(const PresenceZone::DistanceBoundary& d1,
                       const PresenceZone::DistanceBoundary& d2) {
  return d1.GetMinDistanceMeters() == d2.GetMinDistanceMeters() &&
         d1.GetMaxDistanceMeters() == d2.GetMaxDistanceMeters() &&
         d1.GetRangeType() == d2.GetRangeType();
}
inline bool operator!=(const PresenceZone::DistanceBoundary& d1,
                       const PresenceZone::DistanceBoundary& d2) {
  return !(d1 == d2);
}
inline bool operator==(const PresenceZone::AngleOfArrivalBoundary& a1,
                       const PresenceZone::AngleOfArrivalBoundary& a2) {
  return a1.GetMinAngleDegrees() == a2.GetMinAngleDegrees() &&
         a1.GetMaxAngleDegrees() == a2.GetMaxAngleDegrees();
}
inline bool operator!=(const PresenceZone::AngleOfArrivalBoundary& a1,
                       const PresenceZone::AngleOfArrivalBoundary& a2) {
  return !(a1 == a2);
}

inline bool operator==(const PresenceZone& z1, const PresenceZone& z2) {
  return z1.GetDistanceBoundary() == z2.GetDistanceBoundary() &&
         z1.GetAzimuthAngleBoundary() == z2.GetAzimuthAngleBoundary() &&
         z1.GetElevationAngleBoundary() == z2.GetElevationAngleBoundary() &&
         z1.GetLocalDeviceMotions() == z2.GetLocalDeviceMotions();
}
inline bool operator!=(const PresenceZone& z1, const PresenceZone& z2) {
  return !(z1 == z2);
}

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_ZONE_H_
