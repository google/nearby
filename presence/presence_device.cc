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

#include "presence/presence_device.h"

#include "internal/platform/implementation/system_clock.h"
#include "presence/device_motion.h"

namespace nearby {
namespace presence {

PresenceDevice::PresenceDevice(DeviceMetadata device_metadata) noexcept
    : discovery_timestamp_(location::nearby::SystemClock::ElapsedRealtime()),
      device_motion_(DeviceMotion()),
      device_metadata_(device_metadata) {}
PresenceDevice::PresenceDevice(DeviceMotion device_motion,
                               DeviceMetadata device_metadata) noexcept
    : discovery_timestamp_(location::nearby::SystemClock::ElapsedRealtime()),
      device_motion_(device_motion),
      device_metadata_(device_metadata) {}

}  // namespace presence
}  // namespace nearby
