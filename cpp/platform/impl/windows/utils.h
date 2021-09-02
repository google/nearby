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

#ifndef PLATFORM_IMPL_WINDOWS_UTILS_H_
#define PLATFORM_IMPL_WINDOWS_UTILS_H_

#include <windows.h>
#include <stdio.h>

#include <string>

namespace location {
namespace nearby {
namespace windows {

std::string uint64_to_mac_address_string(uint64_t bluetoothAddress);

namespace Constants {
// The Id of the Service Name SDP attribute
const uint16_t SdpServiceNameAttributeId = 0x100;

// The SDP Type of the Service Name SDP attribute.
// The first byte in the SDP Attribute encodes the SDP Attribute Type as
// follows:
//    -  the Attribute Type size in the least significant 3 bits,
//    -  the SDP Attribute Type value in the most significant 5 bits.
const char SdpServiceNameAttributeType = (4 << 3) | 5;
}  // namespace Constants

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  //  PLATFORM_IMPL_WINDOWS_UTILS_H_
