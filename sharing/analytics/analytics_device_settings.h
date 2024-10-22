// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_ANALYTICS_ANALYTICS_DEVICE_SETTINGS_H_
#define THIRD_PARTY_NEARBY_SHARING_ANALYTICS_ANALYTICS_DEVICE_SETTINGS_H_

#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/enums.pb.h"

namespace nearby {
namespace sharing {
namespace analytics {

struct AnalyticsDeviceSettings {
  bool is_fast_init_notification_enabled;
  int device_name_size;
  ::nearby::sharing::proto::DataUsage data_usage;
  ::nearby::sharing::proto::DeviceVisibility visibility;
};

}  // namespace analytics
}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_ANALYTICS_ANALYTICS_DEVICE_SETTINGS_H_
