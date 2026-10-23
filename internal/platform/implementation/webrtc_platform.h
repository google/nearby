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

#ifndef PLATFORM_API_WEBRTC_PLATFORM_H_
#define PLATFORM_API_WEBRTC_PLATFORM_H_

#include <memory>
#include <string>

#include "internal/platform/implementation/webrtc.h"

namespace nearby::api {

class WebRtcImplementationPlatform {
 public:
  static std::unique_ptr<WebRtcMedium> CreateWebRtcMedium();

  // Gets the default two-letter country code associated with current locale.
  // For example, en_US locale resolves to "US".
  // This follows the ISO 3166-1 Alpha-2 standard.
  static std::string GetDefaultCountryCode();
};

}  // namespace nearby::api

#endif  // PLATFORM_API_WEBRTC_PLATFORM_H_
