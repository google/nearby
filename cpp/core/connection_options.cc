// Copyright 2021 Google LLC
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

#include "core/connection_options.h"

#include "core/strategy.h"
#include "platform/base/byte_array.h"
#include "platform/base/core_config.h"

namespace location {
namespace nearby {
namespace connections {

std::vector<Medium> ConnectionOptions::GetMediums() const {
  return allowed.GetMediums(true);
}

// This call follows the standard Microsoft calling pattern of calling first
// to get the size of the array. Caller then allocates memory for the array,
// and makes this call again to copy the array into the provided location.
void ConnectionOptions::GetMediums(
    location::nearby::proto::connections::Medium* mediums,
    uint32_t* mediumsSize) {
  auto size = GetMediums().size();

  // Caller is seeking the size of mediums
  if (mediums == nullptr) {
    *mediumsSize = size;
    return;
  }

  // Caller has not specified enough memory
  if (*mediumsSize < size) {
    mediums = nullptr;    // ensure nullptr return
    *mediumsSize = size;  // update the size to indicate the correct size
    return;
  }

  for (uint32_t i = 0; i < size; i++) {
    // Construct an array at the given location
    mediums[i] = GetMediums().at(i);
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
