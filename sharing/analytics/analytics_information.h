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

#ifndef THIRD_PARTY_NEARBY_SHARING_ANALYTICS_ANALYTICS_INFORMATION_H_
#define THIRD_PARTY_NEARBY_SHARING_ANALYTICS_ANALYTICS_INFORMATION_H_

namespace nearby {
namespace sharing {
namespace analytics {

enum class SendSurfaceState : int {
  // Default, invalid state.
  kUnknown = -1,
  // Background share sheet only listens to transfer update.
  kBackground = 0,
  // Foreground share sheet both scans and listens to transfer update.
  kForeground = 1,
  // Direct share service only scans and listens to share target onFound and
  // onLost.
  kDirectShareService = 2,
  // Foreground share sheet both scans and listens to transfer update, but does
  // not trigger FastInit HUN.
  kForegroundRetry = 3,
  // A foreground surface that is registered from 2nd and 3rd party apps.
  kExternal = 4,
};

struct AnalyticsInformation {
  SendSurfaceState send_surface_state;
};

}  // namespace analytics
}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_ANALYTICS_ANALYTICS_INFORMATION_H_
