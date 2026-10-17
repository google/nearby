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
#include "location/nearby/cpp/sharing/clients/cpp/common/nearby_sharing_common.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include "proto/sharing_enums.pb.h"

namespace nearby::sharing::cpp::common {

using ::location::nearby::proto::sharing::PowerStatus;

PowerStatus GetPowerStatus() {
#if defined(_WIN32)
  SYSTEM_POWER_STATUS status;
  if (!GetSystemPowerStatus(&status) || status.ACLineStatus == 255) {
    return PowerStatus::POWER_STATUS_UNKNOWN;
  }
  bool has_battery = (status.BatteryFlag & 128) == 0;
  if (status.ACLineStatus == 1) {
    return has_battery ? PowerStatus::POWER_STATUS_BATTERY_AND_AC
                       : PowerStatus::POWER_STATUS_AC;
  }
  return PowerStatus::POWER_STATUS_BATTERY;
#else
  return PowerStatus::POWER_STATUS_UNKNOWN;
#endif
}

}  // namespace nearby::sharing::cpp::common
