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

#import <Foundation/Foundation.h>

#include <memory>

#include "connections/implementation/mediums/webrtc/webrtc_medium_impl.h"

namespace nearby {
namespace api {

std::unique_ptr<WebRtcMedium> WebRtcImplementationPlatform::CreateWebRtcMedium() {
  return std::make_unique<nearby::connections::mediums::WebRtcMediumImpl>();
}

std::string WebRtcImplementationPlatform::GetDefaultCountryCode() {
  NSString* countryCode = [NSLocale.currentLocale objectForKey:NSLocaleCountryCode];
  if (countryCode) {
    return std::string([countryCode UTF8String]);
  }
  return "US";
}

}  // namespace api
}  // namespace nearby
