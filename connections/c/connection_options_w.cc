// Copyright 2021-2022 Google LLC
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

#include "connections/c/connection_options_w.h"

#include <string>

namespace nearby::windows {

void ConnectionOptionsW::GetMediums(const MediumW* mediums,
                                    size_t mediums_size) const {
  // Create a collection of enabled mediums
  auto allowedMediums = allowed.GetMediums(true);
  auto iter = allowedMediums.begin();
  int index = 0;
  // There is a fixed buffer of 5 for these, fill it up and leave.
  while (iter != allowedMediums.end() && index < MAX_MEDIUMS) {
    *mediums_[index++] = iter[index];
  }
  mediums_size = allowed.GetMediums(true).size();
  return;
}

}  // namespace nearby::windows
