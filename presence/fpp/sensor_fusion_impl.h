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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_FPP_SENSOR_FUSION_IMPL_H_
#define THIRD_PARTY_NEARBY_PRESENCE_FPP_SENSOR_FUSION_IMPL_H_

#include <cstdint>
#include <vector>

#include "presence/fpp/fpp_manager.h"
#include "presence/implementation/sensor_fusion.h"

namespace nearby {
namespace presence {

class SensorFusionImpl : public SensorFusion {
 public:
  ~SensorFusionImpl() = default;
  std::vector<DataSource> GetDataSources(
      uint64_t elapsed_realtime_millis,
      const std::vector<DataSource>& available_sources) override;
  absl::Status UpdateBleScanResult(uint64_t device_id,
                                   std::optional<int8_t> txPower, int rssi,
                                   uint64_t elapsed_realtime_millis) override;
  void UpdateUwbRangingResult(uint64_t device_id,
                              RangingPosition position) override;
  void RequestZoneTransitionUpdates(ZoneTransitionCallback callback) override;
  void RemoveZoneTransitionUpdates(uint64_t callback_id) override;
  void RequestDeviceMotionUpdates(DeviceMotionCallback callback) override;
  void RemoveDeviceMotionUpdates(DeviceMotionCallback callback) override;
  std::optional<RangingData> GetRangingData(uint64_t device_id) override;

 private:
  FppManager fpp_manager_;
  int id_generator_ = 0;
};
}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_FPP_SENSOR_FUSION_IMPL_H_
