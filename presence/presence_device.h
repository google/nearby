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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_DEVICE_H_
#define THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_DEVICE_H_

namespace nearby {
namespace presence {
class PresenceDevice {
 public:
  enum class MotionType {
    kPointAndHold = 0,
  };
  PresenceDevice(MotionType type = MotionType::kPointAndHold,
                 float confidence = 0) noexcept;
  MotionType GetMotionType() const;
  float GetConfidence() const;

 private:
  const MotionType motion_type_;
  const float confidence_;
};

inline bool operator==(const PresenceDevice& d1, const PresenceDevice& d2) {
  return d1.GetMotionType() == d2.GetMotionType() &&
         d1.GetConfidence() == d2.GetConfidence();
}

inline bool operator!=(const PresenceDevice& d1, const PresenceDevice& d2) {
  return !(d1 == d2);
}

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_DEVICE_H_
