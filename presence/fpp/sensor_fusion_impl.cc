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

#include "presence/fpp/sensor_fusion_impl.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "absl/status/status.h"
#include "presence/fpp/fpp_manager.h"

namespace nearby {
namespace presence {
std::vector<DataSource> SensorFusionImpl::GetDataSources(
    uint64_t elapsed_realtime_millis,
    const std::vector<DataSource>& available_sources) {
  // TODO(b/264547688) - Implement
  return std::vector<DataSource>();
}
absl::Status SensorFusionImpl::UpdateBleScanResult(
    uint64_t device_id, std::optional<int8_t> txPower, int rssi,
    uint64_t elapsed_realtime_millis) {
  return fpp_manager_.UpdateBleScanResult(device_id, txPower, rssi,
                                          elapsed_realtime_millis);
}
void SensorFusionImpl::UpdateUwbRangingResult(uint64_t device_id,
                                              RangingPosition position) {
  // TODO(b/264547688) - Implement
}

void SensorFusionImpl::RequestZoneTransitionUpdates(
    ZoneTransitionCallback callback) {
  int callback_id = ++id_generator_;
  callback.on_callback_id_generated(callback_id);
  fpp_manager_.RegisterZoneTransitionListener(callback_id, std::move(callback));
}

void SensorFusionImpl::RequestDeviceMotionUpdates(
    SensorFusion::DeviceMotionCallback callback) {
  // TODO(b/264547688) - Implement
}
void SensorFusionImpl::RemoveDeviceMotionUpdates(
    SensorFusion::DeviceMotionCallback callback) {
  // TODO(b/264547688) - Implement
}

void SensorFusionImpl::RemoveZoneTransitionUpdates(uint64_t callback_id) {
  fpp_manager_.UnregisterZoneTransitionListener(callback_id);
}

std::optional<RangingData> SensorFusionImpl::GetRangingData(
    uint64_t device_id) {
  return fpp_manager_.GetRangingData(device_id);
}
}  // namespace presence
}  // namespace nearby
