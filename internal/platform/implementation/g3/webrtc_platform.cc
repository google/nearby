// Copyright 2026 Google LLC
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

#include "internal/platform/implementation/webrtc_platform.h"

#include <memory>
#include <string>

#include "internal/platform/implementation/g3/webrtc.h"
#include "internal/platform/implementation/webrtc.h"
#include "internal/platform/medium_environment.h"

namespace nearby::api {

std::unique_ptr<WebRtcMedium>
WebRtcImplementationPlatform::CreateWebRtcMedium() {
  if (MediumEnvironment::Instance().GetEnvironmentConfig().webrtc_enabled) {
    return std::make_unique<g3::WebRtcMedium>();
  } else {
    return nullptr;
  }
}

std::string WebRtcImplementationPlatform::GetDefaultCountryCode() {
  return "US";
}

}  // namespace nearby::api
