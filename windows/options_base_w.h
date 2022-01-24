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
#ifndef WINDOWS_OPTIONS_BASE_W_H_
#define WINDOWS_OPTIONS_BASE_W_H_
#include <string>

#include "core/strategy.h"
#include "third_party/nearby/windows/medium_selector_w.h"

namespace location {
namespace nearby {
namespace windows {
// Feature On/Off switch for mediums.
using BooleanMediumSelector = MediumSelectorW<bool>;

// Connection Options: used for both Advertising and Discovery.
// All fields are mutable, to make the type copy-assignable.
struct OptionsBaseW {
  location::nearby::connections::Strategy strategy;
  BooleanMediumSelector allowed{BooleanMediumSelector().SetAll(true)};
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // WINDOWS_OPTIONS_BASE_W_H_
