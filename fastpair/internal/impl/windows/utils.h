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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_IMPL_WINDOWS_UTILS_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_IMPL_WINDOWS_UTILS_H_

#include <guiddef.h>
#include <stdio.h>

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {

std::string uint64_to_mac_address_string(uint64_t bluetoothAddress);
uint64_t mac_address_string_to_uint64(absl::string_view mac_address);

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_IMPL_WINDOWS_UTILS_H_
