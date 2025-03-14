// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_ADVERTISEMENTS_ADVERTISEMENT_UTIL_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_ADVERTISEMENTS_ADVERTISEMENT_UTIL_H_

#include <optional>
#include <string>

#include "internal/platform/byte_array.h"

namespace nearby::connections::advertisements {

// Reads the device name from the endpoint info.
std::optional<std::string> ReadDeviceName(const ByteArray& endpoint_info);

// Build the endpoint info from the device name.
std::string BuildEndpointInfo(const std::string& device_name);

}  // namespace nearby::connections::advertisements

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MEDIUMS_ADVERTISEMENTS_ADVERTISEMENT_UTIL_H_
