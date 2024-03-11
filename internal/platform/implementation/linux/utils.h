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

#ifndef PLATFORM_IMPL_LINUX_UTILS_H_
#define PLATFORM_IMPL_LINUX_UTILS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace linux {

std::optional<Uuid> UuidFromString(const std::string &uuid_str);
std::optional<std::string> NewUuidStr();

std::string RandSSID();
std::string RandWPAPassphrase();

}  // namespace linux
}  // namespace nearby

#endif  //  PLATFORM_IMPL_LINUX_UTILS_H_
