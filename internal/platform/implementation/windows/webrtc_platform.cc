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

#include <winnls.h>

#include <memory>
#include <string>

#include "internal/platform/logging.h"
#include "internal/platform/implementation/webrtc.h"
#include "connections/implementation/mediums/webrtc/webrtc_medium_impl.h"

namespace nearby::api {

std::unique_ptr<WebRtcMedium>
WebRtcImplementationPlatform::CreateWebRtcMedium() {
  return std::make_unique<nearby::connections::mediums::WebRtcMediumImpl>();
}

std::string WebRtcImplementationPlatform::GetDefaultCountryCode() {
  wchar_t systemGeoName[LOCALE_NAME_MAX_LENGTH];

  if (!GetUserDefaultGeoName(systemGeoName, LOCALE_NAME_MAX_LENGTH)) {
    LOG(ERROR) << __func__
               << ": Failed to GetUserDefaultGeoName: " << ". Fall back to US.";
    return "US";
  }
  std::wstring wideGeo(systemGeoName);
  std::string systemGeoNameString(wideGeo.begin(), wideGeo.end());
  VLOG(1) << "GetUserDefaultGeoName() returns: " << systemGeoNameString;
  return systemGeoNameString;
}

}  // namespace nearby::api
