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

#ifndef CORE_INTERNAL_MEDIUMS_UTILS_H_
#define CORE_INTERNAL_MEDIUMS_UTILS_H_

#include <cstddef>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {

class Utils {
 public:
  static ByteArray GenerateRandomBytes(size_t length);
  static ByteArray Sha256Hash(const ByteArray& source, size_t length);
  static ByteArray Sha256Hash(const std::string& source, size_t length);
  static std::string GenerateSalt();
  static std::string GenerateSalt(size_t length);

  // Derives the service type Nearby advertises and discovers with for
  // |service_id|: the upper-case hex of the first
  // NsdServiceInfo::kTypeFromServiceIdHashLength bytes of SHA-256(service_id),
  // in NsdServiceInfo::kNsdTypeFormat (e.g. "_559AEAD08264._tcp." for "A").
  // Hashing keeps the service ID itself off the air and turns any ID into a
  // valid DNS-SD label.
  //
  // This is the single definition of that derivation. Wi-Fi LAN, AWDL and
  // Wi-Fi Aware all build their mDNS service types / Wi-Fi Aware service names
  // from it, so nothing client specific is ever hardcoded in Nearby
  // Connections and every platform arrives at the same type for the same
  // service ID (Android derives it identically, see WifiLan#toNsdServiceType).
  // iOS clients must declare the derived types in their Info.plist ahead of
  // time (NSBonjourServices for Wi-Fi LAN and AWDL, WiFiAwareServices for
  // Wi-Fi Aware).
  static std::string GenerateServiceType(absl::string_view service_id);

  // Returns the Wi-Fi Aware service name Nearby publishes and subscribes to
  // for |service_id|: GenerateServiceType() of the service ID with an
  // "_AWARE" suffix (so it never collides with the Wi-Fi LAN service type),
  // without the trailing dot (e.g. "_93D474A2A379._tcp" for "A").
  //
  // Lives here rather than in WifiAware so that platform-independent code
  // (e.g. BasePcpHandler) can use it on platforms where the Wi-Fi Aware
  // medium is stubbed out (Windows).
  static std::string GenerateWifiAwareServiceName(absl::string_view service_id);
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_UTILS_H_
