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

#ifndef CORE_POWER_LEVEL_H_
#define CORE_POWER_LEVEL_H_

namespace nearby {
namespace connections {  // Represents the various power levels that can be
                         // used, on mediums that support it.
enum class PowerLevel {
  kHighPower = 0,
  kLowPower = 1,
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_POWER_LEVEL_H_
